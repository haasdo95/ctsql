#ifndef SQL_SELECTOR_H
#define SQL_SELECTOR_H

#include "common.h"
#include <variant>
#include <tuple>

namespace ctsql
{
namespace impl {
    template<Reflectable S1, Reflectable S2, bool is_lhs, std::size_t Cap, std::size_t Len, typename Vec>
    constexpr auto make_indices_1d(const Vec& bfs) {
        std::array<std::size_t, Cap> indices{};
        for (size_t i=0; i < Len; ++i) {
            if constexpr (is_lhs) {
                indices[i] = get_index<S1, S2>(bfs[i].lhs);
            } else {
                if constexpr (ColumnReference<decltype(bfs[i].rhs)>) {
                    indices[i] = get_index<S1, S2>(bfs[i].rhs);
                }
            }
        }
        return indices;
    }

    template<Reflectable S1, Reflectable S2, bool is_lhs, bool one_side, std::array Lens, std::size_t Cap, std::size_t... Idx>
    constexpr auto make_indices_2d_impl(const std::array<std::array<BooleanFactor<one_side>, Cap>, Lens.size()>& cnf, std::index_sequence<Idx...>) {
        return refl::util::to_array<std::array<std::size_t, Cap>>(std::make_tuple(make_indices_1d<S1, S2, is_lhs, Cap, Lens[Idx]>(cnf[Idx])...));
    }

    template<Reflectable S1, Reflectable S2, bool is_lhs, bool one_side, std::array Lens, std::size_t Cap>
    constexpr auto make_indices_2d(const std::array<std::array<BooleanFactor<one_side>, Cap>, Lens.size()>& cnf) {
        return make_indices_2d_impl<S1, S2, is_lhs, one_side, Lens, Cap>(cnf, std::make_index_sequence<Lens.size()>());
    }

    template<std::size_t Cap, std::size_t Len, typename Vec>
    constexpr auto make_cop_list_1d(const Vec& bfs) {
        std::array<CompOp, Cap> cops{};
        for (size_t i=0; i<Len; ++i) {
            cops[i] = bfs[i].cop;
        }
        return cops;
    }

    template<std::array Lens, std::size_t Cap, bool one_side, std::size_t... Idx>
    constexpr auto make_cop_list_2d_impl(const std::array<std::array<BooleanFactor<one_side>, Cap>, Lens.size()>& cnf, std::index_sequence<Idx...>) {
        return refl::util::to_array<std::array<CompOp, Cap>>(std::make_tuple(make_cop_list_1d<Cap, Lens[Idx]>(cnf[Idx])...));
    }

    template<std::array Lens, std::size_t Cap, bool one_side>
    constexpr auto make_cop_list_2d(const std::array<std::array<BooleanFactor<one_side>, Cap>, Lens.size()>& cnf) {
        return make_cop_list_2d_impl<Lens, Cap, one_side>(cnf, std::make_index_sequence<Lens.size()>());
    }

    template<std::size_t Cap, std::size_t Len, typename Vec>
    constexpr auto make_rhs_type_list_1d(const Vec& bfs) {
        std::array<RHSTypeTag, Cap> rhs_types{};
        for (size_t i=0; i<Len; ++i) {
            if constexpr (ColumnReference<decltype(bfs[i].rhs)>) {
                rhs_types[i] = RHSTypeTag::COLNAME;
            } else {
                if (std::holds_alternative<int64_t>(bfs[i].rhs)) {
                    rhs_types[i] = RHSTypeTag::INT;
                } else if (std::holds_alternative<double>(bfs[i].rhs)) {
                    rhs_types[i] = RHSTypeTag::DOUBLE;
                } else {
                    rhs_types[i] = RHSTypeTag::STRING;
                }
            }
        }
        return rhs_types;
    }

    template<std::array Lens, std::size_t Cap, bool one_side, std::size_t... Idx>
    constexpr auto make_rhs_type_list_2d_impl(const std::array<std::array<BooleanFactor<one_side>, Cap>, Lens.size()>& cnf, std::index_sequence<Idx...>) {
        return refl::util::to_array<std::array<RHSTypeTag, Cap>>(std::make_tuple(make_rhs_type_list_1d<Cap, Lens[Idx]>(cnf[Idx])...));
    }

    template<std::array Lens, std::size_t Cap, bool one_side>
    constexpr auto make_rhs_type_list_2d(const std::array<std::array<BooleanFactor<one_side>, Cap>, Lens.size()>& cnf) {
        return make_rhs_type_list_2d_impl<Lens, Cap, one_side>(cnf, std::make_index_sequence<Lens.size()>());
    }

    template<RHSTypeTag rhs_type>
    using RHS = std::conditional_t<rhs_type==RHSTypeTag::INT, int64_t, std::conditional_t<rhs_type==RHSTypeTag::DOUBLE, double, std::string_view>>;

    template<Reflectable S1, Reflectable S2, bool one_side, size_t lhs_idx, size_t rhs_idx, CompOp cop, RHSTypeTag rhs_type>
    constexpr auto make_selector(const BooleanFactor<one_side>& bf) {
        constexpr auto comp_f = to_operator<cop>();
        if constexpr (one_side) {
            if constexpr (std::is_void_v<S2>) {
                return [comp_f, rhs_val = std::get<RHS<rhs_type>>(bf.rhs)](const SchemaTuple<S1>& s) -> bool {
                    return comp_f(std::get<lhs_idx>(s), rhs_val);
                };
            } else {
                return [comp_f, rhs_val = std::get<RHS<rhs_type>>(bf.rhs)](const SchemaTuple2<S1, S2>& s) -> bool {
                    return comp_f(std::get<lhs_idx>(s), rhs_val);
                };
            }
        } else {
            static_assert(not std::is_void_v<S2>);
            return [comp_f](const SchemaTuple2<S1, S2>& s) -> bool {
                return comp_f(std::get<lhs_idx>(s), std::get<rhs_idx>(s));
            };
        }

    }

    template<typename... Selectors>
    [[maybe_unused]] constexpr auto and_construct(Selectors... selector);

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
    [[maybe_unused]] constexpr auto or_construct(Selectors... selector);

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

    // compute length of nested array
    template<std::size_t Len>
    constexpr std::array<std::size_t, Len> make_inner_dim(std::size_t v) {
        std::array<std::size_t, Len> arr;
        for (size_t i = 0; i < Len; ++i) {
            arr[i] = v;
        }
        return arr;
    }

    template<std::size_t Len, bool one_side>
    constexpr std::array<std::size_t, Len> make_inner_dim(const BooleanOrTerms<one_side>& where_conditions) {
        std::array<std::size_t, Len> arr;
        for (size_t i = 0; i < Len; ++i) {
            arr[i] = where_conditions[i].size();
        }
        return arr;
    }

    // turning a vector of vector into a matrix, with the width of the matrix being the largest length
    template<std::array Lens, bool one_side>
    constexpr auto align_dnf(const BooleanOrTerms<one_side>& where_conditions) {
        constexpr std::size_t Len = Lens.size();
        constexpr std::size_t Cap = [](){
            if constexpr (Len != 0) {
                return *std::max_element(Lens.begin(), Lens.end());
            } else {
                return 0;
            }
        }();
        std::array<std::array<BooleanFactor<one_side>, Cap>, Len> aligned{};
        for (std::size_t i=0; i<Len; ++i) {
            for (std::size_t j=0; j < Lens[i]; ++j) {
                aligned[i][j] = where_conditions[i][j];
            }
        }
        return aligned;
    }

    template<Reflectable S1, Reflectable S2, bool one_side, std::array lhs_indices, std::array rhs_indices, std::array cop_list, std::array rhs_types, typename Vec, std::size_t... Idx>
    constexpr auto make_selector_or_cons_impl(const Vec& bfs, std::index_sequence<Idx...>) {
        return or_construct(make_selector<S1, S2, one_side, lhs_indices[Idx], rhs_indices[Idx], cop_list[Idx], rhs_types[Idx]>(bfs[Idx])...);
    }
    template<Reflectable S1, Reflectable S2, bool one_side, std::array lhs_indices, std::array rhs_indices, std::array cop_list, std::array rhs_types, std::size_t Len, typename Vec>
    constexpr auto make_selector_or_cons(const Vec& bfs) {
        return make_selector_or_cons_impl<S1, S2, one_side, lhs_indices, rhs_indices, cop_list, rhs_types>(bfs, std::make_index_sequence<Len>());
    }

    template<Reflectable S1, Reflectable S2, bool one_side, std::array lhs_indices, std::array rhs_indices, std::array cop_list, std::array rhs_types, typename Vec, std::size_t... Idx>
    constexpr auto make_selector_and_cons_impl(const Vec& bfs, std::index_sequence<Idx...>) {
        return and_construct(make_selector<S1, S2, one_side, lhs_indices[Idx], rhs_indices[Idx], cop_list[Idx], rhs_types[Idx]>(bfs[Idx])...);
    }
    template<Reflectable S1, Reflectable S2, bool one_side, std::array lhs_indices, std::array rhs_indices, std::array cop_list, std::array rhs_types, std::size_t Len, typename Vec>
    constexpr auto make_selector_and_cons(const Vec& bfs) {
        return make_selector_and_cons_impl<S1, S2, one_side, lhs_indices, rhs_indices, cop_list, rhs_types>(bfs, std::make_index_sequence<Len>());
    }

    template<Reflectable S1, Reflectable S2,
            bool one_side, std::array lhs_indices, std::array rhs_indices, std::array cop_list, std::array rhs_types, std::array Lens, typename Mat, std::size_t... Idx>
    constexpr auto make_cnf_selector_impl(const Mat& cnf, std::index_sequence<Idx...>) {
        return and_construct(make_selector_or_cons<S1, S2, one_side, lhs_indices[Idx], rhs_indices[Idx], cop_list[Idx], rhs_types[Idx], Lens[Idx]>(cnf[Idx])...);
    }

    template<Reflectable S1, Reflectable S2,
            bool one_side, std::array lhs_indices, std::array rhs_indices, std::array cop_list, std::array rhs_types, std::array Lens, typename Mat>
    constexpr auto make_cnf_selector(const Mat& cnf) {
        return make_cnf_selector_impl<S1, S2, one_side, lhs_indices, rhs_indices, cop_list, rhs_types, Lens>(cnf, std::make_index_sequence<Lens.size()>());
    }

    template<Reflectable S1, Reflectable S2,
            bool one_side, std::array lhs_indices, std::array rhs_indices, std::array cop_list, std::array rhs_types, std::array Lens, typename Mat, std::size_t... Idx>
    constexpr auto make_dnf_selector_impl(const Mat& cnf, std::index_sequence<Idx...>) {
        return or_construct(make_selector_and_cons<S1, S2, one_side, lhs_indices[Idx], rhs_indices[Idx], cop_list[Idx], rhs_types[Idx], Lens[Idx]>(cnf[Idx])...);
    }

    template<Reflectable S1, Reflectable S2,
            bool one_side, std::array lhs_indices, std::array rhs_indices, std::array cop_list, std::array rhs_types, std::array Lens, typename Mat>
    constexpr auto make_dnf_selector(const Mat& cnf) {
        return make_dnf_selector_impl<S1, S2, one_side, lhs_indices, rhs_indices, cop_list, rhs_types, Lens>(cnf, std::make_index_sequence<Lens.size()>());
    }

    // split a CNF matrix into (1) only table 1 (2) only table 2 (3) both
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

    // actually split the sifted matrix returned by sift
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

#endif //SQL_SELECTOR_H
