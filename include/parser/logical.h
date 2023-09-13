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
            basic_column_name(single_entity_reference, '.', sql_identifier) >= [](std::string_view tn, char, std::string_view cn) { return BasicColumnName(tn, cn); }
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

    static constexpr ctpg::nterm<BooleanFactor<true>> boolean_factor_one_side{"boolean_factor_one_side"};
    static constexpr ctpg::nterm<BooleanFactor<false>> boolean_factor_two_side{"boolean_factor_two_side"};
    static constexpr auto boolean_factor_rules = ctpg::rules(
        boolean_factor_one_side(basic_column_name, comp_ops, string_literal) >=
            [](BasicColumnName lhs, CompOp cop, std::string_view rhs) { return BooleanFactor<true>(cop, lhs, rhs); },
        boolean_factor_one_side(string_literal, comp_ops, basic_column_name) >=
            [](std::string_view rhs, CompOp cop, BasicColumnName lhs) { return BooleanFactor<true>(invert(cop), lhs, rhs); },
        boolean_factor_one_side(basic_column_name, comp_ops, integer) >=
            [](BasicColumnName lhs, CompOp cop, int64_t rhs) { return BooleanFactor<true>(cop, lhs, rhs); },
        boolean_factor_one_side(integer, comp_ops, basic_column_name) >=
            [](int64_t rhs, CompOp cop, BasicColumnName lhs) { return BooleanFactor<true>(invert(cop), lhs, rhs); },
        boolean_factor_one_side(basic_column_name, comp_ops, floating_point) >=
            [](BasicColumnName lhs, CompOp cop, double rhs) { return BooleanFactor<true>(cop, lhs, rhs); },
        boolean_factor_one_side(floating_point, comp_ops, basic_column_name) >=
            [](double rhs, CompOp cop, BasicColumnName lhs) { return BooleanFactor<true>(invert(cop), lhs, rhs); },
        boolean_factor_one_side(not_kw, boolean_factor_one_side) >=
            [](std::string_view, BooleanFactor<true> bf) { bf.cop = negate(bf.cop); return bf; },
        boolean_factor_two_side(not_kw, boolean_factor_two_side) >=
            [](std::string_view, BooleanFactor<false> bf) { bf.cop = negate(bf.cop); return bf; },
        boolean_factor_two_side(basic_column_name, comp_ops, basic_column_name) >=
            [](BasicColumnName lhs, CompOp cop, BasicColumnName rhs) { return BooleanFactor<false>(cop, lhs, rhs); }
    );

    static constexpr ctpg::nterm<BooleanAndTerms<true>> boolean_and_terms_one_side{"boolean_and_terms_one_side"};
    static constexpr ctpg::nterm<BooleanAndTerms<false>> boolean_and_terms_two_side{"boolean_and_terms_two_side"};
    static constexpr ctpg::nterm<BooleanOrTerms<true>> boolean_or_terms_one_side{"boolean_or_terms_one_side"};
    static constexpr ctpg::nterm<BooleanOrTerms<false>> boolean_or_terms_two_side{"boolean_or_terms_two_side"};
    static constexpr auto and_or_rules = ctpg::rules(
        boolean_and_terms_one_side(boolean_factor_one_side) >= [](BooleanFactor<true> bf) { return BooleanAndTerms<true>{bf, 1}; },
        boolean_and_terms_one_side(boolean_and_terms_one_side, and_kw, boolean_factor_one_side) >=
            [](BooleanAndTerms<true> bat, std::string_view, BooleanFactor<true> bf) { bat.push_back(bf); return bat; },
        boolean_or_terms_one_side(boolean_and_terms_one_side) >= [](BooleanAndTerms<true> bat) { return BooleanOrTerms<true>{bat, 1}; },
        boolean_or_terms_one_side(boolean_or_terms_one_side, or_kw, boolean_and_terms_one_side) >=
            [](BooleanOrTerms<true> bot, std::string_view, BooleanAndTerms<true> bat) { bot.push_back(bat); return bot; },

        boolean_and_terms_two_side(boolean_factor_two_side) >= [](BooleanFactor<false> bf) { return BooleanAndTerms<false>{bf, 1}; },
        boolean_and_terms_two_side(boolean_and_terms_two_side, and_kw, boolean_factor_two_side) >=
            [](BooleanAndTerms<false> bat, std::string_view, BooleanFactor<false> bf) { bat.push_back(bf); return bat; },
        boolean_or_terms_two_side(boolean_and_terms_two_side) >= [](BooleanAndTerms<false> bat) { return BooleanOrTerms<false>{bat, 1}; },
        boolean_or_terms_two_side(boolean_or_terms_two_side, or_kw, boolean_and_terms_two_side) >=
            [](BooleanOrTerms<false> bot, std::string_view, BooleanAndTerms<false> bat) { bot.push_back(bat); return bot; }
    );

}

namespace impl_exp {
    static constexpr auto logical_terms = ctpg::terms(impl::eq, impl::gt, impl::lt, impl::geq, impl::leq, impl::neq);
    static constexpr auto logical_nterms = ctpg::nterms(impl::basic_column_name, impl::comp_ops,
                                                        impl::boolean_factor_one_side, impl::boolean_factor_two_side,
                                                        impl::boolean_and_terms_one_side, impl::boolean_and_terms_two_side,
                                                        impl::boolean_or_terms_one_side, impl::boolean_or_terms_two_side);
    static constexpr auto logical_rules = std::tuple_cat(impl::basic_column_name_rules, impl::comp_op_rules, impl::boolean_factor_rules, impl::and_or_rules);
}

}


#endif //SQL_LOGICAL_H
