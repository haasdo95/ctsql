#ifndef SQL_PARSER_H
#define SQL_PARSER_H
#include <string>
#include <array>
#include <iostream>

#include <fmt/core.h>
#include <cassert>
#include "utils.h"
#include "refl.hpp"
#include "ctpg.hpp"

template<size_t N>
constexpr bool is_keyword(const char(&kw)[N], std::string_view w) {
    static_assert(N >= 1);  // null-termination
    if (w.size() + 1 != N) {
        return false;
    }
    for (size_t i=0; i<N-1; ++i) {
        if (to_upper(kw[i]) != to_upper(w[i])) {
            return false;
        }
    }
    return true;
}

static constexpr char sql_identifier_pattern[] = "[a-zA-Z][a-zA-Z0-9_]*";
static constexpr ctpg::regex_term<sql_identifier_pattern> sql_identifier("sql_identifier");


template<std::size_t NTables>
class FromParser {
public:
    struct TableName {
        std::string_view name;
        std::string_view alias;
        constexpr TableName() = default;
        constexpr TableName(std::string_view name, std::string_view alias): name{name}, alias{alias} {}
    };
    static constexpr ctpg::nterm<TableName> tab_name{"tab_name"};
    using TableNames = ctpg::stdex::cvector<TableName, NTables>;
    static constexpr ctpg::nterm<TableNames> tab_name_list{"tab_name_list"};
    static constexpr ctpg::parser p{
        tab_name_list,
        ctpg::terms(
            sql_identifier, ',', '\'', '\"'
        ),
        ctpg::nterms(
            tab_name_list,
            tab_name
        ),
        ctpg::rules(
            tab_name('\'', sql_identifier, '\'') >= [](char, std::string_view tn, char) { return TableName{tn, ""}; },
            tab_name('\"', sql_identifier, '\"') >= [](char, std::string_view tn, char) { return TableName{tn, ""}; },
            tab_name(sql_identifier) >= [](std::string_view tn) { return TableName{tn, ""}; },
            tab_name(tab_name, sql_identifier, sql_identifier) >=
                [](TableName name, std::string_view as, std::string_view alias){
                    if (not is_keyword("AS", as)) {
                        throw std::runtime_error("missing/misspelled AS keyword when aliasing");
                    }
                    name.alias = alias;
                    return name;
                },
            tab_name_list(tab_name) >= [](TableName tn) { return TableNames{tn, 1}; },
            tab_name_list(tab_name_list, ',', tab_name) >= [](TableNames table_names, char, TableName tn) {
                table_names.push_back(tn);
                return table_names;
            }
        )
    };
};


template<std::size_t NColsSel>
class SelectParser {
public:
    struct ColumnName {
        constexpr ColumnName(std::string_view table_name, std::string_view column_name, std::string_view alias, AggOp agg)
                : table_name{table_name}, column_name{column_name}, alias{alias}, agg{agg} {}
        constexpr ColumnName() = default;
        constexpr ColumnName(const ColumnName&) = default;
        std::string_view table_name;
        std::string_view column_name;
        std::string_view alias;
        AggOp agg{};
    };
    static constexpr ctpg::nterm<ColumnName> col_name{"col_name"};

    using ColumnNames = ctpg::stdex::cvector<ColumnName, NColsSel>;
    static constexpr ctpg::nterm<ColumnNames> col_name_list{"col_name_list"};

    static constexpr ctpg::parser p{
        col_name_list,
        ctpg::terms(
            sql_identifier, '.', ',', '*', '(', ')', '\'', '\"'
        ),
        ctpg::nterms(
            col_name_list,
            col_name
        ),
        ctpg::rules(
            col_name('\'', sql_identifier, '\'') >= [](char, std::string_view cn, char) { return ColumnName{"", cn, "", AggOp::NONE}; },
            col_name('\"', sql_identifier, '\"') >= [](char, std::string_view cn, char) { return ColumnName{"", cn, "", AggOp::NONE}; },
            col_name(sql_identifier) >= [](std::string_view cn) { return ColumnName{"", cn, "", AggOp::NONE}; },
            col_name(sql_identifier, '.', sql_identifier) >=
                [](std::string_view tn, char, std::string_view cn) { return ColumnName{tn, cn, "", AggOp::NONE}; },
            col_name(sql_identifier, '(', col_name, ')') >=
                [](std::string_view agg_op, char, ColumnName cname, char) {
                    AggOp agg{};
                    if (is_keyword("COUNT", agg_op)) {
                        agg = AggOp::COUNT;
                    } else if (is_keyword("SUM", agg_op)) {
                        agg = AggOp::SUM;
                    } else if (is_keyword("MAX", agg_op)) {
                        agg = AggOp::MAX;
                    } else if (is_keyword("MIN", agg_op)) {
                        agg = AggOp::MIN;
                    } else if (is_keyword("AVG", agg_op)) {
                        agg = AggOp::AVG;
                    } else {
                        throw std::runtime_error("unknown aggregating operator");
                    }
                    cname.agg = agg;
                    return cname;
                },
            col_name(col_name, sql_identifier, sql_identifier) >=
                [](ColumnName cname, std::string_view as, std::string_view alias) {
                    if (not is_keyword("AS", as)) {
                        throw std::runtime_error("missing/misspelled AS keyword when aliasing");
                    }
                    cname.alias = alias;
                    return cname;
                },
            col_name_list(sql_identifier, '(', '*', ')') >= [](std::string_view count_kw, char, char, char) {
                if (not is_keyword("COUNT", count_kw)) {
                    throw std::runtime_error("unknown/infeasible aggregation on \'*\'");
                }
                ColumnName cn{};
                cn.agg = AggOp::COUNT;
                ColumnNames cns{cn, 1};
                return cns;

            },
            col_name_list('*') >= [](char) { return ColumnNames{}; },
            col_name_list(col_name_list, ',', col_name) >=
                [](ColumnNames col_names, char, ColumnName cname) {
                    col_names.push_back(cname);
                    return col_names;
                },
            col_name_list(col_name) >=
                [](ColumnName cname) {
                    return ColumnNames{cname, 1};
                }
        )};
};


template<refl::const_string query>
class Query {
private:
    static constexpr std::size_t npos = decltype(query)::npos;
    static constexpr auto select_kw = refl::make_const_string("SELECT");
    static constexpr std::size_t select_pos = find_substr_case_insensitive(query, select_kw);
    static_assert(select_pos != npos);
    static constexpr std::size_t select_kw_end = select_pos + select_kw.size;
    static constexpr auto from_kw = refl::make_const_string("FROM");
    static constexpr std::size_t from_pos = find_substr_case_insensitive(query, from_kw);
    static_assert(from_pos != npos);
    static constexpr std::size_t from_kw_end = from_pos + from_kw.size;

    static constexpr auto column_names = query.template substr<select_kw_end, from_pos-select_kw_end>();
    static constexpr auto c_buf = ctpg::buffers::cstring_buffer(column_names.data);
    static constexpr std::size_t num_column_names = count_commas(column_names) + 1;
    static constexpr auto select_p = SelectParser<num_column_names>::p;

    static constexpr auto table_names = query.template substr<from_kw_end>();
    static constexpr auto t_buf = ctpg::buffers::cstring_buffer(table_names.data);
    static constexpr std::size_t num_table_names = count_commas(table_names) + 1;
    static constexpr auto from_p = FromParser<num_table_names>::p;

public:
    static constexpr auto select_res = select_p.parse(c_buf).value();
    static constexpr auto from_res = from_p.parse(t_buf).value();


};



//template<std::size_t NColsSel>
//class Parser {
//public:
////    using ColumnNames = ArrayLike<ColumnName, NColsSel>;
//    static constexpr ctpg::nterm<size_t> col_name_list{"col_name_list"};
//
//    static constexpr ctpg::parser p{
//            col_name_list,
//            ctpg::terms(
//                sql_identifier, '.', ',', '*'
//            ),
//            ctpg::nterms(
//                    col_name_list,
//                    col_name
//            ),
//            ctpg::rules(
//                    col_name(sql_identifier) >= [](std::string_view cn) { return 1; },
//                    col_name(sql_identifier, '.', sql_identifier) >=
//                        [](std::string_view tn, char, std::string_view cn) { return 1; },
//                    col_name_list('*') >= [](char) { return 0; },
//                    col_name_list(col_name) >= [](size_t cname) { return 1; },
//                    col_name_list(col_name_list, ',', col_name) >=
//                        [](size_t col_names, char, size_t cname) constexpr { return col_names + cname; }
//            )};
//};




#endif //SQL_PARSER_H
