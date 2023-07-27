#ifndef SQL_KEYWORDS_H
#define SQL_KEYWORDS_H

#include "common.h"

namespace ctsql {
namespace impl {
    static constexpr char select_pattern[] = "[sS][eE][lL][eE][cC][tT]";
    static constexpr ctpg::regex_term<select_pattern> select_kw("select_kw");

    static constexpr char as_pattern[] = "[aA][sS]";
    static constexpr ctpg::regex_term<as_pattern> as_kw("as_kw");

    static constexpr char from_pattern[] = "[fF][rR][oO][mM]";
    static constexpr ctpg::regex_term<from_pattern> from_kw("from_kw");

    static constexpr char where_pattern[] = "[wW][hH][eE][rR][eE]";
    static constexpr ctpg::regex_term<where_pattern> where_kw("where_kw");

    static constexpr char on_pattern[] = "[oO][nN]";
    static constexpr ctpg::regex_term<on_pattern> on_kw("on_kw");

    static constexpr char count_pattern[] = "[cC][oO][uU][nN][tT]";
    static constexpr ctpg::regex_term<count_pattern> count_kw("count_kw");

    static constexpr char sum_pattern[] = "[sS][uU][mM]";
    static constexpr ctpg::regex_term<sum_pattern> sum_kw("sum_kw");

    static constexpr char max_pattern[] = "[mM][aA][xX]";
    static constexpr ctpg::regex_term<max_pattern> max_kw("max_kw");

    static constexpr char min_pattern[] = "[mM][iI][nN]";
    static constexpr ctpg::regex_term<min_pattern> min_kw("min_kw");

    static constexpr char not_pattern[] = "[nN][oO][tT]";
    static constexpr ctpg::regex_term<not_pattern> not_kw("not_kw");

    // precedence: AND > OR
    static constexpr char and_pattern[] = "[aA][nN][dD]";
    static constexpr ctpg::regex_term<and_pattern> and_kw("and_kw", 1);

    static constexpr char or_pattern[] = "[oO][rR]";
    static constexpr ctpg::regex_term<or_pattern> or_kw("or_kw", 0);

    static constexpr ctpg::nterm<AggOp> agg_kws{"agg_kws"};
    static constexpr auto agg_kws_rules = ctpg::rules(
        agg_kws(count_kw) >= [](std::string_view) { return AggOp::COUNT; },
        agg_kws(sum_kw) >= [](std::string_view) { return AggOp::SUM; },
        agg_kws(max_kw) >= [](std::string_view) { return AggOp::MAX; },
        agg_kws(min_kw) >= [](std::string_view) { return AggOp::MIN; }
    );
}

namespace impl_exp {
    static constexpr auto kw_terms = ctpg::terms(impl::select_kw, impl::as_kw, impl::from_kw, impl::where_kw, impl::on_kw,
                                                 impl::count_kw, impl::sum_kw, impl::max_kw, impl::min_kw,
                                                 impl::not_kw, impl::and_kw, impl::or_kw);
    static constexpr auto kw_nterms = ctpg::nterms(impl::agg_kws);
    static constexpr auto kw_rules = std::tuple_cat(impl::agg_kws_rules);
}

}

// keywords




#endif //SQL_KEYWORDS_H
