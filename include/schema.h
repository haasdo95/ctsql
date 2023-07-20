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
    // CREDIT: https://stackoverflow.com/questions/28997271/c11-way-to-index-tuple-at-runtime-without-using-switch
    // a method to access tuple elements at run-time
    template<typename TPred, typename ...Ts, size_t ...Is>
    void invoke_at_impl(const std::tuple<Ts...>& tpl, std::index_sequence<Is...>, size_t idx, TPred pred)
    {
        ((void)(Is == idx && (pred(std::get<Is>(tpl)), true)), ...);
        // for example: std::tuple<int, float, bool> `transformations` (idx == 1):
        //
        // Is... expansion    -> ((void)(0 == idx && (pred(std::get<0>(tpl)), true)), (void)(1 == idx && (pred(std::get<1>(tpl)), true)), (void)(2 == idx && (pred(std::get<2>(tpl)), true)));
        //                    -> ((void)(false && (pred(std::get<0>(tpl)), true)), (void)(true && (pred(std::get<1>(tpl)), true)), (void)(false && (pred(std::get<2>(tpl)), true)));
        // '&&' short-circuit -> ((void)(false), (void)(true && (pred(std::get<1>(tpl)), true)), (void)(false), true)));
        //
        // i.e. pred(std::get<1>(tpl) will be executed ONLY for idx == 1
    }

    template<typename TPred, typename ...Ts>
    void invoke_at(const std::tuple<Ts...>& tpl, size_t idx, TPred pred)
    {
        invoke_at_impl(tpl, std::make_index_sequence<sizeof...(Ts)>{}, idx, pred);
    }

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

    // TODO: we enforce the simplifying rule that the rhs of a where-condition has to be a literal
    template<Reflectable Schema>
    constexpr auto make_selector(const BooleanFactor& bf) {
        if (std::holds_alternative<BasicColumnName>(bf.rhs)) {
            throw std::runtime_error("non-literal LHS of where conditions is not yet supported");
        }
        auto make_pred = [&bf](bool& result) {
            auto pred = [&result, &bf](const auto& lhs_val) {
                std::visit([&result, &bf, &lhs_val](const auto& rhs_val){
                    if constexpr (std::three_way_comparable_with<decltype(lhs_val), decltype(rhs_val)>) {
                        switch (bf.cop) {
                            case CompOp::EQ:
                                result = (lhs_val == rhs_val);
                                break;
                            case CompOp::GT:
                                result = (lhs_val > rhs_val);
                                break;
                            case CompOp::LT:
                                result = (lhs_val < rhs_val);
                                break;
                            case CompOp::GEQ:
                                result = (lhs_val >= rhs_val);
                                break;
                            case CompOp::LEQ:
                                result = (lhs_val <= rhs_val);
                                break;
                            default:
                                result = (lhs_val != rhs_val);
                                break;
                        }
                    }
                }, bf.rhs);
            };
            return pred;
        };
        const std::size_t lhs_idx = get_index<Schema>(bf.lhs.column_name);
        return [lhs_idx, make_pred](const SchemaTuple<Schema>& s) -> bool {
            bool res = false;
            auto pred = make_pred(res);
            invoke_at(s, lhs_idx, pred);
            return res;
        };
    }

    template<typename Schema>
    std::function<bool(const SchemaTuple<Schema>&)> and_construct(const BooleanAndTerms& bat) {
        std::function<bool(const SchemaTuple<Schema>&)> and_selector = [](const auto&){ return true; };
        for (const auto& bf: bat) {
            and_selector = [and_selector=std::move(and_selector), selector=make_selector<Schema>(bf)](const auto& tuple){
                return selector(tuple) and and_selector(tuple);
            };
        }
        return and_selector;
    }

    template<typename Schema>
    std::function<bool(const SchemaTuple<Schema>&)> or_construct(const BooleanOrTerms& bot) {
        if (bot.empty()) { return [](const auto&){ return true; }; }  // empty condition -> take all
        std::function<bool(const SchemaTuple<Schema>&)> or_selector = [](const auto&){ return false; };
        for (const auto& bat: bot) {
            or_selector = [or_selector=std::move(or_selector), and_selector=and_construct<Schema>(bat)](const auto& tuple){
                return and_selector(tuple) or or_selector(tuple);
            };
        }
        return or_selector;
    }

//    template<typename... Selectors>
//    auto and_construct(Selectors... selector);
//
//    template<typename Selector, typename... Selectors>
//    auto and_construct(Selector s, Selectors... selectors) {
//        const auto rec = and_construct(selectors...);
//        return [s, rec](auto tuple){ return s(tuple) and rec(tuple); };
//    }
//
//    template<>
//    auto and_construct() {
//        return [](auto tuple) { return true; };
//    }



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
    constexpr auto resolve_name(std::string_view column_name) -> const char* {
        auto td1 = resolve_name<S1>(column_name);
        auto td2 = resolve_name<S2>(column_name);
        if (td1 and td2) {
            throw std::runtime_error("ambiguous column name");
        } else if (!td1 and !td2) {
            throw std::runtime_error("cannot resolve column name");
        } else if (td1) {
            return "0";
        } else {
            return "1";
        }
    }
}
}

#endif //SQL_SCHEMA_H
