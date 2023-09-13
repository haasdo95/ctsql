#ifndef SQL_JOIN_H
#define SQL_JOIN_H

#include "common.h"

/*** if the join condition contains only AND, it makes sense to pick out the equality conditions and use hash join
 *
 */

namespace ctsql::impl {
    template<Reflectable S1, Reflectable S2, typename Vec>
    requires requires {not std::is_void_v<S1> and not std::is_void_v<S2>;}
    constexpr auto sift_join_condition(const Vec& bfs) {
        BooleanAndTerms<false> eq_conditions, the_rest;
        for (const BooleanFactor<false>& bf: bfs) {
            if (bf.cop == CompOp::EQ) {
                eq_conditions.push_back(bf);
            } else {
                the_rest.push_back(bf);
            }
        }
        return std::make_pair(eq_conditions, the_rest);
    }

    template<Reflectable S1, Reflectable S2, std::size_t N, typename Vec>
    requires requires {not std::is_void_v<S1> and not std::is_void_v<S2>;}
    constexpr auto make_join_indices(const Vec& bfs) {
        std::array<size_t, N> t0_hj_indices{}, t1_hj_indices{};
        for (size_t i = 0; i < N; ++i) {
            const BooleanFactor<false>& bf = bfs[i];
            if (bf.lhs.table_name == "0") {
                t0_hj_indices[i] = get_index<S1, void>(bf.lhs);
                t1_hj_indices[i] = get_index<S2, void>(bf.rhs);
            } else {
                t1_hj_indices[i] = get_index<S2, void>(bf.lhs);
                t0_hj_indices[i] = get_index<S1, void>(bf.rhs);
            }
        }
        return std::make_pair(t0_hj_indices, t1_hj_indices);
    }

}


#endif //SQL_JOIN_H
