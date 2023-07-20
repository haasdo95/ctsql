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

    static constexpr auto indices = impl::make_lhs_indices<Point>(cnf[1]);
    static constexpr auto cop_list = impl::make_cop_list(cnf[1]);
    static constexpr auto rhs_types = impl::make_rhs_type_list(cnf[1]);

    const auto t = impl::schema_to_tuple(pt);

    static constexpr auto ss = impl::make_selectors<Point, indices, cop_list, rhs_types>(cnf[1]);
    assert(std::get<0>(ss)(t));
    assert(!std::get<1>(ss)(t));

    static constexpr auto and_cons = std::apply([](auto&&... args) { return impl::and_construct(args...); }, ss);
    assert(!and_cons(t));

    static constexpr auto or_cons = std::apply([](auto&&... args) { return impl::or_construct(args...); }, ss);
    assert(or_cons(t));
}