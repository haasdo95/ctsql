#include "refl.hpp"
#include <cstdint>
#include <sstream>
#include <iostream>
#include <vector>
#include <type_traits>
#include <array>
#include <string_view>

#include "identifiers.h"
#include "numeral.h"
#include "keywords.h"
#include "logical.h"
#include "parser.h"

#include "__generator.hpp"

std::generator<int> fibonacci() {
    int a = 0;
    int b = 1;
    co_yield a;
    for (int i=0; i<10; ++i) {
        co_yield a = std::exchange(b, a + b);
    }
}


int main() {
    for (auto const& v: fibonacci()) {
        std::cout << v << ' ';
    }
    std::cout << '\n';
//    using namespace ctsql;
//
//    static constexpr char c_names[] = R"(select "S-2".name "123", height AS HEIGHT, 'Student.age' s_age
//                                         FROM "Student" as "S-1", School 'S-2'
//                                         where height=173 AND name="smith" OR age >= 8 AND School.loc <> 'NY')";
//    static constexpr auto cbuf = ctpg::buffers::cstring_buffer(c_names);
//    constexpr auto res = ctsql::SelectParser::p.parse(cbuf).value();
//    ctsql::print(std::cout, res.cns, ", ") << std::endl;
//    ctsql::print(std::cout, res.tns, ", ") << std::endl;
//    for (const auto& bat: res.bot) {
//        ctsql::print(std::cout, bat, " AND ");
//        std::cout << std::endl;
//    }
}