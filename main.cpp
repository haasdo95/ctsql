#include "refl.hpp"
#include "parser.h"
#include <cstdint>
#include <sstream>
#include <iostream>
#include <vector>
#include <type_traits>
#include <array>
#include <string_view>

//template<std::size_t StrLen>
//consteval std::size_t count_commas(refl::const_string<StrLen> s) {
//    std::size_t count = 0;
//    for (std::size_t i=0; i<s.size; ++i) {
//        if (s.data[i] == ',') {
//            ++count;
//        }
//    }
//    return count;
//}
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
    constexpr char c_names[] = "name, height";
    Parser parser;
    constexpr auto res = Parser::p.parse(ctpg::buffers::cstring_buffer(c_names));
//    static_assert(res->len == 3);


//    static_assert(count_commas(refl::make_const_string("hello, world lol")) == 1);
//    constexpr auto s = refl::make_const_string("    hello,  world,lol  , ");
//    constexpr auto arr = split<s>();
//    static_assert(split<s>().size() == 4);
//    static_assert(split<s>()[0] == "hello");
//    static_assert(split<s>()[1] == "world");
//    static_assert(split<s>()[2] == "lol");
//    static_assert(split<s>()[3].empty());
//
//    std::cout << split<s>()[0] << std::endl;
//    std::cout << split<s>()[1] << std::endl;
//    std::cout << split<s>()[2] << std::endl;
//    std::cout << split<s>()[3] << std::endl;
//
//    constexpr auto cnt = map(arr, [](auto s){return s.size();});
//    static_assert(cnt[0] == 5);
//    static_assert(cnt[1] == 5);
//    static_assert(cnt[2] == 3);
//    static_assert(cnt[3] == 0);

}