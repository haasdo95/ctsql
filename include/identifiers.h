#ifndef SQL_IDENTIFIERS_H
#define SQL_IDENTIFIERS_H

#include "common.h"
#include "keywords.h"

namespace ctsql
{
namespace impl {
    static constexpr char string_literal_double_quote_pattern[] = R"("[^"]*")";
    static constexpr ctpg::regex_term<string_literal_double_quote_pattern> string_literal_double_quote("string_literal_double_quote");
    static constexpr char string_literal_single_quote_pattern[] = R"('[^']*')";
    static constexpr ctpg::regex_term<string_literal_single_quote_pattern> string_literal_single_quote("string_literal_single_quote");
    static constexpr ctpg::nterm<std::string_view> string_literal("string_literal");

    static constexpr char sql_identifier_pattern[] = "[a-zA-Z][a-zA-Z0-9_]*";
    static constexpr ctpg::regex_term<sql_identifier_pattern> sql_identifier("sql_identifier");

    static constexpr ctpg::nterm<ColumnName> col_name_no_alias_no_agg_no_table_name{"col_name_no_alias_no_agg_no_table_name"};
    static constexpr ctpg::nterm<ColumnName> col_name_no_alias_no_agg{"col_name_no_alias_no_agg"};
    static constexpr ctpg::nterm<ColumnName> col_name_no_alias{"col_name_no_alias"};
    static constexpr ctpg::nterm<ColumnName> col_name{"col_name"};
    static constexpr ctpg::nterm<ColumnNames> col_name_list{"col_name_list"};

    static constexpr auto str_lit_rules = ctpg::rules(
        string_literal(string_literal_double_quote) >= [](std::string_view cn){ return cn.substr(1, cn.size()-2); },
        string_literal(string_literal_single_quote) >= [](std::string_view cn){ return cn.substr(1, cn.size()-2); }
    );

    // captures Student.name & name
    static constexpr auto col_name_no_alias_no_agg_rules = ctpg::rules(
        col_name_no_alias_no_agg_no_table_name(string_literal) >= [](std::string_view cn) { return ColumnName("", cn, "", AggOp::NONE); },
        col_name_no_alias_no_agg_no_table_name(sql_identifier) >= [](std::string_view cn) { return ColumnName{"", cn, "", AggOp::NONE}; },
        col_name_no_alias_no_agg(col_name_no_alias_no_agg_no_table_name) >= [](ColumnName cname){ return cname; },
        col_name_no_alias_no_agg(sql_identifier, '.', sql_identifier) >= [](std::string_view tn, char, std::string_view cn) { return ColumnName{tn, cn, "", AggOp::NONE}; }
    );

    // additionally captures things like AVG(Student.age)
    static constexpr auto col_name_no_alias_rules = ctpg::rules(
        col_name_no_alias(col_name_no_alias_no_agg) >= [](ColumnName cname) { return cname; },
        col_name_no_alias(agg_kws, '(', col_name_no_alias_no_agg, ')') >=
            [](AggOp agg, char, ColumnName cname, char) {
                cname.agg = agg;
                return cname;
            }
    );

    // handling AS-clause for aliasing
    static constexpr auto col_name_rules = ctpg::rules(
        col_name(col_name_no_alias) >= [](ColumnName cname) { return cname; },
        col_name(col_name_no_alias, col_name_no_alias_no_agg_no_table_name) >=
            [](ColumnName cname, ColumnName alias) {
                cname.alias = alias.column_name;
                return cname;
            },
        col_name(col_name_no_alias, as_kw, col_name_no_alias_no_agg_no_table_name) >=
            [](ColumnName cname, std::string_view, ColumnName alias) {
                cname.alias = alias.column_name;
                return cname;
            }
    );

    // list of column names
    static constexpr auto col_name_list_rules = ctpg::rules(
        col_name_list(count_kw, '(', '*', ')') >=
            [](std::string_view, char, char, char) {
                ColumnName cn{};
                cn.agg = AggOp::COUNT;
                return ColumnNames{cn, 1};
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
    );

    // list of tables in FROM clause
    static constexpr ctpg::nterm<TableName> tab_name_no_alias{"tab_name_no_alias"};
    static constexpr ctpg::nterm<TableName> tab_name{"tab_name"};
    static constexpr ctpg::nterm<TableNames> tab_name_list{"tab_name_list"};

    static constexpr auto tab_name_rules = ctpg::rules(
        tab_name_no_alias(string_literal) >= [](std::string_view tn) { return TableName{tn, ""}; },
        tab_name_no_alias(sql_identifier) >= [](std::string_view tn) { return TableName{tn, ""}; },
        tab_name(tab_name_no_alias) >= [](TableName tn) { return tn; },
        tab_name(tab_name_no_alias, as_kw, tab_name_no_alias) >=
        [](TableName name, std::string_view, TableName alias_tn){
            name.alias = alias_tn.name;
            return name;
        },
        tab_name(tab_name_no_alias, tab_name_no_alias) >=
        [](TableName name, TableName alias_tn){
            name.alias = alias_tn.name;
            return name;
        },
        tab_name_list(tab_name) >= [](TableName tn) { return TableNames{tn, 1}; },
        tab_name_list(tab_name_list, ',', tab_name) >= [](TableNames table_names, char, TableName tn) {
            table_names.push_back(tn);
            return table_names;
        }
    );
}

namespace impl_exp {
    static constexpr auto ident_terms = ctpg::terms(impl::string_literal_double_quote, impl::string_literal_single_quote, impl::sql_identifier, '.', '(', ')', ',', '*');
    static constexpr auto ident_nterms = ctpg::nterms(impl::string_literal, impl::col_name_list, impl::col_name,
                                                      impl::col_name_no_alias, impl::col_name_no_alias_no_agg,
                                                      impl::col_name_no_alias_no_agg_no_table_name,
                                                      impl::tab_name, impl::tab_name_no_alias, impl::tab_name_list);
    static constexpr auto ident_rules = std::tuple_cat(impl::str_lit_rules,
                                                       impl::col_name_no_alias_no_agg_rules,
                                                       impl::col_name_no_alias_rules,
                                                       impl::col_name_rules,
                                                       impl::col_name_list_rules,
                                                       impl::tab_name_rules);
}

}


#endif //SQL_IDENTIFIERS_H
