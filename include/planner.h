#ifndef SQL_PLANNER_H
#define SQL_PLANNER_H
//#include <parser/parser.h>
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

        static auto equi_join(std::ranges::range auto& l_input, std::ranges::range auto& r_input) {
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
                            co_yield std::tuple_cat(l_tuple, r_tuple);
                        }
                    }

                }
            }
        }
    };

    template<typename T>
    struct EqJoin<false, T> {};
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

    static constexpr bool can_eq_join = res.join_condition.size() == 1 and [](){
        for (const auto& bf: res.join_condition[0]) {
            if (bf.cop == CompOp::EQ) {
                return true;
            }
        }
        return false;
    }();

    using EqJ = impl::EqJoin<can_eq_join, QueryPlanner>;
//    static constexpr auto x = EqJ::sifted_join_conditions;
//    using S1HJT = typename EqJ::S1HJT;
//    using S2HJT = typename EqJ::S2HJT;

    // joined tuple selector
    static constexpr auto non_eq_join_conditions = [](){
        if constexpr (can_eq_join) {
            return BooleanOrTerms<false>{EqJ::the_rest_jc, 1};
        } else {
            return res.join_condition;
        }
    }();

};
}



#endif //SQL_PLANNER_H
