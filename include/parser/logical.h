#ifndef SQL_LOGICAL_H
#define SQL_LOGICAL_H

#include "common.h"
#include "identifiers.h"
#include "numeral.h"

namespace ctsql {
namespace impl {
    // used in boolean expressions in WHERE clause as column reference
    static constexpr ctpg::nterm<BasicColumnName> basic_column_name{"basic_column_name"};
    static constexpr auto basic_column_name_rules = ctpg::rules(
            basic_column_name(sql_identifier) >= [](std::string_view cn) { return BasicColumnName("", cn); },
            basic_column_name(sql_identifier, '.', sql_identifier) >= [](std::string_view tn, char, std::string_view cn) { return BasicColumnName(tn, cn); }
    );

    static constexpr ctpg::char_term eq('=');
    static constexpr ctpg::char_term gt('>');
    static constexpr ctpg::char_term lt('<');
    static constexpr ctpg::string_term geq(">=");
    static constexpr ctpg::string_term leq("<=");
    static constexpr ctpg::string_term neq("<>");

    static constexpr ctpg::nterm<CompOp> comp_ops{"comp_ops"};
    static constexpr auto comp_op_rules = ctpg::rules(
        comp_ops(eq) >= [](char) { return CompOp::EQ; },
        comp_ops(gt) >= [](char) { return CompOp::GT; },
        comp_ops(lt) >= [](char) { return CompOp::LT; },
        comp_ops(geq) >= [](std::string_view) { return CompOp::GEQ; },
        comp_ops(leq) >= [](std::string_view) { return CompOp::LEQ; },
        comp_ops(neq) >= [](std::string_view) { return CompOp::NEQ; }
    );

    static constexpr ctpg::nterm<BooleanFactor> boolean_factor{"boolean_factor"};
    static constexpr auto boolean_factor_rules = ctpg::rules(
        boolean_factor(basic_column_name, comp_ops, basic_column_name) >=
            [](BasicColumnName lhs, CompOp cop, BasicColumnName rhs) { return BooleanFactor(cop, lhs, rhs); },
        boolean_factor(basic_column_name, comp_ops, string_literal) >=
            [](BasicColumnName lhs, CompOp cop, std::string_view rhs) { return BooleanFactor(cop, lhs, rhs); },
        boolean_factor(string_literal, comp_ops, basic_column_name) >=
            [](std::string_view rhs, CompOp cop, BasicColumnName lhs) { return BooleanFactor(invert(cop), lhs, rhs); },
        boolean_factor(basic_column_name, comp_ops, integer) >=
            [](BasicColumnName lhs, CompOp cop, int64_t rhs) { return BooleanFactor(cop, lhs, rhs); },
        boolean_factor(integer, comp_ops, basic_column_name) >=
            [](int64_t rhs, CompOp cop, BasicColumnName lhs) { return BooleanFactor(invert(cop), lhs, rhs); },
        boolean_factor(basic_column_name, comp_ops, floating_point) >=
            [](BasicColumnName lhs, CompOp cop, double rhs) { return BooleanFactor(cop, lhs, rhs); },
        boolean_factor(floating_point, comp_ops, basic_column_name) >=
            [](double rhs, CompOp cop, BasicColumnName lhs) { return BooleanFactor(invert(cop), lhs, rhs); },
        boolean_factor(not_kw, boolean_factor) >=
            [](std::string_view, BooleanFactor bf) { bf.cop = negate(bf.cop); return bf; }
    );

    static constexpr ctpg::nterm<BooleanAndTerms> boolean_and_terms{"boolean_and_terms"};
    static constexpr ctpg::nterm<BooleanOrTerms> boolean_or_terms{"boolean_or_terms"};
    static constexpr auto and_or_rules = ctpg::rules(
        boolean_and_terms(boolean_factor) >= [](BooleanFactor bf) { return BooleanAndTerms{bf, 1}; },
        boolean_and_terms(boolean_and_terms, and_kw, boolean_factor) >=
            [](BooleanAndTerms bat, std::string_view, BooleanFactor bf) { bat.push_back(bf); return bat; },
        boolean_or_terms(boolean_and_terms) >= [](BooleanAndTerms bat) { return BooleanOrTerms{bat, 1}; },
        boolean_or_terms(boolean_or_terms, or_kw, boolean_and_terms) >=
            [](BooleanOrTerms bot, std::string_view, BooleanAndTerms bat) { bot.push_back(bat); return bot; }
    );

}

namespace impl_exp {
    static constexpr auto logical_terms = ctpg::terms(impl::eq, impl::gt, impl::lt, impl::geq, impl::leq, impl::neq);
    static constexpr auto logical_nterms = ctpg::nterms(impl::basic_column_name, impl::comp_ops, impl::boolean_factor, impl::boolean_and_terms, impl::boolean_or_terms);
    static constexpr auto logical_rules = std::tuple_cat(impl::basic_column_name_rules, impl::comp_op_rules, impl::boolean_factor_rules, impl::and_or_rules);
}

}





//static constexpr ctpg::nterm<double> numeral{"numeral"};
//static constexpr auto numeral_rules = ctpg::rules(
//    numeral(floating_point) >= [](double v) { return v; },
//    numeral(integer) >= [](int64_t v) { return static_cast<double>(v); }
//);


#endif //SQL_LOGICAL_H
