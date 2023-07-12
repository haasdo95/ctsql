#include "refl.hpp"
#include "parser.h"
#include <cstdint>
#include <sstream>
#include <iostream>
#include <vector>
#include <type_traits>
#include <array>
#include <string_view>

int main() {
//    static constexpr char fp[] = "+--00120.0140";
//    static constexpr ctpg::parser num_parser{
//            numeral,
//            ctpg::terms('-', '+', non_neg_floating_point, non_neg_decimal_integer, zeros, '.'),
//            ctpg::nterms(floating_point, integer_without_dec_pt, integer, numeral),
//            std::tuple_cat(floating_point_rules, integer_rules, int_or_double_rules)
//    };
//
//    constexpr auto pi = num_parser.parse(ctpg::buffers::cstring_buffer(fp)).value();
//    std::visit([](auto v){std::cout << v << std::endl;}, pi);
//    static_assert(pi >= 3);

//    static constexpr char query[] = " select \'ST.name\', School.location from Student as ST, School";
//    static constexpr auto cs = refl::make_const_string(query);
//
//    static constexpr auto c_res = Query<cs>::select_res;
//    static_assert(c_res.size() == 2);
//    static constexpr auto t_res = Query<cs>::from_res;
//    static_assert(t_res.size() == 2);
//
//    for (const auto& e: c_res) {
//        std::cout << e.column_name << " ";
//    }
//    std::cout << std::endl;
//    for (const auto& e: t_res) {
//        std::cout << e.name << " ";
//    }

    using namespace ctsql;

    static constexpr char s[] = "height=173";
    static constexpr ctpg::parser p{
        impl::boolean_factor,
        std::tuple_cat(impl_exp::kw_terms, impl_exp::ident_terms, impl_exp::numeral_terms, impl_exp::logical_terms),
        std::tuple_cat(impl_exp::kw_nterms, impl_exp::ident_nterms, impl_exp::numeral_nterms, impl_exp::logical_nterms, ctpg::nterms(query)),
        std::tuple_cat(impl_exp::kw_rules, impl_exp::ident_rules, impl_exp::numeral_rules, impl_exp::logical_rules)
    };

//    static constexpr char c_names[] = R"(select "S-2".name "123", height AS HEIGHT, 'Student.age' s_age FROM "Student" as "S-1", School 'S-2' where height=173)";

//    auto buf = ctpg::buffers::string_buffer(c_names);
//    auto res = SelectParser<3>::p.parse(buf, std::cerr).value();

//    static constexpr auto cbuf = ctpg::buffers::cstring_buffer(c_names);
//    constexpr auto res = ctsql::SelectParser::p.parse(cbuf).value();
//    static_assert(res.cns[1].column_name == "height");
//    ctsql::print(std::cout, res.cns, ", ") << std::endl;
//    ctsql::print(std::cout, res.tns, ", ") << std::endl;

//    for (const auto& e: res) {
//        std::cout << e.table_name << '.' << e.column_name << " AS " << e.alias << " with agg: " << static_cast<int>(e.agg) << std::endl;
//    }
//    static_assert(res.len == 3);
//
//    static_assert(res->arr.size() == 2);
}