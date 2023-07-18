#ifndef SQL_SCHEMA_H
#define SQL_SCHEMA_H

#include "refl.hpp"
#include <concepts>
#include <__generator.hpp>
#include <variant>

template<typename T>
concept Reflectable = refl::is_reflectable<T>() or std::is_void_v<T>;

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



template<std::default_initializable Schema>
using schema_tuple_type = decltype(refl::util::map_to_tuple(refl::reflect<Schema>().members, [](auto td){
    return td(Schema{});
}));

// schema object => tuple with field values
template<typename Schema>
constexpr auto schema_to_tuple(const Schema& schema) -> schema_tuple_type<Schema> {
    return refl::util::map_to_tuple(refl::reflect<Schema>().members, [&schema](auto td){
        return td(schema);
    });
}

template<typename Schema>
struct Stream {
    using type = Schema;
    static constexpr std::array members = refl::util::map_to_array<std::string_view>(refl::reflect<Schema>().members, [](auto td){
        return std::string_view(td.name.data, td.name.size);
    });
    std::generator<schema_tuple_type<Schema>> gen;
};

#endif //SQL_SCHEMA_H
