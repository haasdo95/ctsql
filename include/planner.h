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

    template<bool admits_eq_join, typename QPI>
    struct Join;

    // code that will only "exist" if the query admits equi-join
    template<typename QPI>
    struct Join<true, QPI> {
        using S1 = typename QPI::S1;
        using S2 = typename QPI::S2;
        // two tuple selector from where conditions
        static constexpr std::optional where_two_tuple_selector = QPI::where_two_tuple_selector;

        static constexpr auto sifted_join_conditions = sift_join_condition<S1, S2>(QPI::res.join_condition[0]);
        static constexpr auto eq_jc = sifted_join_conditions.first;
        static constexpr auto the_rest_jc = sifted_join_conditions.second;
        static_assert(not eq_jc.empty());
        static constexpr auto hj_indices = make_join_indices<S1, S2, eq_jc.size()>(eq_jc);
        static constexpr std::array t0_hj_indices = hj_indices.first;
        static constexpr std::array t1_hj_indices = hj_indices.second;
        static constexpr auto t0_hj_projector = make_projector<t0_hj_indices>();
        static constexpr auto t1_hj_projector = make_projector<t1_hj_indices>();
        using S1HJT = ProjectedTuple<SchemaTuple<S1>, t0_hj_indices>;
        using S2HJT = ProjectedTuple<SchemaTuple<S2>, t1_hj_indices>;
        static_assert(std::is_same_v<S1HJT, S2HJT>, "misaligned types on equi-join conditions; please fix types and retry");
        using S1Dict = std::unordered_map<S1HJT, std::vector<SchemaTuple<S1>>, hash_tuple::hash<S1HJT>>;
        using S2Dict = std::unordered_map<S2HJT, std::vector<SchemaTuple<S2>>, hash_tuple::hash<S2HJT>>;

        // make a selector from the non-eq join conditions, if there's any
        static constexpr std::optional non_eq_selector = the_rest_jc.empty() ? std::nullopt : std::optional{impl::make_selector_and_cons<S1, S2, false,
                impl::make_indices_1d<S1, S2, true, the_rest_jc.size(), the_rest_jc.size()>(the_rest_jc),
                impl::make_indices_1d<S1, S2, false, the_rest_jc.size(), the_rest_jc.size()>(the_rest_jc),
                impl::make_cop_list_1d<the_rest_jc.size(), the_rest_jc.size()>(the_rest_jc),
                impl::make_rhs_type_list_1d<the_rest_jc.size(), the_rest_jc.size()>(the_rest_jc), the_rest_jc.size()>(the_rest_jc)};

        // handles the non-eq part of join & where conditions
        static inline constexpr bool predicate(const auto& lr_tuple) {
            if constexpr (non_eq_selector and where_two_tuple_selector) {
                return non_eq_selector.value()(lr_tuple) and where_two_tuple_selector.value()(lr_tuple);
            } else if constexpr (non_eq_selector) {
                return non_eq_selector.value()(lr_tuple);
            } else if constexpr (where_two_tuple_selector) {
                return where_two_tuple_selector.value()(lr_tuple);
            } else {
                return true;
            }
        }

        // we build hash table using the smaller of the two inputs
        static std::generator<SchemaTuple2<S1, S2>> join(std::ranges::range auto& l_input, std::ranges::range auto& r_input,
                                                         std::size_t l_estimated_size, std::size_t r_estimated_size) {
            const auto l_size = get_input_size(l_input, l_estimated_size);
            const auto r_size = get_input_size(r_input, r_estimated_size);
            // standard hash-join; a bit repetitive but should be okay
            if (l_size <= r_size) {  // cannot be determined at compile-time
                S1Dict s1d;
                for (auto&& l_tuple: l_input) {
                    auto& v = s1d[t0_hj_projector(l_tuple)];
                    v.emplace_back(l_tuple);
                }
                for (auto&& r_tuple: r_input) {
                    const auto& r_key = t1_hj_projector(r_tuple);
                    auto pos = s1d.find(r_key);
                    if (pos != s1d.end()) {
                        for (const auto& l_tuple: pos->second) {
                            auto lr_tuple = std::tuple_cat(l_tuple, r_tuple);
                            if (predicate(lr_tuple)) {
                                co_yield lr_tuple;
                            }
                        }
                    }
                }
            } else {
                S2Dict s2d;
                for (auto&& r_tuple: r_input) {
                    auto& v = s2d[t1_hj_projector(r_tuple)];
                    v.emplace_back(r_tuple);
                }
                for (auto&& l_tuple: l_input) {
                    const auto& l_key = t0_hj_projector(l_tuple);
                    auto pos = s2d.find(l_key);
                    if (pos != s2d.end()) {
                        for (const auto& r_tuple: pos->second) {
                            auto lr_tuple = std::tuple_cat(l_tuple, r_tuple);
                            if (predicate(lr_tuple)) {
                                co_yield lr_tuple;
                            }
                        }
                    }
                }

            }
        }
    };

    // no equi-join available; just use dnf selector
    template<typename QPI>
    struct Join<false, QPI> {
        using S1 = typename QPI::S1;
        using S2 = typename QPI::S2;
        static_assert(not std::is_void_v<S1> and not std::is_void_v<S2>);
        static constexpr auto dnf_join_inner_dim = impl::make_inner_dim<QPI::res.join_condition.size()>(QPI::res.join_condition);
        static constexpr auto aligned_join_dnf = impl::align_dnf<dnf_join_inner_dim>(QPI::res.join_condition);
        static constexpr std::optional dnf_join_selector = aligned_join_dnf.empty() ? std::nullopt : std::optional{impl::make_dnf_selector<S1, S2, false,
                impl::make_indices_2d<S1, S2, true, false, dnf_join_inner_dim>(aligned_join_dnf),
                impl::make_indices_2d<S1, S2, false, false, dnf_join_inner_dim>(aligned_join_dnf),
                impl::make_cop_list_2d<dnf_join_inner_dim>(aligned_join_dnf), impl::make_rhs_type_list_2d<dnf_join_inner_dim>(aligned_join_dnf), dnf_join_inner_dim>(aligned_join_dnf)};

        // two-tuple selector from where conditions
        static constexpr std::optional where_two_tuple_selector = QPI::where_two_tuple_selector;

        // handles join & where conditions
        static inline constexpr bool predicate(const auto& lr_tuple) {
            if constexpr (dnf_join_selector and where_two_tuple_selector) {
                return dnf_join_selector.value()(lr_tuple) and where_two_tuple_selector.value()(lr_tuple);
            } else if constexpr (dnf_join_selector) {
                return dnf_join_selector.value()(lr_tuple);
            } else if constexpr (where_two_tuple_selector) {
                return where_two_tuple_selector.value()(lr_tuple);
            } else {  // no 2-tuple filter at all
                return true;
            }
        }

        template<class Input>
        static constexpr bool is_materialized = std::ranges::sized_range<Input> and std::ranges::common_range<Input>;

        // making the assumption that if a range is sized, it can be iterated for multiple times
        static std::generator<SchemaTuple2<S1, S2>> join(std::ranges::range auto& l_input, std::ranges::range auto& r_input,
                                                         std::size_t l_estimated_size, std::size_t r_estimated_size) {
            if constexpr (is_materialized<decltype(l_input)>) {
                for (const auto& r_tuple: r_input) {  // one-pass through r-input
                    for (const auto& l_tuple: l_input) {
                        auto lr_tuple = std::tuple_cat(l_tuple, r_tuple);
                        if (predicate(lr_tuple)) {
                            co_yield lr_tuple;
                        }
                    }
                }
            } else if constexpr (is_materialized<decltype(r_input)>) {
                for (const auto& l_tuple: l_input) {  // one-pass through l-input
                    for (const auto &r_tuple: r_input) {
                        auto lr_tuple = std::tuple_cat(l_tuple, r_tuple);
                        if (predicate(lr_tuple)) {
                            co_yield lr_tuple;
                        }
                    }
                }
            } else {  // neither is materialized; materialize the smaller one
                const auto l_size = get_input_size(l_input, l_estimated_size);
                const auto r_size = get_input_size(r_input, r_estimated_size);
                if (l_size <= r_size) {
                    std::vector<std::ranges::range_value_t<decltype(l_input)>> materialized_input;
                    for (auto&& l_tuple: l_input) {
                        materialized_input.emplace_back(l_tuple);
                    }
                    co_yield std::ranges::elements_of(join(materialized_input, r_input, l_estimated_size, r_estimated_size));
                } else {
                    std::vector<std::ranges::range_value_t<decltype(r_input)>> materialized_input;
                    for (auto&& r_tuple: r_input) {
                        materialized_input.emplace_back(r_tuple);
                    }
                    co_yield std::ranges::elements_of(join(l_input, materialized_input, l_estimated_size, r_estimated_size));
                }
            }
        }
    };

    template<bool has_groups, typename QP>
    struct ReduceGroup;

    // reduce by groups
    template<typename QP>
    struct ReduceGroup<true, QP> {
        static constexpr auto projector = QP::projector.value();

        static_assert(QP::res.group_by_keys.size() != 0);

        static constexpr std::array group_by_indices = []() {
            std::array<std::size_t, QP::res.group_by_keys.size()> gb_indices;
            for (size_t i = 0; i < gb_indices.size(); ++i) {
                gb_indices[i] = get_index<typename QP::S1Type, typename QP::S2Type>(QP::res.group_by_keys[i]);
            }
            return gb_indices;
        }();

        using STuple = typename QP::STuple;
        using PTuple = typename QP::PTuple;
        static constexpr auto gb_projector = impl::make_projector<group_by_indices>();
        using GBTuple = ProjectedTuple<STuple, group_by_indices>;
        using GBDict = std::unordered_map<GBTuple, PTuple, hash_tuple::hash<GBTuple>>;

        static std::generator<PTuple> reduce(std::ranges::range auto& input) {
            GBDict gb_dict;
            std::function<void(PTuple&, const PTuple&)> reduce_op = to_tuple_operator<QP::agg_ops>();
            for (auto&& inp_tuple: input) {
                auto gb_tuple = gb_projector(inp_tuple);
                auto pos = gb_dict.find(gb_tuple);
                if (pos == gb_dict.end()) {
                    reduce_op = to_tuple_operator<QP::agg_ops>();  // reset
                    pos = gb_dict.emplace(gb_tuple, make_tuple_reduction_base<PTuple, QP::agg_ops>()).first;
                }
                reduce_op(pos->second, projector(inp_tuple));
            }
            for (const auto& kv: gb_dict) {
                co_yield kv.second;
            }
        }

    };

    // global reduce
    template<typename QP>
    struct ReduceGroup<false, QP> {
        using PTuple = typename QP::PTuple;
        static constexpr auto projector = QP::projector.value();

        static_assert(QP::res.group_by_keys.size() == 0);

        static std::generator<PTuple> reduce(std::ranges::range auto& input) {
            auto reduce_op = to_tuple_operator<QP::agg_ops>();
            auto base = make_tuple_reduction_base<PTuple, QP::agg_ops>();
            for (const auto& inp_tuple: input) {
                reduce_op(base, projector(inp_tuple));
            }
            co_yield base;
        }
    };

    template<bool need_reduce, bool need_group_by, typename QP>
    struct Reduce;

    template<bool need_group_by, typename QP>
    struct Reduce<true, need_group_by, QP> {
        using RG = ReduceGroup<need_group_by, QP>;
    };

    template<bool need_group_by, typename QP>
    struct Reduce<false, need_group_by, QP> {};
}

template<bool one_table, typename QP>
struct QueryPlannerImpl;

// query planner that only involves one table
template<typename QP>
struct QueryPlannerImpl<true, QP> {
    using STuple = SchemaTuple<typename QP::S1Type>;
};

// query planner that involves two tables
template<typename QP>
struct QueryPlannerImpl<false, QP> {
    using S1 = typename QP::S1Type;
    using S2 = typename QP::S2Type;
    static_assert(not std::is_void_v<S2>);
    static constexpr auto res = QP::res;  // this is the parsed SQL statement
    using STuple = SchemaTuple2<S1, S2>;

    //  - DNF -> CNF transformation
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

    // given the raw input in the form of DNF (several AND clauses connected by OR),
    // we compute the equivalent CNF, which is several OR clauses connected by AND.
    // we split these OR clauses based on the tables that it makes reference to.
    // table0 -> t0_selector; table1 -> t1_selector; mixed -> mixed_selector.
    // note that mixed_selector is a selector on joined tuple, as it should be

    // if either t0 or t1 is non-empty, we can and will use operator push-down;
    //      after the push-down, we still need to filter the joined tuple according to the mixed-selector to ensure correctness
    // if both t0 and t1 are empty, meaning that push-down is impossible, we abandon the CNF entirely and use the original DNF

    static constexpr std::optional t0_selector = t0.empty() ? std::nullopt : std::optional{impl::make_cnf_selector<S1, void, true,
                    impl::make_indices_2d<S1, void, true, true, t0_inner_dim>(t0),
                    impl::make_indices_2d<S1, void, false, true, t0_inner_dim>(t0),  // just a placeholder, rhs_indices not used
                    impl::make_cop_list_2d<t0_inner_dim>(t0), impl::make_rhs_type_list_2d<t0_inner_dim>(t0), t0_inner_dim>(t0)};

    static constexpr std::optional t1_selector = t1.empty() ? std::nullopt : std::optional{impl::make_cnf_selector<S2, void, true,
                    impl::make_indices_2d<S2, void, true, true, t1_inner_dim>(t1),
                    impl::make_indices_2d<S2, void, false, true, t1_inner_dim>(t1),
                    impl::make_cop_list_2d<t1_inner_dim>(t1), impl::make_rhs_type_list_2d<t1_inner_dim>(t1), t1_inner_dim>(t1)};

    static constexpr std::optional mixed_selector = mixed.empty() ? std::nullopt : std::optional{impl::make_cnf_selector<S1, S2, true,
                    impl::make_indices_2d<S1, S2, true, true, mixed_inner_dim>(mixed),
                    impl::make_indices_2d<S1, S2, false, true, mixed_inner_dim>(mixed),
                    impl::make_cop_list_2d<mixed_inner_dim>(mixed), impl::make_rhs_type_list_2d<mixed_inner_dim>(mixed), mixed_inner_dim>(mixed)};

    static constexpr bool use_push_down = t0_selector or t1_selector;

    // fall back if push-down is impossible
    static constexpr std::optional dnf_where_selector = QP::dnf_where_selector;

    // combining the above, the selector on joined tuples
    static constexpr std::optional where_two_tuple_selector = [](){
        if constexpr (use_push_down) { return mixed_selector; }
        else { return dnf_where_selector; }
    }();

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

    // eq-join is possible if (1) no OR exists in the statement (2) one of the comparisons is equality
    static constexpr bool admits_eq_join = res.join_condition.size() == 1 and [](){
        for (const auto& bf: res.join_condition[0]) {
            if (bf.cop == CompOp::EQ) {
                return true;
            }
        }
        return false;
    }();

    using Join = impl::Join<admits_eq_join, QueryPlannerImpl>;
};

template<refl::const_string query_str, Reflectable S1, Reflectable S2=void>
struct QueryPlanner {
    static constexpr auto cbuf = ctpg::buffers::cstring_buffer(query_str.data);
    static constexpr auto res = impl::resolve_table_name<S1, S2>(impl::dealias_query(SelectParser::p.parse(cbuf).value()));

    using S1Type = S1;
    using S2Type = S2;

    static constexpr auto dnf_where_inner_dim = impl::make_inner_dim<res.where_condition.size()>(res.where_condition);
    static constexpr auto aligned_where_dnf = impl::align_dnf<dnf_where_inner_dim>(res.where_condition);

    static constexpr std::optional dnf_where_selector = aligned_where_dnf.empty() ? std::nullopt : std::optional{impl::make_dnf_selector<S1, S2, true,
            impl::make_indices_2d<S1, S2, true, true, dnf_where_inner_dim>(aligned_where_dnf), impl::make_indices_2d<S1, S2, false, true, dnf_where_inner_dim>(aligned_where_dnf),
            impl::make_cop_list_2d<dnf_where_inner_dim>(aligned_where_dnf), impl::make_rhs_type_list_2d<dnf_where_inner_dim>(aligned_where_dnf), dnf_where_inner_dim>(aligned_where_dnf)};

    using QPI = QueryPlannerImpl<std::is_void_v<S2>, QueryPlanner>;

    static constexpr auto proj_indices_and_agg_ops = impl::make_indices_and_agg_ops<S1, S2, res.cns.size()>(res.cns);
    static constexpr auto proj_indices = proj_indices_and_agg_ops.first;
    static constexpr auto agg_ops = proj_indices_and_agg_ops.second;
    static constexpr std::optional projector = res.cns.empty() ? std::nullopt : std::optional{impl::make_projector<proj_indices>()};

    using STuple = typename QPI::STuple;
    using PTuple = impl::ProjectedTuple<STuple, proj_indices>;

    static constexpr bool need_reduce = impl::need_reduction<agg_ops>;
    using Reduce = impl::Reduce<need_reduce, not res.group_by_keys.empty(), QueryPlanner>;
    using ResultType = std::conditional_t<projector.has_value(), PTuple, STuple>;

    static std::generator<ResultType> reduce_project(std::ranges::range auto& input) {
        if constexpr (need_reduce) {
            auto reduced = Reduce::RG::reduce(input);
            co_yield std::ranges::elements_of(reduced);
        } else {  // only apply projector
            if constexpr (projector) {
                for (auto&& t: input) {
                    co_yield projector.value()(t);
                }
            } else {
                co_yield std::ranges::elements_of(input);
            }
        }
    }

};

template<typename QP> requires requires { std::is_void_v<typename QP::S2Type>; }
std::generator<typename QP::ResultType> process(std::ranges::range auto& input) {
    if constexpr (QP::dnf_where_selector) {
        auto filtered = filter<QP::dnf_where_selector.value()>(input);
        co_yield std::ranges::elements_of(QP::reduce_project(filtered));
    } else {
        co_yield std::ranges::elements_of(QP::reduce_project(input));
    }
}

template<typename QP> requires requires { not std::is_void_v<typename QP::S2Type>; }
std::generator<typename QP::ResultType> process(std::ranges::range auto& l_input, std::ranges::range auto& r_input,
                                                std::size_t l_estimated_size=0, std::size_t r_estimated_size=1) {
    // this is clumsy, but it preserves the materialized-ness of the input
    if constexpr (QP::QPI::t0_selector and QP::QPI::t1_selector) {
        auto l_filtered = filter<QP::QPI::t0_selector.value()>(l_input);
        auto r_filtered = filter<QP::QPI::t1_selector.value()>(r_input);
        auto joined = QP::QPI::Join::join(l_filtered, r_filtered, l_estimated_size, r_estimated_size);
        co_yield std::ranges::elements_of(QP::reduce_project(joined));
    } else if constexpr (QP::QPI::t0_selector) {
        auto l_filtered = filter<QP::QPI::t0_selector.value()>(l_input);
        auto joined = QP::QPI::Join::join(l_filtered, r_input, l_estimated_size, r_estimated_size);
        co_yield std::ranges::elements_of(QP::reduce_project(joined));
    } else if constexpr (QP::QPI::t1_selector) {
        auto r_filtered = filter<QP::QPI::t1_selector.value()>(r_input);
        auto joined = QP::QPI::Join::join(l_input, r_filtered, l_estimated_size, r_estimated_size);
        co_yield std::ranges::elements_of(QP::reduce_project(joined));
    } else {  // we do not use push-down at all
        auto joined = QP::QPI::Join::join(l_input, r_input, l_estimated_size, r_estimated_size);
        co_yield std::ranges::elements_of(QP::reduce_project(joined));
    }
}

}



#endif //SQL_PLANNER_H
