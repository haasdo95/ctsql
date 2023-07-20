#ifndef SQL_PREPROC_H
#define SQL_PREPROC_H

#include "common.h"
#include "schema.h"

namespace ctsql {
namespace impl {
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

    constexpr auto dealias_query(Query query) {
        if (not query.tns.first.alias.empty()
            and query.tns.second and query.tns.first.alias==query.tns.second.value().alias) {
            throw std::runtime_error("two tables are receiving the same alias");
        }
        auto substitute = [&query](auto& cn) {
            if (not cn.table_name.empty()) {
                if (query.tns.first.alias == cn.table_name or query.tns.first.name == cn.table_name) {
                    cn.table_name = "0";
                } else if (query.tns.second and (query.tns.second.value().alias == cn.table_name or query.tns.second.value().name == cn.table_name)) {
                    cn.table_name = "1";
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
                    throw std::runtime_error("only literal values are allowed as RHS of where conditions");
                }
            }
        }
        return query;
    }

    template<Reflectable S1, Reflectable S2=void>
    constexpr auto resolve_table_name(Query query) {
        auto substitute = [&query](auto& cn) {
            if constexpr (std::is_void_v<S2>) {  // only one table
                cn.table_name = "0";
            } else {
                if (cn.table_name.empty()) {  // no disambiguation available
                    cn.table_name = resolve_name<S1, S2>(cn.column_name);;
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
                    throw std::runtime_error("only literal values are allowed as RHS of where conditions");
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

    template<std::size_t N>
    constexpr void increment_carrying_indices(std::array<std::size_t, N>& indices, const std::array<std::size_t, N>& limits) {
        for (size_t i = 0; i < N; ++i) {
            if (indices[i] + 1 < limits[i]) {
                ++indices[i];
                return;
            } else {
                indices[i] = 0;
            }
        }
    }

    constexpr std::size_t compute_number_of_cnf_clauses(const BooleanOrTerms& dnf) {
        size_t num_cnf_clauses = 1;
        std::for_each(dnf.begin(), dnf.end(), [&num_cnf_clauses](const auto& bat){ num_cnf_clauses *= bat.size();});
        return num_cnf_clauses;
    }

    // num_terms_in_each_cnf_clause == dnf.size()
    template<std::size_t num_cnf_clauses, std::size_t num_terms_in_each_cnf_clause>
    constexpr auto dnf_to_cnf(const BooleanOrTerms& dnf) -> std::array<std::array<BooleanFactor, num_terms_in_each_cnf_clause>, num_cnf_clauses> {
        std::array<std::size_t, num_terms_in_each_cnf_clause> indices{};
        const std::array<std::size_t, num_terms_in_each_cnf_clause> limits = [&dnf](){
            std::array<std::size_t, num_terms_in_each_cnf_clause> arr{};
            for (size_t i = 0; i < num_terms_in_each_cnf_clause; ++i) {
                arr[i] = dnf[i].size();
            }
            return arr;
        }();

        auto extract = [&dnf](const std::array<std::size_t, num_terms_in_each_cnf_clause>& indices, std::array<BooleanFactor, num_terms_in_each_cnf_clause>& clause) {
            for (size_t i = 0; i < num_terms_in_each_cnf_clause; ++i) {
                clause[i] = dnf[i][indices[i]];
            }
        };

        std::array<std::array<BooleanFactor, num_terms_in_each_cnf_clause>, num_cnf_clauses> result;
        for (size_t result_idx=0; result_idx<num_cnf_clauses; ++result_idx) {
            extract(indices, result[result_idx]);
            increment_carrying_indices(indices, limits);
        }
        return result;
    }
}
}



#endif //SQL_PREPROC_H
