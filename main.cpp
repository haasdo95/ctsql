#include "refl.hpp"
#include "parser.h"
#include <cstdint>
#include <sstream>
#include <iostream>
#include <vector>
#include <type_traits>
#include <array>
#include <string_view>

//
//consteval std::string_view trim(std::string_view s) {
//    const char* ws = " \t\n\r\f\v";
//    std::size_t start = s.find_first_not_of(ws);
//    if (start == std::string_view::npos) {
//        return {""};
//    }
//    std::size_t end = s.find_last_not_of(ws);
//    return {s.data() + start, end - start + 1};
//}
//
//template<refl::const_string s>
//consteval auto split() {
//    constexpr std::size_t count = count_commas(s);
//    std::array<std::string_view, count+1> arr;
//    std::size_t start = 0, end;
//    for (std::size_t i=0; i<count; ++i) {
//        end = s.find(',', start);
//        arr[i] = trim(std::string_view(&s.data[start], end - start));
//        start = end + 1;
//    }
//    arr[count] = trim(std::string_view(&s.data[start], s.size-start));
//    return arr;
//}
//
//template<typename T, std::size_t N>
//consteval auto map(std::array<T, N> arr, auto && f) {
//    using NT = decltype(f(arr[0]));
//    std::array<NT, N> mapped{};
//    for (std::size_t i=0; i<arr.size(); ++i) {
//        mapped[i] = f(arr[i]);
//    }
//    return mapped;
//}




int main() {
    static constexpr char query[] = " select ST.name, School.location from Student as ST, School";
    static constexpr auto cs = refl::make_const_string(query);

    static constexpr auto c_res = Query<cs>::select_res;
    static_assert(c_res.size() == 2);
    static constexpr auto t_res = Query<cs>::from_res;
    static_assert(t_res.size() == 2);

    for (const auto& e: c_res) {
        std::cout << e.column_name << " ";
    }
    std::cout << std::endl;
    for (const auto& e: t_res) {
        std::cout << e.name << " ";
    }

//    static_assert(res.size() == 3);
//    static constexpr char c_names[] = "name, height, Student.age, nickname as alias";
//    static constexpr char c_names[] = "   COUNT(  * )   ";
//    static constexpr char c_names[] = "COUNT(School.name) AS n_a_m_e, \"height\" As Height as hEight, Student.age";

//    auto buf = ctpg::buffers::string_buffer(c_names);
//    auto res = SelectParser<3>::p.parse(buf, std::cerr).value();

//    static constexpr auto cbuf = ctpg::buffers::cstring_buffer(ss.data);
//    constexpr auto res = SelectParser<2>::p.parse(cbuf).value();

//    for (const auto& e: res) {
//        std::cout << e.table_name << '.' << e.column_name << " AS " << e.alias << " with agg: " << static_cast<int>(e.agg) << std::endl;
//    }
//    static_assert(res.len == 3);

//    static_assert(res->arr.size() == 2);
}