#ifndef SQL_PREPROC_H
#define SQL_PREPROC_H

#include "common.h"
#include "schema.h"

namespace ctsql {
namespace impl {
    constexpr auto dealias_query(Query query) {
        if (not query.tns.first.alias.empty()
            and query.tns.second and query.tns.first.alias==query.tns.second.value().alias) {
            throw std::runtime_error("two tables are receiving the same alias");
        }
        auto substitute = [&query](auto& cn) {
            if (not cn.table_name.empty()) {
                if (query.tns.first.alias == cn.table_name) {
                    cn.table_name = query.tns.first.name;
                } else if (query.tns.second and query.tns.second.value().alias == cn.table_name) {
                    cn.table_name = query.tns.second.value().name;
                }
            }
        };
        // de-alias select-list
        std::for_each(query.cns.begin(), query.cns.end(), substitute);
        // de-alias condition lists
        for (auto& bf: query.join_condition) {
            substitute(bf.lhs);
            if (std::holds_alternative<BasicColumnName>(bf.rhs)) {
                substitute(std::get<BasicColumnName>(bf.rhs));
            } else {
                throw std::runtime_error("please only use JOIN-ON clause for JOIN conditions");
            }
        }
        for (auto& bat: query.where_condition) {
            for (auto& bf: bat) {
                substitute(bf.lhs);
                if (std::holds_alternative<BasicColumnName>(bf.rhs)) {
                    substitute(std::get<BasicColumnName>(bf.rhs));
                }
            }
        }
        return query;
    }

    template<Reflectable S1, Reflectable S2=void>
    constexpr auto resolve_table_name(Query query) {
        auto substitute = [&query](auto& cn) {
            constexpr std::string_view s1_name = refl::reflect<S1>().name.str_view();
            if constexpr (std::is_void_v<S2>) {  // only one table
                cn.table_name = s1_name;
            } else {
                constexpr std::string_view s2_name = refl::reflect<S2>().name.str_view();
                if (cn.table_name.empty()) {  // no disambiguation available
                    const auto table_td = resolve_name<S1, S2>(cn.column_name);
                    const std::string_view tab_name = std::visit([](const auto& td){ return td.name.str_view(); }, table_td);
                    cn.table_name = tab_name;
                } else {  // explicit disambiguator
                    if (query.tns.first.name == cn.table_name) {
                        cn.table_name = s1_name;
                    } else if (query.tns.second.value().name == cn.table_name) {
                        cn.table_name = s2_name;
                    } else {
                        throw std::runtime_error("cannot resolve table reference");
                    }
                }
            }
        };
        // de-alias select-list
        std::for_each(query.cns.begin(), query.cns.end(), substitute);
        // de-alias condition lists
        for (auto& bf: query.join_condition) {
            substitute(bf.lhs);
            substitute(std::get<BasicColumnName>(bf.rhs));
        }
        for (auto& bat: query.where_condition) {
            for (auto& bf: bat) {
                substitute(bf.lhs);
                if (std::holds_alternative<BasicColumnName>(bf.rhs)) {
                    substitute(std::get<BasicColumnName>(bf.rhs));
                }
            }
        }

        // fix table names at last
        query.tns.first.name = refl::reflect<S1>().name.str_view();
        if (query.tns.second) {
            if (std::is_void_v<S2>) {
                throw std::runtime_error("2 tables in SQL; only 1 in template parameter");
            }
            query.tns.second.value().name = refl::reflect<S2>().name.str_view();
        } else if (not std::is_void_v<S2>) {
            throw std::runtime_error("2 tables in template parameter; only 1 in SQL");
        }
        return query;
    }

}
}



#endif //SQL_PREPROC_H
