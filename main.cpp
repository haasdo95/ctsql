#include "refl.hpp"
#include <iostream>
#include <utility>
#include <vector>
#include <type_traits>
#include <array>
#include <string_view>

#include "parser/identifiers.h"
#include "parser/numeral.h"
#include "parser/keywords.h"
#include "parser/logical.h"
#include "parser/parser.h"

#include <__generator.hpp>

struct Point {
    Point() = default;
    Point(int x, int y, std::string name): x{x}, y{y}, name{std::move(name)} {}
    int x{};
    int y{};
    [[nodiscard]] double get_mag() const { return std::sqrt(static_cast<double>(x * x + y * y)); }
    std::string name;
};

REFL_AUTO(
    type(Point),
    field(x),
    field(y),
    func(get_mag, refl::attr::property("mag")),
    field(name)
)

std::generator<int> fibonacci() {
    int a = 0;
    int b = 1;
    co_yield a;
    for (int i=0; i<10; ++i) {
        co_yield a = std::exchange(b, a + b);
    }
}

template<typename T>
using schema_tuple_type = decltype(refl::util::map_to_tuple(refl::reflect<T>().members, [](auto td){
    return td(T{});
}));
//static_assert(std::is_same_v<tuple_type<Point>, int>);



int main() {
    Point pt{1, 1, "pt1"};

    refl::util::for_each(refl::reflect<Point>().members, [&](auto member){
        std::cout << refl::descriptor::get_display_name(member) << ' ' << member.name << " value: ";
        std::cout << member(pt);
        std::cout << std::endl;
    });


//    for (auto const& v: fibonacci()) {
//        std::cout << v << ' ';
//    }
//    std::cout << '\n';

//    using namespace ctsql;
//
    static constexpr char c_names[] = R"(select "S-2".name "123", height AS HEIGHT, 'Student.age' s_age
                                         FROM "Student" as "S-1", School 'S-2'
                                         where not height=173 AND name="smith" OR NOT age >= 8 AND School.loc <> 'NY')";
    static constexpr auto cbuf = ctpg::buffers::cstring_buffer(c_names);
    constexpr auto res = ctsql::SelectParser::p.parse(cbuf).value();
    ctsql::print(std::cout, res.cns, ", ") << std::endl;
    ctsql::print(std::cout, res.tns, ", ") << std::endl;
    for (const auto& bat: res.bot) {
        ctsql::print(std::cout, bat, " AND ");
        std::cout << std::endl;
    }
}