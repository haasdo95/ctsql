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

    template<Reflectable Schema>
    static constexpr auto member_list = refl::util::map_to_array<std::string_view>(refl::reflect<Schema>().members,
                                                                                   [](auto td){return td.name.str_view();});

    template<Reflectable Schema>
    constexpr std::size_t get_index(std::string_view column_name) {
        const auto& ml = member_list<Schema>;
        auto pos = std::find(ml.begin(), ml.end(), column_name);
        return std::distance(ml.begin(), pos);
    }

    template<Reflectable Schema, std::size_t N>
    constexpr auto make_lhs_indices(const std::array<BooleanFactor, N>& bfs) {
        std::array<std::size_t, N> indices;
        for (size_t i=0; i<N; ++i) {
            indices[i] = get_index<Schema>(bfs[i].lhs.column_name);
        }
        return indices;
    }

    template<std::size_t N>
    constexpr auto make_cop_list(const std::array<BooleanFactor, N>& bfs) {
        std::array<CompOp, N> cops;
        for (size_t i=0; i<N; ++i) {
            cops[i] = bfs[i].cop;
        }
        return cops;
    }

    template<std::size_t N>
    constexpr auto make_rhs_type_list(const std::array<BooleanFactor, N>& bfs) {
        std::array<RHSTypeTag, N> rhs_types;
        for (size_t i=0; i<N; ++i) {
            if (std::holds_alternative<int64_t>(bfs[i].rhs)) {
                rhs_types[i] = RHSTypeTag::INT;
            } else if (std::holds_alternative<double>(bfs[i].rhs)) {
                rhs_types[i] = RHSTypeTag::DOUBLE;
            } else if (std::holds_alternative<std::string_view>(bfs[i].rhs)) {
                rhs_types[i] = RHSTypeTag::STRING;
            } else {
                throw std::runtime_error("non-literal RHS in where conditions is not supported");
            }
        }
        return rhs_types;
    }

    // TODO: we enforce the simplifying rule that the rhs of a where-condition has to be a literal
    template<Reflectable Schema, size_t lhs_idx, CompOp cop, RHSTypeTag rhs_type>
    constexpr auto make_selector(const BooleanFactor& bf) {
        if (std::holds_alternative<BasicColumnName>(bf.rhs)) {
            throw std::runtime_error("non-literal LHS of where conditions is not yet supported");
        }
        const auto comp_f = to_operator<cop>();
        using RHS = std::conditional_t<rhs_type==RHSTypeTag::INT,
                                       int64_t,
                                       std::conditional_t<rhs_type==RHSTypeTag::DOUBLE, double, std::string_view>>;
        return [comp_f, &bf](const SchemaTuple<Schema>& s) -> bool {
            return comp_f(std::get<lhs_idx>(s), std::get<RHS>(bf.rhs));
        };
    }

    template<Reflectable Schema, std::array lhs_indices, std::array cop_list, std::array rhs_types, std::size_t N, std::size_t... Idx>
    constexpr auto make_selectors_impl(const std::array<BooleanFactor, N>& bfs, std::index_sequence<Idx...>) {
        return std::make_tuple(make_selector<Schema, lhs_indices[Idx], cop_list[Idx], rhs_types[Idx]>(bfs[Idx])...);
    }

    template<Reflectable Schema, std::array lhs_indices, std::array cop_list, std::array rhs_types, std::size_t N>
    constexpr auto make_selectors(const std::array<BooleanFactor, N>& bfs) {
        return make_selectors_impl<Schema, lhs_indices, cop_list, rhs_types>(bfs, std::make_index_sequence<N>());
    }

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

}
}

#endif //SQL_SCHEMA_H
