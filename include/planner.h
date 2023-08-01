#ifndef SQL_PLANNER_H
#define SQL_PLANNER_H
#include <__generator.hpp>
#include "common.h"
#include "parser/parser.h"
#include "parser/preproc.h"
#include "operator/selector.h"
#include "operator/projector.h"
#include "operator/join.h"

namespace ctsql {
namespace impl {
    // CREDIT: https://stackoverflow.com/questions/7110301/generic-hash-for-tuples-in-unordered-map-unordered-set
    // usage be like: unordered_set<tuple<double, int>, hash_tuple::hash<tuple<double, int>>> dict;
    namespace hash_tuple{
        template <typename TT>
        struct hash
        {
            size_t
            operator()(TT const& tt) const
            {
                return std::hash<TT>()(tt);
            }
        };

        namespace {
            template <class T>
            inline void hash_combine(std::size_t& seed, T const& v)
            {
                seed ^= hash_tuple::hash<T>()(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
            }
        }

        namespace {
            // Recursive template code derived from Matthieu M.
            template <class Tuple, size_t Index = std::tuple_size<Tuple>::value - 1>
            struct HashValueImpl
            {
                static void apply(size_t& seed, Tuple const& tuple)
                {
                    HashValueImpl<Tuple, Index-1>::apply(seed, tuple);
                    hash_combine(seed, std::get<Index>(tuple));
                }
            };

            template <class Tuple>
            struct HashValueImpl<Tuple,0>
            {
                static void apply(size_t& seed, Tuple const& tuple)
                {
                    hash_combine(seed, std::get<0>(tuple));
                }
            };
        }

        template <typename ... TT>
        struct hash<std::tuple<TT...>>
        {
            size_t
            operator()(std::tuple<TT...> const& tt) const
            {
                size_t seed = 0;
                HashValueImpl<std::tuple<TT...> >::apply(seed, tt);
                return seed;
            }
        };

    }

    template<bool is_eq_join, typename T>
    struct EqJoin {
        // code that will only "exist" if the query admits equi-join
        static constexpr auto sifted_join_conditions = impl::sift_join_condition<typename T::S1Type, typename T::S2Type>(T::res.join_condition[0]);
        static constexpr auto eq_jc = sifted_join_conditions.first;
        static constexpr auto the_rest_jc = sifted_join_conditions.second;
        static_assert(not eq_jc.empty());
        static constexpr auto hj_indices = impl::make_join_indices<typename T::S1Type, typename T::S2Type, eq_jc.size()>(eq_jc);
        static constexpr std::array t0_hj_indices = hj_indices.first;
        static constexpr std::array t1_hj_indices = hj_indices.second;
        static constexpr auto t0_hj_projector = make_projector<t0_hj_indices>();
        static constexpr auto t1_hj_projector = make_projector<t1_hj_indices>();
        using S1HJT = ProjectedTuple<SchemaTuple<typename T::S1Type>, t0_hj_indices>;
        using S2HJT = ProjectedTuple<SchemaTuple<typename T::S2Type>, t1_hj_indices>;
        static_assert(std::is_same_v<S1HJT, S2HJT>, "misaligned types on equi-join conditions; please fix types and retry");
        using S1Dict = std::unordered_map<S1HJT, std::vector<SchemaTuple<typename T::S1Type>>, hash_tuple::hash<S1HJT>>;
        using S2Dict = std::unordered_map<S2HJT, std::vector<SchemaTuple<typename T::S2Type>>, hash_tuple::hash<S2HJT>>;

        // make a selector from the non-eq join conditions, if there's any
        static constexpr auto non_eq_selector = impl::make_selector_and_cons<typename T::S1Type, typename T::S2Type, false,
            impl::make_indices_1d<typename T::S1Type, typename T::S2Type, true, the_rest_jc.size(), the_rest_jc.size()>(the_rest_jc),
            impl::make_indices_1d<typename T::S1Type, typename T::S2Type, false, the_rest_jc.size(), the_rest_jc.size()>(the_rest_jc),
            impl::make_cop_list_1d<the_rest_jc.size(), the_rest_jc.size()>(the_rest_jc),
            impl::make_rhs_type_list_1d<the_rest_jc.size(), the_rest_jc.size()>(the_rest_jc), the_rest_jc.size()>(the_rest_jc);

        // materialization doesn't matter here, since we build hash tables anyway; two placeholders
        static std::generator<SchemaTuple2<typename T::S1Type, typename T::S2Type>>
        join(std::ranges::range auto& l_input, std::ranges::range auto& r_input, auto, auto) {
            // build index
            S1Dict s1d;
            S2Dict s2d;
            for (const auto& l_tuple: l_input) {
                s1d[t0_hj_projector(l_tuple)].emplace_back(l_tuple);
            }
            for (const auto& r_tuple: r_input) {
                s2d[t1_hj_projector(r_tuple)].emplace_back(r_tuple);
            }
            // do the joining
            for (const auto& [lk, lv]: s1d) {
                auto pos = s2d.find(lk);
                if (pos != s2d.end()) {
                    for (const auto& l_tuple: lv) {
                        for (const auto& r_tuple: pos->second) {
                            if constexpr (the_rest_jc.empty()) {  // no non-eq selector at all
                                co_yield std::tuple_cat(l_tuple, r_tuple);
                            } else {
                                auto lr_tuple = std::tuple_cat(l_tuple, r_tuple);
                                if (non_eq_selector(lr_tuple)) {
                                    co_yield std::move(lr_tuple);
                                }
                            }
                        }
                    }

                }
            }
        }
    };

    // no equi-join available; just use dnf selector
    template<typename T>
    struct EqJoin<false, T> {
        using S1 = typename T::S1Type;
        using S2 = typename T::S2Type;
        static_assert(not std::is_void_v<S1> and not std::is_void_v<S2>);
        static constexpr auto dnf_inner_dim = impl::make_inner_dim<T::res.join_condition.size()>(T::res.join_condition);
        static constexpr auto aligned_dnf = impl::align_dnf<dnf_inner_dim>(T::res.join_condition);
        static constexpr std::optional dnf_selector = aligned_dnf.empty() ? std::nullopt
                                : std::optional{impl::make_dnf_selector<S1, S2, true,
                                      impl::make_indices_2d<S1, S2, true, true, dnf_inner_dim>(aligned_dnf),
                                      impl::make_indices_2d<S1, S2, false, true, dnf_inner_dim>(aligned_dnf),
                                      impl::make_cop_list_2d<dnf_inner_dim>(aligned_dnf), impl::make_rhs_type_list_2d<dnf_inner_dim>(aligned_dnf), dnf_inner_dim>(aligned_dnf)};
        // left side is materialized
        static std::generator<SchemaTuple2<S1, S2>> join(std::ranges::range auto& l_input, std::ranges::range auto& r_input, std::true_type, std::false_type) {
            for (const auto& r_tuple: r_input) {
                for (const auto& l_tuple: l_input) {
                    if constexpr (dnf_selector) {
                        auto lr_tuple = std::tuple_cat(l_tuple, r_tuple);
                        if (dnf_selector.value()(lr_tuple)) {
                            co_yield std::move(lr_tuple);
                        }
                    } else {
                        co_yield std::tuple_cat(l_tuple, r_tuple);
                    }
                }
            }
        }

        static std::generator<SchemaTuple2<S1, S2>> join(std::ranges::range auto& l_input, std::ranges::range auto& r_input, std::false_type, std::true_type) {
            for (const auto& l_tuple: l_input) {
                for (const auto& r_tuple: r_input) {
                    if constexpr (dnf_selector) {
                        auto lr_tuple = std::tuple_cat(l_tuple, r_tuple);
                        if (dnf_selector.value()(lr_tuple)) {
                            co_yield std::move(lr_tuple);
                        }
                    } else {
                        co_yield std::tuple_cat(l_tuple, r_tuple);
                    }
                }
            }
        }

        static std::generator<SchemaTuple2<S1, S2>> join(std::ranges::range auto& l_input, std::ranges::range auto& r_input, std::true_type, std::true_type) {
            return join(l_input, r_input, std::true_type{}, std::false_type{});
        }

        static std::generator<SchemaTuple2<S1, S2>> join(std::ranges::range auto& l_input, std::ranges::range auto& r_input, std::false_type, std::false_type) {
            std::vector<std::ranges::range_value_t<decltype(r_input)>> materialized_r_input;
            for (auto&& r_tuple: r_input) {
                materialized_r_input.emplace_back(std::forward<decltype(r_tuple)>(r_tuple));
            }
            return join(l_input, materialized_r_input, std::false_type{}, std::true_type{});
        }

    };

    template<bool need_join, bool admits_eq_join, typename T>
    struct Join {
        using EqJ = EqJoin<admits_eq_join, T>;
    };

    template<bool admits_eq_join, typename T>
    struct Join<false, admits_eq_join, T> {};

    template<bool has_groups, typename T>
    struct ReduceGroup;
    // reduce by groups
    template<typename T>
    struct ReduceGroup<true, T> {
        using S1 = typename T::S1Type;
        using S2 = typename T::S2Type;
        static constexpr auto projector = T::projector.value();

        static_assert(T::res.group_by_keys.size() != 0);

        static constexpr std::array group_by_indices = []() {
            std::array<std::size_t, T::res.group_by_keys.size()> gb_indices;
            for (size_t i = 0; i < gb_indices.size(); ++i) {
                gb_indices[i] = get_index<S1, S2>(T::res.group_by_keys[i]);
            }
            return gb_indices;
        }();

        using STuple = typename T::STuple;
        using PTuple = typename T::PTuple;
        static constexpr auto gb_projector = impl::make_projector<group_by_indices>();
        using GBTuple = ProjectedTuple<STuple, group_by_indices>;
        using GBDict = std::unordered_map<GBTuple, PTuple, hash_tuple::hash<GBTuple>>;

        static std::generator<PTuple> reduce(std::ranges::range auto& input) {
            GBDict gb_dict;
            auto reduce_op = impl::to_tuple_operator<T::agg_ops>();
            for (const auto& inp_tuple: input) {
                auto gb_tuple = gb_projector(inp_tuple);
                auto pos = gb_dict.find(gb_tuple);
                if (pos == gb_dict.end()) {
                    pos = gb_dict.emplace(gb_tuple, impl::make_reduction_base<PTuple, T::agg_ops>()).first;
                }
                reduce_op(pos->second, projector(inp_tuple));
            }
            for (const auto& kv: gb_dict) {
                co_yield kv.second;
            }
        }

    };

    // global reduce
    template<typename T>
    struct ReduceGroup<false, T> {
        using PTuple = typename T::PTuple;
        static constexpr auto projector = T::projector.value();

        static_assert(T::res.group_by_keys.size() == 0);

        static std::generator<PTuple> reduce(std::ranges::range auto& input) {
            auto reduce_op = impl::to_tuple_operator<T::agg_ops>();
            auto base = make_tuple_reduction_base<PTuple, T::agg_ops>();
            for (const auto& inp_tuple: input) {
                reduce_op(base, projector(inp_tuple));
            }
            co_yield std::move(base);
        }
    };

    template<bool need_reduce, bool need_group_by, typename T>
    struct Reduce;

    template<bool need_group_by, typename T>
    struct Reduce<true, need_group_by, T> {
        using RG = ReduceGroup<need_group_by, T>;
    };

    template<bool need_group_by, typename T>
    struct Reduce<false, need_group_by, T> {};
}

template<refl::const_string query_str, Reflectable S1, Reflectable S2=void>
struct QueryPlanner {
    static constexpr auto cbuf = ctpg::buffers::cstring_buffer(query_str.data);
    static constexpr auto res = impl::resolve_table_name<S1, S2>(impl::dealias_query(SelectParser::p.parse(cbuf).value()));

    using S1Type = S1;
    using S2Type = S2;

    // design selecting strategy
    //  - CNF transformation
    static constexpr size_t cnf_clause_size = res.where_condition.size();
    static constexpr size_t num_cnf_clauses = impl::compute_number_of_cnf_clauses(res.where_condition);
    static constexpr auto cnf = impl::sift(impl::dnf_to_cnf<num_cnf_clauses, cnf_clause_size>(res.where_condition));
    static constexpr size_t t0_end = std::get<0>(cnf);
    static constexpr size_t t1_end = std::get<1>(cnf);
    static constexpr auto sifted_cnf = std::get<2>(cnf); // sorted as [0, t0_end), [t0_end, t1_end), [t1_end, sifted_cnf.size())
    static constexpr auto sift_split = impl::split_sifted<t0_end, t1_end>(sifted_cnf);  // split into 3 sub-matrices
    static constexpr auto t0 = std::get<0>(sift_split);
    static constexpr auto t1 = std::get<1>(sift_split);
    static constexpr auto mixed = std::get<2>(sift_split);
    static_assert((not std::is_void_v<S2>) or (t1.empty() and mixed.empty()));  // sanity check
    static constexpr auto t0_inner_dim = impl::make_inner_dim<t0.size()>(cnf_clause_size);
    static constexpr auto t1_inner_dim = impl::make_inner_dim<t1.size()>(cnf_clause_size);
    static constexpr auto mixed_inner_dim = impl::make_inner_dim<mixed.size()>(cnf_clause_size); // inner dim is trivial cuz CNF clauses are of uniform length
    //  - make selectors
    // TODO: it's very obnoxious that I need to pass stuff in this way; this has to do with the fact that function parameters cannot be a const expr
    static constexpr std::optional t0_selector = t0.empty() ? std::nullopt
                                                   : std::optional{impl::make_cnf_selector<S1, void, true,
                                                         impl::make_indices_2d<S1, void, true, true, t0_inner_dim>(t0),
                                                         impl::make_indices_2d<S1, void, false, true, t0_inner_dim>(t0),  // just a placeholder, rhs_indices not used
                                                         impl::make_cop_list_2d<t0_inner_dim>(t0), impl::make_rhs_type_list_2d<t0_inner_dim>(t0), t0_inner_dim>(t0)};

    static constexpr std::optional t1_selector = []() {
        if constexpr (std::is_void_v<S2>) {
            return std::nullopt;
        } else {
            return t1.empty() ? std::nullopt
                              : std::optional{impl::make_cnf_selector<S2, void, true,
                                    impl::make_indices_2d<S2, void, true, true, t1_inner_dim>(t1),
                                    impl::make_indices_2d<S2, void, false, true, t1_inner_dim>(t1),
                                    impl::make_cop_list_2d<t1_inner_dim>(t1), impl::make_rhs_type_list_2d<t1_inner_dim>(t1), t1_inner_dim>(t1)};
        }
    }();

    static constexpr std::optional mixed_selector = []() {  // the CNF clauses that CANNOT be pushed down
        if constexpr (std::is_void_v<S2>) {
            return std::nullopt;
        } else {
            return mixed.empty() ? std::nullopt
                                 : std::optional{impl::make_cnf_selector<S1, S2, true,
                                       impl::make_indices_2d<S1, S2, true, true, mixed_inner_dim>(mixed),
                                       impl::make_indices_2d<S1, S2, false, true, mixed_inner_dim>(mixed),
                                       impl::make_cop_list_2d<mixed_inner_dim>(mixed), impl::make_rhs_type_list_2d<mixed_inner_dim>(mixed), mixed_inner_dim>(mixed)};
        }
    }();

    //  - when either (1) S2=void (2) t0 & t1 are both null, i.e. selector cannot be decoupled, simply use DNF
    static constexpr auto dnf_inner_dim = impl::make_inner_dim<res.where_condition.size()>(res.where_condition);
    static constexpr auto aligned_dnf = impl::align_dnf<dnf_inner_dim>(res.where_condition);

    static constexpr std::optional dnf_selector = aligned_dnf.empty() ? std::nullopt
                                : std::optional{impl::make_dnf_selector<S1, S2, true,
                                      impl::make_indices_2d<S1, S2, true, true, dnf_inner_dim>(aligned_dnf),
                                      impl::make_indices_2d<S1, S2, false, true, dnf_inner_dim>(aligned_dnf),
                                      impl::make_cop_list_2d<dnf_inner_dim>(aligned_dnf), impl::make_rhs_type_list_2d<dnf_inner_dim>(aligned_dnf), dnf_inner_dim>(aligned_dnf)};

    // if there is only one table OR there's no hope of pushing down, just use DNF selector
    static constexpr bool should_dnf = std::is_void_v<S2> or (!t0_selector and !t1_selector);

    // processing join conditions
    static constexpr bool is_join_well_formed = [](){
        for (const auto& bat: res.join_condition) {
            for (const auto& bf: bat) {
                if (bf.lhs.table_name == bf.rhs.table_name) {
                    return false;
                }
            }
        }
        return true;
    }();
    static_assert(is_join_well_formed, "please only use join conditions for predicates that involve both tables");

    static constexpr bool admits_eq_join = res.join_condition.size() == 1 and [](){
        for (const auto& bf: res.join_condition[0]) {
            if (bf.cop == CompOp::EQ) {
                return true;
            }
        }
        return false;
    }();

    using Join = impl::Join<not res.join_condition.empty(), admits_eq_join, QueryPlanner>;

    // reduction


    static constexpr auto proj_indices_and_agg_ops = impl::make_indices_and_agg_ops<S1, S2, res.cns.size()>(res.cns);
    static constexpr auto proj_indices = proj_indices_and_agg_ops.first;
    static constexpr auto agg_ops = proj_indices_and_agg_ops.second;
    static constexpr std::optional projector = res.cns.empty() ? std::nullopt : std::optional{impl::make_projector<proj_indices>()};

    using STuple = std::conditional_t<std::is_void_v<S2>, SchemaTuple<S1>, SchemaTuple2<S1, S2>>;
    using PTuple = impl::ProjectedTuple<STuple, proj_indices>;

    using Reduce = impl::Reduce<impl::need_reduction<agg_ops>, not res.group_by_keys.empty(), QueryPlanner>;
};
}



#endif //SQL_PLANNER_H
