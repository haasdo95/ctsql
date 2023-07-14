#ifndef SQL_SCHEMA_H
#define SQL_SCHEMA_H

#include "refl.hpp"
#include <concepts>

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

#endif //SQL_SCHEMA_H
