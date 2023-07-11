#ifndef SQL_NUMERAL_H
#define SQL_NUMERAL_H

#include "ctpg.hpp"

namespace ctsql {
namespace impl {
    static constexpr char non_neg_decimal_integer_pattern[] = "[0-9]+";
    static constexpr ctpg::regex_term<non_neg_decimal_integer_pattern> non_neg_decimal_integer{"non_neg_decimal_integer"};
    static constexpr char zeros_pattern[] = "0*";
    static constexpr ctpg::regex_term<zeros_pattern> zeros{"zeros"};

    constexpr auto parse_digits(std::string_view digits) {
        uint64_t sum = 0;
        for (auto c : digits) { sum *= 10; sum += c - '0'; }
        return sum;
    }

    static constexpr ctpg::nterm<int64_t> integer_without_dec_pt{"integer_without_dec_pt"};
    static constexpr ctpg::nterm<int64_t> integer{"integer"};
    static constexpr auto integer_rules = ctpg::rules(
            integer_without_dec_pt('-', integer_without_dec_pt) >= [](char, int64_t v) { return -v; },
            integer_without_dec_pt(non_neg_decimal_integer) >= [](std::string_view digits) { return static_cast<int64_t>(parse_digits(digits)); },
            integer_without_dec_pt('+', integer_without_dec_pt) >= [](char, int64_t v) { return v; },
            integer(integer_without_dec_pt) >= [](int64_t v) { return v; },
            integer(integer_without_dec_pt, '.', zeros) >= [](int64_t v, char, std::string_view) { return v; }
    );

    static constexpr char non_neg_floating_point_pattern[] = "[0-9]*\\.0*[1-9][0-9]*";
    static constexpr ctpg::regex_term<non_neg_floating_point_pattern> non_neg_floating_point{"non_neg_floating_point"};
    static constexpr ctpg::nterm<double> floating_point("floating_point");
    static constexpr auto floating_point_rules = ctpg::rules(
            floating_point(non_neg_floating_point) >=
            [](std::string_view fp) {  // TODO: this is a fairly shabby fp parser
                std::size_t dec_pt_pos = fp.find('.');
                auto integral_part = parse_digits(fp.substr(0, dec_pt_pos));
                std::string_view dec_part_str = fp.substr(dec_pt_pos+1);
                // find the last non-zero digit; must exist because of regex
                auto last_non_zero_pos = dec_part_str.find_last_not_of('0');
                auto expo = last_non_zero_pos + 1;
                auto dec_part_int_repr = parse_digits(dec_part_str.substr(0, last_non_zero_pos+1));
                auto dec_part = static_cast<double>(dec_part_int_repr);
                for (unsigned int _ = 0; _ < expo; ++_) {
                    dec_part = dec_part / 10;
                }
                return static_cast<double>(integral_part) + dec_part;
            },
            floating_point('-', floating_point) >= [](char, double v) { return -v; },
            floating_point('+', floating_point) >= [](char, double v) { return v; }
    );

    static constexpr ctpg::nterm<std::variant<int64_t, double>> numeral{"numeral"};
    static constexpr auto int_or_double_rules = ctpg::rules(
            numeral(floating_point) >= [](double v) { return std::variant<int64_t, double>(v); },
            numeral(integer) >= [](int64_t v) { return std::variant<int64_t, double>(v); }
    );
}

namespace impl_exp {
    static constexpr auto numeral_terms = ctpg::terms('-', '+', impl::non_neg_floating_point, impl::non_neg_decimal_integer, impl::zeros);
    static constexpr auto numeral_nterms = ctpg::nterms(impl::floating_point, impl::integer_without_dec_pt, impl::integer, impl::numeral);
    static constexpr auto numeral_rules = std::tuple_cat(impl::floating_point_rules, impl::integer_rules, impl::int_or_double_rules);
}

}

#endif //SQL_NUMERAL_H
