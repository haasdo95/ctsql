#include "refl.hpp"
#include <iostream>
#include <utility>
#include <vector>
#include <type_traits>
#include <array>
#include <string_view>
#include <cassert>

//#include <__generator.hpp>

#include "parser/parser.h"
#include "parser/preproc.h"

#include "operator/selector.h"
#include "operator/projector.h"
#include <fmt/format.h>
#include <fmt/ranges.h>

#include "planner.h"

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

static_assert(not std::ranges::sized_range<std::generator<int>> and std::ranges::range<std::generator<int>>);
static_assert(not std::ranges::common_range<std::generator<int>>);

using namespace ctsql;
using namespace ctsql::impl;

int main() {
    std::vector<SchemaTuple<Point>> pvs;
    pvs.emplace_back(schema_to_tuple(Point(1, 1, "first")));
    pvs.emplace_back(schema_to_tuple(Point(1, 1, "first")));
    pvs.emplace_back(schema_to_tuple(Point(2, 1, "er")));
    pvs.emplace_back(schema_to_tuple(Point(2, 2, "er")));
    pvs.emplace_back(schema_to_tuple(Point(2, 3, "er")));
    pvs.emplace_back(schema_to_tuple(Point(1, 3, "san")));


//    static constexpr char query_s[] = R"(SELECT V.name, pt.name, P.x as X, y1 as Y, SUM(y) FROM pt P, v as V
//                                         ON x1=x AND y<V.y1 AND P.name = V.name WHERE x1 > 3 AND get_mag >= 1 OR x > 88)";
//    static constexpr char query_s[] = R"(SELECT name, MAX(get_mag), COUNT(y), SUM(y) FROM Point WHERE y>1 GROUP BY name)";
//    using QP = QueryPlanner<refl::make_const_string(query_s), Point>;
//    auto g = process<QP>(pvs);
//    fmt::print("one table: \n");
//    for (const auto& t: g) {
//        fmt::print("{}\n", t);
//    }

    std::vector<SchemaTuple<Vec>> vvs;
    vvs.emplace_back(schema_to_tuple(Vec(1, 1, 2, 2, "er")));
    vvs.emplace_back(schema_to_tuple(Vec(2, 3, 2, 1, "second")));

    static constexpr char query_join[] = R"(SELECT SUM(Point.x), MAX(y), Vec.x1, Point.name, Vec.name FROM Point, Vec ON Point.name=Vec.name WHERE Point.y<=2 AND Vec.x1<1.1)";
    using QP2 = QueryPlanner<refl::make_const_string(query_join), Point, Vec>;
    fmt::print("two tables: \n");
    auto g2 = process<QP2>(pvs, vvs);
    for (const auto& t: g2) {
        fmt::print("{}\n", t);
    }
}
