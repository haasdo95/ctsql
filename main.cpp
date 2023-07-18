#include "refl.hpp"
#include <iostream>
#include <utility>
#include <vector>
#include <type_traits>
#include <array>
#include <string_view>

//#include "parser/identifiers.h"
//#include "parser/numeral.h"
//#include "parser/keywords.h"
//#include "parser/logical.h"
//#include "parser/parser.h"
//#include "parser/preproc.h"

#include <__generator.hpp>

#include "schema.h"

struct Point {
    Point() = default;
    Point(int x, int y, std::string name): x{x}, y{y}, name{std::move(name)} {}
    int x{};
    int y{};
    [[nodiscard]] double get_mag() const { return std::sqrt(static_cast<double>(x * x + y * y)); }
    std::string name;
};

struct Vec {
    Vec() = default;
    Vec(int x1, int y1, int x2, int y2, std::string name): x1{x1}, y1{y1}, x2{x2}, y2{y2}, name{std::move(name)} {}
    int x1{};
    int y1{};
    int x2{};
    int y2{};
    std::string name;
};

REFL_AUTO(
    type(Point),
    field(x),
    field(y),
    func(get_mag),
    field(name)
)

REFL_AUTO(
    type(Vec),
    field(x1),
    field(y1),
    field(x2),
    field(y2),
    field(name)
)

//std::generator<int> fibonacci() {
//    int a = 0;
//    int b = 1;
//    co_yield a;
//    for (int i=0; i<10; ++i) {
//        co_yield a = std::exchange(b, a + b);
//    }
//}


//static_assert(std::is_same_v<tuple_type<Point>, int>);

using namespace ctsql;

int main() {
    Point pt{1, 2, "lol"};
    Vec v{1, 2, 3, 4, "ha"};
    BooleanFactor bf{CompOp::GT, BasicColumnName{"", "x"}, 1};
//    const auto selector = impl::make_selector<Point>(bf);
    auto t = impl::schema_to_tuple<Vec>(v);

//    static_assert(std::is_same_v<decltype(t), int>);
//    auto t = std::make_tuple(1, 2, 0.1, std::string("lol"));
//    assert(selector(t));

//
//    static constexpr char query_s[] = R"(select y, x from Point)";
//    static constexpr auto cbuf = ctpg::buffers::cstring_buffer(query_s);
//    static constexpr auto res = ctsql::SelectParser::p.parse(cbuf).value();

//    auto selector = impl::make_selector<Point>(bf);

//    static constexpr auto eq = to_operator<Point, Point>(CompOp::EQ);
//    static_assert(eq(Point{1, 1, "lol"}, Point{1, 1, "lol"}));
//    static constexpr auto neq = to_operator<CompOp::NEQ>();
//    static_assert(neq(Point{1, 1, "lol"}, Point{1, 2, "lol"}));

//    static constexpr char query_s[] = R"(SELECT V.name, pt.name, P.x as X, y1 as Y, SUM(y) FROM pt P, v as V
//                                         ON x=x1 AND y=V.y1 WHERE x1 > 3 OR pt.y >= P.x)";
//
//    static constexpr auto cbuf = ctpg::buffers::cstring_buffer(query_s);
//    static constexpr auto res = ctsql::SelectParser::p.parse(cbuf).value();
//    ctsql::print(std::cout, res.cns, ", ") << std::endl;
//    std::cout << res.tns << std::endl;
//    ctsql::print(std::cout, res.join_condition, " AND ") << std::endl;
//    for (const auto& bat: res.where_condition) {
//        ctsql::print(std::cout, bat, " AND ") << std::endl;
//    }
//
//    std::cout << std::endl;
//
//    static constexpr auto pp_res = ctsql::impl::dealias_query(res);
//    ctsql::print(std::cout, pp_res.cns, ", ") << std::endl;
//    std::cout << pp_res.tns << std::endl;
//    ctsql::print(std::cout, pp_res.join_condition, " AND ") << std::endl;
//    for (const auto& bat: pp_res.where_condition) {
//        ctsql::print(std::cout, bat, " AND ") << std::endl;
//    }
//
//    std::cout << std::endl;
//
//    static constexpr auto resolv_res = ctsql::impl::resolve_table_name<Point, Vec>(pp_res);
//    ctsql::print(std::cout, resolv_res.cns, ", ") << std::endl;
//    std::cout << resolv_res.tns << std::endl;
//    ctsql::print(std::cout, resolv_res.join_condition, " AND ") << std::endl;
//    for (const auto& bat: resolv_res.where_condition) {
//        ctsql::print(std::cout, bat, " AND ") << std::endl;
//    }

//
//    std::cout << refl::reflect<Point>().name << std::endl;
//
//    Stream<Point> s;
//    for (const auto& n: Stream<Point>::members) {
//        std::cout << n << " ";
//    }

//    static constexpr char c_names[] = R"(select "S-2".name "123", height AS HEIGHT, 'Student.age' s_age
//                                         FROM "Student" as "S-1", School 'S-2'
//                                         ON "S-1".loc = "S-2".loc and NOT "S-1".age < "S-2".age
//                                         where not height=173 AND name="smith" OR NOT age >= 8 AND School.loc <> 'NY')";
//
//    auto buf = ctpg::buffers::string_buffer(c_names);
//    auto res = ctsql::SelectParser::p.parse(buf, std::cerr).value();

//    static constexpr auto cbuf = ctpg::buffers::cstring_buffer(c_names);
//    constexpr auto res = ctsql::SelectParser::p.parse(cbuf).value();

//    ctsql::print(std::cout, res.cns, ", ") << std::endl;
//    std::cout << res.tns << std::endl;
//    ctsql::print(std::cout, res.join_condition, " AND ");
//    std::cout << std::endl;
//    for (const auto& bat: res.where_condition) {
//        ctsql::print(std::cout, bat, " AND ");
//        std::cout << std::endl;
//    }
}