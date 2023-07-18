#ifndef SQL_PARSER_H
#define SQL_PARSER_H
#include <string>
#include <array>
#include <iostream>

#include <cassert>
#include "refl.hpp"
#include "logical.h"
#include "keywords.h"
#include "identifiers.h"
#include "numeral.h"

namespace ctsql {
    static constexpr ctpg::nterm<Query> query{"query"};
    static constexpr auto query_rules = ctpg::rules(
        query(impl::select_kw, impl::col_name_list,
              impl::from_kw, impl::tab_name, ',', impl::tab_name,
              impl::on_kw, impl::boolean_and_terms,
              impl::where_kw, impl::boolean_or_terms) >=
            [](std::string_view, ColumnNames cns, std::string_view, TableName tn1, char, TableName tn2, std::string_view, BooleanAndTerms bat, std::string_view, BooleanOrTerms bot) {
                return Query(cns, TableNames{tn1, tn2}, bat, bot);
            },
        query(impl::select_kw, impl::col_name_list,
              impl::from_kw, impl::tab_name, ',', impl::tab_name,
              impl::on_kw, impl::boolean_and_terms) >=
            [](std::string_view, ColumnNames cns, std::string_view, TableName tn1, char, TableName tn2, std::string_view, BooleanAndTerms bat) {
                return Query(cns, TableNames{tn1, tn2}, bat, BooleanOrTerms{});
            },
        query(impl::select_kw, impl::col_name_list, impl::from_kw, impl::tab_name_list, impl::where_kw, impl::boolean_or_terms) >=
            [](std::string_view, ColumnNames cns, std::string_view, TableNames tns, std::string_view, BooleanOrTerms bot) {
                return Query(cns, tns, BooleanAndTerms{}, bot);
            },
        query(impl::select_kw, impl::col_name_list, impl::from_kw, impl::tab_name_list) >=
            [](std::string_view, ColumnNames cns, std::string_view, TableNames tns) {
                return Query(cns, tns, BooleanAndTerms{}, BooleanOrTerms{});
            }
    );

    class SelectParser {
    public:
        static constexpr ctpg::parser p {
            query,
            std::tuple_cat(impl_exp::kw_terms, impl_exp::ident_terms, impl_exp::numeral_terms, impl_exp::logical_terms),
            std::tuple_cat(impl_exp::kw_nterms, impl_exp::ident_nterms, impl_exp::numeral_nterms, impl_exp::logical_nterms, ctpg::nterms(query)),
            std::tuple_cat(impl_exp::kw_rules, impl_exp::ident_rules, impl_exp::numeral_rules, impl_exp::logical_rules, query_rules)
        };
    };

}

#endif //SQL_PARSER_H