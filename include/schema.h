#ifndef SQL_SCHEMA_H
#define SQL_SCHEMA_H

#include "refl.hpp"
#include "parser/common.h"
#include <concepts>
#include <__generator.hpp>
#include <variant>
#include <tuple>

namespace ctsql
{
namespace impl {
    // CREDIT: https://stackoverflow.com/questions/28997271/c11-way-to-index-tuple-at-runtime-without-using-switch
    // a method to access tuple elements at run-time
    template<typename ...Ts, size_t ...Is>
    auto invoke_at_impl(std::tuple<Ts...>& tpl, std::index_sequence<Is...>, size_t idx)
    {
        return ((void)(Is == idx && (std::get<Is>(tpl), true)), ...);
        // for example: std::tuple<int, float, bool> `transformations` (idx == 1):
        //
        // Is... expansion    -> ((void)(0 == idx && (pred(std::get<0>(tpl)), true)), (void)(1 == idx && (pred(std::get<1>(tpl)), true)), (void)(2 == idx && (pred(std::get<2>(tpl)), true)));
        //                    -> ((void)(false && (pred(std::get<0>(tpl)), true)), (void)(true && (pred(std::get<1>(tpl)), true)), (void)(false && (pred(std::get<2>(tpl)), true)));
        // '&&' short-circuit -> ((void)(false), (void)(true && (pred(std::get<1>(tpl)), true)), (void)(false), true)));
        //
        // i.e. pred(std::get<1>(tpl) will be executed ONLY for idx == 1
    }

    template<typename ...Ts>
    auto invoke_at(std::tuple<Ts...>& tpl, size_t idx) {
        return invoke_at_impl(tpl, std::make_index_sequence<sizeof...(Ts)>{}, idx);
    }

    template<typename T>
    concept Reflectable = refl::is_reflectable<T>() or std::is_void_v<T>;

    // TODO: this is atrocious; can we do better?
    template<Reflectable Schema> requires std::default_initializable<Schema>
    using SchemaTuple = decltype(refl::util::map_to_tuple(refl::reflect<Schema>().members, [](auto td){
        return td(Schema{});
    }));

    template<Reflectable Schema>
    static constexpr auto member_list = refl::util::map_to_array<std::string_view>(refl::reflect<Schema>().members,
                                                                                   [](auto td){return td.name.str_view();});

    template<Reflectable Schema>
    constexpr std::size_t get_index(std::string_view column_name) {
        const auto& ml = member_list<Schema>;
        auto pos = std::find(ml.begin(), ml.end(), column_name);
        return std::distance(ml.begin(), pos);
    }

    template<Reflectable Schema>
    auto make_getter(std::string_view column_name) {
        const auto idx = get_index<Schema>(column_name);
        return [&](const auto& tuple){ std::get<idx>(tuple); };
    }

    template<Reflectable Schema>
    auto make_selector(BooleanFactor bf) {
        return [&](const SchemaTuple<Schema>& s) -> bool {
            const auto lhs = make_getter<Schema>(bf.lhs.column_name)(s);
            if (std::holds_alternative<BasicColumnName>(bf.rhs)) {
                const auto rhs = make_getter<Schema>(std::get<BasicColumnName>(bf.rhs).column_name)(s);
                const auto cop = to_operator<decltype(lhs), decltype(rhs)>(bf.cop);
                return cop(lhs, rhs);
            }
            throw std::runtime_error("not implemented");
        };
    }

    template<Reflectable Schema>
    constexpr auto resolve_name(std::string_view column_name) noexcept -> std::optional<refl::type_descriptor<Schema>> {
        constexpr auto schema_td = refl::reflect<Schema>();
        constexpr auto ml = refl::util::map_to_array<std::string_view>(schema_td.members, [](auto td){return td.name.str_view();});
        if (std::find(ml.begin(), ml.end(), column_name) != ml.end()) {
            return schema_td;
        } else {
            return std::nullopt;
        }
    }

    template<Reflectable S1, Reflectable S2>
    constexpr auto resolve_name(std::string_view column_name) -> std::variant<refl::type_descriptor<S1>, refl::type_descriptor<S2>> {
        auto td1 = resolve_name<S1>(column_name);
        auto td2 = resolve_name<S2>(column_name);
        if (td1 and td2) {
            throw std::runtime_error("ambiguous column name");
        } else if (!td1 and !td2) {
            throw std::runtime_error("cannot resolve column name");
        } else if (td1) {
            return td1.value();
        } else {
            return td2.value();
        }
    }

    // schema object => tuple with field values
    template<typename Schema>
    constexpr auto schema_to_tuple(const Schema& schema) -> SchemaTuple<Schema> {
        return refl::util::map_to_tuple(refl::reflect<Schema>().members, [&schema](auto td){
            return td(schema);
        });
    }
}
}



//template<typename Schema>
//struct Stream {
//    using type = Schema;
//    static constexpr std::array members = refl::util::map_to_array<std::string_view>(refl::reflect<Schema>().members, [](auto td){
//        return std::string_view(td.name.data, td.name.size);
//    });
//    std::generator<schema_tuple_type<Schema>> gen;
//};

#endif //SQL_SCHEMA_H
