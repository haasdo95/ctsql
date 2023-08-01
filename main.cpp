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

using namespace ctsql;
using namespace ctsql::impl;

int main() {
/*** testing proj
    Point p1{1, 1, "p1"}, p2{2, 2, "p2"}, p3{3, 3, "p3"};
    std::vector<Point> ps = {p1, p2, p3};
    static constexpr char query_s[] = R"(SELECT name, COUNT(*), COUNT(name), SUM(get_mag), MIN(y), MAX(y) FROM Point)";

    static constexpr auto cbuf = ctpg::buffers::cstring_buffer(query_s);
    static constexpr auto res = ctsql::SelectParser::p.parse(cbuf).value();
    static constexpr auto pp_res = ctsql::impl::dealias_query(res);
    static constexpr auto resolv_res = ctsql::impl::resolve_table_name<Point>(pp_res);
    ctsql::print(std::cout, resolv_res.cns, ", ") << std::endl;
    std::cout << resolv_res.tns << std::endl;
    for (const auto& bat: resolv_res.join_condition) {
        ctsql::print(std::cout, bat, " AND ") << std::endl;
    }
    for (const auto& bat: resolv_res.where_condition) {
        ctsql::print(std::cout, bat, " AND ") << std::endl;
    }

    static constexpr auto indices_and_agg_ops = make_indices_and_agg_ops<Point, void, resolv_res.cns.size()>(resolv_res.cns);
    static constexpr auto indices = indices_and_agg_ops.first;
    for (const auto& idx: indices) {
        std::cout << idx << " ";
    }
    std::cout << std::endl;
    static constexpr auto agg_ops = indices_and_agg_ops.second;
    static_assert(need_reduction<agg_ops>);
    static constexpr auto projector = make_projector<indices>();
    using PTuple = ProjectedTuple<SchemaTuple<Point>, indices>;
    std::vector<PTuple> pv;
    for (const auto& p: ps) {
        pv.push_back(projector(schema_to_tuple(p)));
    }

    auto base = make_tuple_reduction_base<PTuple, agg_ops>();
    static constinit auto reduce_op = to_tuple_operator<agg_ops>();
    for (const auto& t: pv) {
        reduce_op(base, t);
    }
    std::cout << std::get<0>(base) << ", " << std::get<1>(base)
            << ", " << std::get<2>(base) << ", " << std::get<3>(base)
            << ", " << std::get<4>(base) << ", " << std::get<5>(base) << std::endl;
***/


/*** Testing selector
    Point pt{1, 2, "lol"};
    Vec v{1, 2, 3, 4, "ha"};

    static constexpr char query_s[] = R"(SELECT V.name, pt.name, P.x as X, y1 as Y, SUM(y) FROM pt P, v as V
                                         ON x=x1 AND y=V.y1 OR P.name = V.name WHERE x1 > 3 AND get_mag >= 1 OR x > 88)";

    static constexpr auto cbuf = ctpg::buffers::cstring_buffer(query_s);
    static constexpr auto res = ctsql::SelectParser::p.parse(cbuf).value();
    static constexpr auto pp_res = ctsql::impl::dealias_query(res);
    static constexpr auto resolv_res = ctsql::impl::resolve_table_name<Point, Vec>(pp_res);

    ctsql::print(std::cout, resolv_res.cns, ", ") << std::endl;
    std::cout << resolv_res.tns << std::endl;
    for (const auto& bat: resolv_res.join_condition) {
        ctsql::print(std::cout, bat, " AND ") << std::endl;
    }
    for (const auto& bat: resolv_res.where_condition) {
        ctsql::print(std::cout, bat, " AND ") << std::endl;
    }

    static constexpr auto num_terms = resolv_res.where_condition.size();
    static constexpr auto num_clauses = impl::compute_number_of_cnf_clauses(resolv_res.where_condition);

    static constexpr auto cnf = impl::dnf_to_cnf<num_clauses, num_terms>(resolv_res.where_condition);

    static constexpr auto sifted = impl::sift(cnf);
    static constexpr size_t t0_end = std::get<0>(sifted);
    static constexpr size_t t1_end = std::get<1>(sifted);
    static constexpr auto sifted_cnf = std::get<2>(sifted);

    static constexpr auto sift_split = impl::split_sifted<t0_end, t1_end>(sifted_cnf);
    static constexpr auto t0 = std::get<0>(sift_split);
    static constexpr auto t1 = std::get<1>(sift_split);
    static constexpr auto mixed = std::get<2>(sift_split);

    auto print_cnf = [](const auto& cnf) {
        if (!cnf.empty()) {
            std::cout << '(';
            print(std::cout, cnf[0], " OR ") << ')';
            for (size_t i=1; i<cnf.size(); ++i) {
                std::cout << " AND (";
                print(std::cout, cnf[i], " OR ") << ')';
            }
        }
        std::cout << std::endl;
    };
    std::cout << "t0: \n";
    print_cnf(t0);
    std::cout << "t1: \n";
    print_cnf(t1);
    std::cout << "mixed: \n";
    print_cnf(mixed);

    static constexpr auto t0_inner_dim = impl::make_inner_dim<t0.size()>(num_terms);
    static constexpr auto t0_selector = impl::make_cnf_selector<Point, void, true, impl::make_indices_2d<Point, void, true, true, t0_inner_dim>(t0), impl::make_indices_2d<Point, void, false, true, t0_inner_dim>(t0), impl::make_cop_list_2d<t0_inner_dim>(t0), impl::make_rhs_type_list_2d<t0_inner_dim>(t0), t0_inner_dim>(t0);
    static constexpr auto t1_inner_dim = impl::make_inner_dim<t1.size()>(num_terms);
    static constexpr auto t1_selector = impl::make_cnf_selector<Vec, void, true, impl::make_indices_2d<Vec, void, true, true, t1_inner_dim>(t1), impl::make_indices_2d<Vec, void, false, true, t1_inner_dim>(t1), impl::make_cop_list_2d<t1_inner_dim>(t1), impl::make_rhs_type_list_2d<t1_inner_dim>(t1), t1_inner_dim>(t1);

    assert(t0_selector(schema_to_tuple(pt)));
    assert(t1_selector(schema_to_tuple(v)));

    static constexpr auto mixed_inner_dim = impl::make_inner_dim<mixed.size()>(num_terms);
    static constexpr auto mixed_selector = impl::make_cnf_selector<Point, Vec, true, impl::make_indices_2d<Point, Vec, true, true, mixed_inner_dim>(mixed), impl::make_indices_2d<Point, Vec, false, true, mixed_inner_dim>(mixed), impl::make_cop_list_2d<mixed_inner_dim>(mixed), impl::make_rhs_type_list_2d<mixed_inner_dim>(mixed), mixed_inner_dim>(mixed);
    assert(!mixed_selector(schema_to_tuple_2(pt, v)));

    static constexpr auto dnf_lens = impl::make_inner_dim<resolv_res.where_condition.size()>(resolv_res.where_condition);
    static constexpr auto aligned_dnf = impl::align_dnf<dnf_lens>(resolv_res.where_condition);

    static constexpr auto dnf_selector = impl::make_dnf_selector<Point, Vec, true, impl::make_indices_2d<Point, Vec, true, true, dnf_lens>(aligned_dnf), impl::make_indices_2d<Point, Vec, false, true, dnf_lens>(aligned_dnf), impl::make_cop_list_2d<dnf_lens>(aligned_dnf), impl::make_rhs_type_list_2d<dnf_lens>(aligned_dnf), dnf_lens>(aligned_dnf);
    assert(!dnf_selector(schema_to_tuple_2(pt, v)));

    static constexpr auto join_condition_lens = impl::make_inner_dim<resolv_res.join_condition.size()>(resolv_res.join_condition);
    static constexpr auto aligned_join_dnf = impl::align_dnf<join_condition_lens>(resolv_res.join_condition);
    static constexpr auto join_dnf_selector = impl::make_dnf_selector<Point, Vec, false, impl::make_indices_2d<Point, Vec, true, false, join_condition_lens>(aligned_join_dnf), impl::make_indices_2d<Point, Vec, false, false, join_condition_lens>(aligned_join_dnf), impl::make_cop_list_2d<join_condition_lens>(aligned_join_dnf), impl::make_rhs_type_list_2d<join_condition_lens>(aligned_join_dnf), join_condition_lens>(aligned_join_dnf);
    assert(join_dnf_selector(schema_to_tuple_2(pt, v)));
***/

//    static constexpr char query_s[] = R"(SELECT V.name, pt.name, P.x as X, y1 as Y, SUM(y) FROM pt P, v as V
//                                         ON x1=x AND y<V.y1 AND P.name = V.name WHERE x1 > 3 AND get_mag >= 1 OR x > 88)";
    static constexpr char query_s[] = R"(SELECT name, MAX(get_mag), COUNT(y), SUM(y) FROM Point)";
    using QP = QueryPlanner<refl::make_const_string(query_s), Point>;
    std::vector<SchemaTuple<Point>> pvs;
    pvs.emplace_back(schema_to_tuple(Point(1, 1, "yi")));
    pvs.emplace_back(schema_to_tuple(Point(2, 1, "er")));
    pvs.emplace_back(schema_to_tuple(Point(1, 3, "san")));
    auto reduce_gen = QP::Reduce::RG::reduce(pvs);
}
