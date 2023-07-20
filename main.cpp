#include "refl.hpp"
#include <iostream>
#include <utility>
#include <vector>
#include <type_traits>
#include <array>
#include <string_view>
#include <cassert>

#include "parser/parser.h"
#include "parser/preproc.h"

//#include <__generator.hpp>

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

//    const auto members = (refl::reflect<Point>().members);
//    static_assert(std::is_same_v<int, decltype(std::get<0>(members)(Point{}))>);

//    auto t = impl::schema_to_tuple<Point>(pt);
//    std::cout << std::get<0>(t) << std::get<1>(t) << std::get<2>(t) << std::get<3>(t) << std::endl;
//
//    static constexpr BooleanFactor leq_bf{CompOp::LEQ, BasicColumnName{"", "x"}, 1};
//    static constexpr auto selector = impl::make_selector<Point>(leq_bf);
//    assert(selector(impl::schema_to_tuple(pt)));

//    BooleanFactor geq_bf{CompOp::GEQ, BasicColumnName{"", "x"}, 1};
//    BooleanFactor y_neq_bf{CompOp::NEQ, BasicColumnName{"", "y"}, 2};
//
//    BooleanFactor mag_bf{CompOp::GT, BasicColumnName{"", "get_mag"}, 1};
//
//
//    BooleanAndTerms bat;
//    bat.push_back(leq_bf); bat.push_back(geq_bf); bat.push_back(y_neq_bf);
//    BooleanAndTerms bat_1{mag_bf, 1};
//
//    BooleanOrTerms bot;
//    bot.push_back(bat); bot.push_back(bat_1);
//
//    const auto and_cons = impl::and_construct<Point>(bat);
//    assert(!and_cons(t));
//
//    const auto or_cons = impl::or_construct<Point>(bot);
//    assert(or_cons(t));

//
//    static constexpr char query_s[] = R"(select y, x from Point)";
//    static constexpr auto cbuf = ctpg::buffers::cstring_buffer(query_s);
//    static constexpr auto res = ctsql::SelectParser::p.parse(cbuf).value();


    static constexpr char query_s[] = R"(SELECT V.name, pt.name, P.x as X, y1 as Y, SUM(y) FROM pt P, v as V
                                         ON x=x1 AND y=V.y1 WHERE x1 > 3 AND get_mag >= 1 AND x2 <= 4 OR 78 <= P.x AND v.name="lol" AND y2 < 3)";

    static constexpr auto cbuf = ctpg::buffers::cstring_buffer(query_s);
    static constexpr auto res = ctsql::SelectParser::p.parse(cbuf).value();

//    ctsql::print(std::cout, res.cns, ", ") << std::endl;
//    std::cout << res.tns << std::endl;
//    ctsql::print(std::cout, res.join_condition, " AND ") << std::endl;
//    for (const auto& bat: res.where_condition) {
//        ctsql::print(std::cout, bat, " AND ") << std::endl;
//    }
//
//    std::cout << std::endl;

    static constexpr auto pp_res = ctsql::impl::dealias_query(res);
//    ctsql::print(std::cout, pp_res.cns, ", ") << std::endl;
//    std::cout << pp_res.tns << std::endl;
//    ctsql::print(std::cout, pp_res.join_condition, " AND ") << std::endl;
//    for (const auto& bat: pp_res.where_condition) {
//        ctsql::print(std::cout, bat, " AND ") << std::endl;
//    }
//
//    std::cout << std::endl;

    static constexpr auto resolv_res = ctsql::impl::resolve_table_name<Point, Vec>(pp_res);
//    ctsql::print(std::cout, resolv_res.cns, ", ") << std::endl;
//    std::cout << resolv_res.tns << std::endl;
//    ctsql::print(std::cout, resolv_res.join_condition, " AND ") << std::endl;
//    for (const auto& bat: resolv_res.where_condition) {
//        ctsql::print(std::cout, bat, " AND ") << std::endl;
//    }

    static constexpr auto num_terms = resolv_res.where_condition.size();
    static constexpr auto num_clauses = impl::compute_number_of_cnf_clauses(resolv_res.where_condition);

    static constexpr auto cnf = impl::dnf_to_cnf<num_clauses, num_terms>(resolv_res.where_condition);

    for (size_t i=0; i<num_clauses; ++i) {
        for (size_t j=0; j<num_terms; ++j) {
            std::cout << cnf[i][j] << " ";
        }
        std::cout << std::endl;
    }

    const auto t = impl::schema_to_tuple(pt);

    static constexpr auto ss = impl::make_selectors<Point>(cnf[1]);
    assert(std::get<0>(ss)(t));
    assert(!std::get<1>(ss)(t));


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