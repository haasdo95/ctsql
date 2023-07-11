#ifndef SQL_LOGICAL_H
#define SQL_LOGICAL_H

#include "common.h"

namespace ctsql {
static constexpr ctpg::char_term eq('=');
static constexpr ctpg::char_term gt('>');
static constexpr ctpg::char_term lt('<');
static constexpr ctpg::string_term geq(">=");
static constexpr ctpg::string_term leq("<=");
static constexpr ctpg::string_term neq("<>");


static constexpr ctpg::nterm<CompOp> comp_ops{"comp_ops"};
static constexpr auto comp_op_rules = ctpg::rules(
    comp_ops(eq) >= [](std::string_view) { return CompOp::EQ; },
    comp_ops(gt) >= [](std::string_view) { return CompOp::GT; },
    comp_ops(lt) >= [](std::string_view) { return CompOp::LT; },
    comp_ops(geq) >= [](std::string_view) { return CompOp::GEQ; },
    comp_ops(leq) >= [](std::string_view) { return CompOp::LEQ; },
    comp_ops(neq) >= [](std::string_view) { return CompOp::NEQ; }
);
}





//static constexpr ctpg::nterm<double> numeral{"numeral"};
//static constexpr auto numeral_rules = ctpg::rules(
//    numeral(floating_point) >= [](double v) { return v; },
//    numeral(integer) >= [](int64_t v) { return static_cast<double>(v); }
//);


#endif //SQL_LOGICAL_H
