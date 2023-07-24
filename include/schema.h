#ifndef SQL_SCHEMA_H
#define SQL_SCHEMA_H

#include "refl.hpp"
#include "parser/common.h"
#include <concepts>
#include <variant>
#include <tuple>

namespace ctsql
{
namespace impl {
    enum class RHSTypeTag {
        INT=0, DOUBLE, STRING
    };

    template<typename T>
    concept Reflectable = refl::is_reflectable<T>() or std::is_void_v<T>;

    // schema object => tuple with field values
    template<Reflectable Schema>
    constexpr auto schema_to_tuple(const Schema& schema) {
        return refl::util::map_to_tuple(refl::reflect<Schema>().members, [&schema](auto td){
            return td(schema);
        });
    }

    template<Reflectable Schema> requires std::default_initializable<Schema>
    using SchemaTuple = decltype(schema_to_tuple(Schema{}));

    template<Reflectable S1, Reflectable S2>
    constexpr auto schema_to_tuple_2(const S1& s1, const S2& s2) {
        return std::tuple_cat(schema_to_tuple(s1), schema_to_tuple(s2));
    }

    template<Reflectable S1, Reflectable S2> requires std::default_initializable<S1> and std::default_initializable<S2>
    using SchemaTuple2 = decltype(schema_to_tuple_2(S1{}, S2{}));

    template<Reflectable Schema>
    static constexpr auto member_list = refl::util::map_to_array<std::string_view>(refl::reflect<Schema>().members,
                                                                                   [](auto td){return td.name.str_view();});

    template<Reflectable S1, Reflectable S2>
    constexpr std::size_t get_index(const BasicColumnName& bcn) {
        if constexpr (std::is_void_v<S2>) {
            const auto& ml = member_list<S1>;
            auto pos = std::find(ml.begin(), ml.end(), bcn.column_name);
            return std::distance(ml.begin(), pos);
        } else {
            if (bcn.table_name == "0") {
                return get_index<S1, void>(bcn);
            } else {
                return member_list<S1>.size() + get_index<S2, void>(bcn);  // with offset
            }
        }
    }

    template<Reflectable S1, Reflectable S2, std::size_t N, typename Container>
    constexpr auto make_lhs_indices(const Container& bfs) {
        std::array<std::size_t, N> indices;
        for (size_t i=0; i<N; ++i) {
            indices[i] = get_index<S1, S2>(bfs[i].lhs);
        }
        return indices;
    }

    template<Reflectable S1, Reflectable S2, std::size_t N, std::size_t M, std::size_t... Idx>
    constexpr auto make_lhs_indices_impl(const std::array<std::array<BooleanFactor<>, N>, M>& cnf, std::index_sequence<Idx...>) {
        return refl::util::to_array<std::array<std::size_t, N>>(std::make_tuple(make_lhs_indices<S1, S2, N>(cnf[Idx])...));
    }

    template<Reflectable S1, Reflectable S2, std::size_t N, std::size_t M>
    constexpr auto make_lhs_indices(const std::array<std::array<BooleanFactor<>, N>, M>& cnf) {
        return make_lhs_indices_impl<S1, S2>(cnf, std::make_index_sequence<M>());
    }

    template<std::size_t N, typename Container>
    constexpr auto make_cop_list(const Container& bfs) {
        std::array<CompOp, N> cops;
        for (size_t i=0; i<N; ++i) {
            cops[i] = bfs[i].cop;
        }
        return cops;
    }

    template<std::size_t N, std::size_t M, std::size_t... Idx>
    constexpr auto make_cop_list_impl(const std::array<std::array<BooleanFactor<>, N>, M>& cnf, std::index_sequence<Idx...>) {
        return refl::util::to_array<std::array<CompOp, N>>(std::make_tuple(make_cop_list<N>(cnf[Idx])...));
    }

    template<std::size_t N, std::size_t M>
    constexpr auto make_cop_list(const std::array<std::array<BooleanFactor<true>, N>, M>& cnf) {
        return make_cop_list_impl(cnf, std::make_index_sequence<M>());
    }

    template<std::size_t N, typename Container>
    constexpr auto make_rhs_type_list(const Container& bfs) {
        std::array<RHSTypeTag, N> rhs_types;
        for (size_t i=0; i<N; ++i) {
            if (std::holds_alternative<int64_t>(bfs[i].rhs)) {
                rhs_types[i] = RHSTypeTag::INT;
            } else if (std::holds_alternative<double>(bfs[i].rhs)) {
                rhs_types[i] = RHSTypeTag::DOUBLE;
            } else {
                rhs_types[i] = RHSTypeTag::STRING;
            }
        }
        return rhs_types;
    }

    template<std::size_t N, std::size_t M, std::size_t... Idx>
    constexpr auto make_rhs_type_list_impl(const std::array<std::array<BooleanFactor<>, N>, M>& cnf, std::index_sequence<Idx...>) {
        return refl::util::to_array<std::array<RHSTypeTag, N>>(std::make_tuple(make_rhs_type_list<N>(cnf[Idx])...));
    }

    template<std::size_t N, std::size_t M>
    constexpr auto make_rhs_type_list(const std::array<std::array<BooleanFactor<>, N>, M>& cnf) {
        return make_rhs_type_list_impl(cnf, std::make_index_sequence<M>());
    }

    template<RHSTypeTag rhs_type>
    using RHS = std::conditional_t<rhs_type==RHSTypeTag::INT, int64_t, std::conditional_t<rhs_type==RHSTypeTag::DOUBLE, double, std::string_view>>;

    template<Reflectable S1, Reflectable S2, size_t lhs_idx, CompOp cop, RHSTypeTag rhs_type>
    constexpr auto make_selector(const BooleanFactor<>& bf) {
        const auto comp_f = to_operator<cop>();
        if constexpr (std::is_void_v<S2>) {
            return [comp_f, &bf](const SchemaTuple<S1>& s) -> bool {
                return comp_f(std::get<lhs_idx>(s), std::get<RHS<rhs_type>>(bf.rhs));
            };
        } else {
            return [comp_f, &bf](const SchemaTuple2<S1, S2>& s) -> bool {
                return comp_f(std::get<lhs_idx>(s), std::get<RHS<rhs_type>>(bf.rhs));
            };
        }
    }

//    template<Reflectable Schema, std::array lhs_indices, std::array cop_list, std::array rhs_types, std::size_t N, std::size_t... Idx>
//    constexpr auto make_selectors_impl(const std::array<BooleanFactor, N>& bfs, std::index_sequence<Idx...>) {
//        return std::make_tuple(make_selector<Schema, lhs_indices[Idx], cop_list[Idx], rhs_types[Idx]>(bfs[Idx])...);
//    }
//
//    template<Reflectable Schema, std::array lhs_indices, std::array cop_list, std::array rhs_types, std::size_t N>
//    constexpr auto make_selectors(const std::array<BooleanFactor, N>& bfs) {
//        return make_selectors_impl<Schema, lhs_indices, cop_list, rhs_types>(bfs, std::make_index_sequence<N>());
//    }
//
//    template<Reflectable Schema, // type is std::array<std::array<size_t, N>, M>, etc
//            std::array lhs_indices, std::array cop_list, std::array rhs_types, std::size_t N, std::size_t M, std::size_t... Idx>
//    constexpr auto make_selectors_impl(const std::array<std::array<BooleanFactor, N>, M>& cnf, std::index_sequence<Idx...>) {
//        return std::make_tuple(make_selectors<Schema, lhs_indices[Idx], cop_list[Idx], rhs_types[Idx]>(cnf[Idx])...);
//    }
//
//    template<Reflectable Schema, std::array lhs_indices, std::array cop_list, std::array rhs_types, std::size_t N, std::size_t M>
//    constexpr auto make_selectors(const std::array<std::array<BooleanFactor, N>, M>& cnf) {
//        return make_selectors_impl<Schema, lhs_indices, cop_list, rhs_types>(cnf, std::make_index_sequence<M>());
//    }

    template<typename... Selectors>
    constexpr auto and_construct(Selectors... selector);

    template<typename Selector, typename... Selectors>
    constexpr auto and_construct(Selector s, Selectors... selectors) {
        const auto rec = and_construct(selectors...);
        return [s, rec](const auto& tuple){ return s(tuple) and rec(tuple); };
    }

    // actual base case
    template<typename Selector>
    constexpr auto and_construct(Selector s) {
        return [s](const auto& tuple){ return s(tuple); };
    }

    // no filter -> true value always; special case
    template<>
    constexpr auto and_construct() {
        return [](const auto& tuple) { return true; };
    }

    template<typename... Selectors>
    constexpr auto or_construct(Selectors... selector);

    template<typename Selector, typename... Selectors>
    constexpr auto or_construct(Selector s, Selectors... selectors) {
        const auto rec = or_construct(selectors...);
        return [s, rec](const auto& tuple){ return s(tuple) or rec(tuple); };
    }

    // actual base case
    template<typename Selector>
    constexpr auto or_construct(Selector s) {
        return [s](const auto& tuple){ return s(tuple); };
    }

    // no filter -> true value always; special case
    template<>
    constexpr auto or_construct() {
        return [](const auto& tuple) { return true; };
    }

    // DNF selector
    template<Reflectable Schema>
    constexpr auto make_dnf_selector() {

    }

    // turning the "CNF matrix" into a boolean-valued selector
    template<Reflectable S1, Reflectable S2, std::array lhs_indices, std::array cop_list, std::array rhs_types, std::size_t N, typename Vec, std::size_t... Idx>
    constexpr auto make_selector_or_cons_impl(const Vec& bfs, std::index_sequence<Idx...>) {
        return or_construct(make_selector<S1, S2, lhs_indices[Idx], cop_list[Idx], rhs_types[Idx]>(bfs[Idx])...);
    }
    template<Reflectable S1, Reflectable S2, std::array lhs_indices, std::array cop_list, std::array rhs_types, std::size_t N, typename Vec>
    constexpr auto make_selector_or_cons(const Vec& bfs) {
        return make_selector_or_cons_impl<S1, S2, lhs_indices, cop_list, rhs_types, N>(bfs, std::make_index_sequence<N>());
    }

    template<Reflectable S1, Reflectable S2, std::array lhs_indices, std::array cop_list, std::array rhs_types, std::size_t N, typename Vec, std::size_t... Idx>
    constexpr auto make_selector_and_cons_impl(const Vec& bfs, std::index_sequence<Idx...>) {
        return and_construct(make_selector<S1, S2, lhs_indices[Idx], cop_list[Idx], rhs_types[Idx]>(bfs[Idx])...);
    }
    template<Reflectable S1, Reflectable S2, std::array lhs_indices, std::array cop_list, std::array rhs_types, std::size_t N, typename Vec>
    constexpr auto make_selector_and_cons(const Vec& bfs) {
        return make_selector_and_cons_impl<S1, S2, lhs_indices, cop_list, rhs_types, N>(bfs, std::make_index_sequence<N>());
    }

    template<Reflectable S1, Reflectable S2, // type is std::array<std::array<size_t, N>, M>, etc
            std::array lhs_indices, std::array cop_list, std::array rhs_types, std::size_t N, std::size_t M, std::size_t... Idx>
    constexpr auto make_cnf_selector_impl(const std::array<std::array<BooleanFactor<>, N>, M>& cnf, std::index_sequence<Idx...>) {
        return and_construct(make_selector_or_cons<S1, S2, lhs_indices[Idx], cop_list[Idx], rhs_types[Idx], N>(cnf[Idx])...);
    }

    template<Reflectable S1, Reflectable S2, // type is std::array<std::array<size_t, N>, M>, etc
            std::array lhs_indices, std::array cop_list, std::array rhs_types, std::size_t N, std::size_t M>
    constexpr auto make_cnf_selector(const std::array<std::array<BooleanFactor<>, N>, M>& cnf) {
        return make_cnf_selector_impl<S1, S2, lhs_indices, cop_list, rhs_types, N, M>(cnf, std::make_index_sequence<M>());
    }

    template<std::size_t M, std::size_t N>
    constexpr auto sift(std::array<std::array<BooleanFactor<>, N>, M> conditions) {
        size_t sentinel = 0;
        for (size_t i=0; i<M; ++i) {
            if (std::all_of(conditions[i].begin(), conditions[i].end(), [](const BooleanFactor<>& bf){ return bf.lhs.table_name == "0"; })) {
                std::swap(conditions[sentinel], conditions[i]);
                ++sentinel;
            }
        }
        const size_t table_0_delim = sentinel;
        for (size_t i=table_0_delim; i<M; ++i) {
            if (std::all_of(conditions[i].begin(), conditions[i].end(), [](const BooleanFactor<>& bf){ return bf.lhs.table_name == "1"; })) {
                std::swap(conditions[sentinel], conditions[i]);
                ++sentinel;
            }
        }
        const size_t table_1_delim = sentinel;
        return std::make_tuple(table_0_delim, table_1_delim, conditions);
    }


    template<std::size_t t0_end, std::size_t t1_end, std::size_t M, std::size_t N>
    constexpr auto split_sifted(const std::array<std::array<BooleanFactor<>, N>, M>& conditions) {
        static_assert(t0_end <= t1_end and t1_end <= M);
        std::array<std::array<BooleanFactor<>, N>, t0_end> t0;
        for (size_t i = 0; i < t0_end; ++i) {
            t0[i] = conditions[i];
        }
        std::array<std::array<BooleanFactor<>, N>, t1_end-t0_end> t1;
        for (size_t i = 0; i < t1_end-t0_end; ++i) {
            t1[i] = conditions[t0_end + i];
        }
        std::array<std::array<BooleanFactor<>, N>, M-t1_end> mixed;
        for (size_t i = 0; i < M-t1_end; ++i) {
            mixed[i] = conditions[t1_end + i];
        }
        return std::make_tuple(t0, t1, mixed);
    }


}
}

#endif //SQL_SCHEMA_H
