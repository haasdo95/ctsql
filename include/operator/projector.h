#ifndef SQL_PROJECTOR_H
#define SQL_PROJECTOR_H

#include "common.h"

namespace ctsql
{
    static constexpr std::size_t cnt_agg_mark = static_cast<std::size_t>(-1);

    template<Reflectable S1, Reflectable S2, std::size_t N>
    constexpr auto make_indices_and_agg_ops(const ColumnNames& cns) {
        std::array<std::size_t, N> indices{};
        std::array<AggOp, N> agg_ops{};
        for (size_t i=0; i<N; ++i) {
            agg_ops[i] = cns[i].agg;
            if (cns[i].agg == AggOp::COUNT) {
                indices[i] = cnt_agg_mark;
            } else {
                indices[i] = get_index<S1, S2>(cns[i]);
            }
        }
        return std::make_pair(indices, agg_ops);
    }

    template<std::array agg_ops>
    static constexpr bool need_reduction = not std::all_of(agg_ops.begin(), agg_ops.end(), [](const auto& agg){ return agg == AggOp::NONE; });

    template<std::array indices, std::size_t... Idx>
    constexpr auto make_projector_impl(std::index_sequence<Idx...>) {
        return [](const auto& t) {
            return std::make_tuple(
                [&t](){
                    if constexpr (indices[Idx] == cnt_agg_mark) {
                        return static_cast<uint64_t>(1);
                    } else {
                        return std::get<indices[Idx]>(t);
                    }
                }()...  // unpacked IIFE; when count AGG is used, always project to 1
            );
        };
    }

    template<std::array indices>
    constexpr auto make_projector() {
        return make_projector_impl<indices>(std::make_index_sequence<indices.size()>());
    }

    template<typename STuple, std::array indices>
    using ProjectedTuple = decltype(make_projector<indices>()(std::declval<STuple>()));

    template<typename VT, AggOp agg>
    constexpr auto make_reduction_base() {
        if constexpr (agg == AggOp::COUNT) {
            return static_cast<uint64_t>(0);
        } else if constexpr (agg == AggOp::NONE or agg == AggOp::SUM) {
            return VT{};
        } else {
            if constexpr (std::is_arithmetic_v<VT>) {
                if constexpr (agg == AggOp::MAX) {
                    return std::numeric_limits<VT>::min();
                } else {
                    static_assert(agg == AggOp::MIN);
                    return std::numeric_limits<VT>::max();
                }
            } else {  // is string-like
                throw std::runtime_error("MAX/MIN operation on string/non-arithmetic values are not yet supported");
            }
        }
    }

    template<typename PTuple, std::array agg_ops, std::size_t ...Idx>
    constexpr auto make_tuple_reduction_base_impl(std::index_sequence<Idx...>) {
        return std::make_tuple(make_reduction_base<std::tuple_element_t<Idx, PTuple>, agg_ops[Idx]>()...);
    }

    template<typename PTuple, std::array agg_ops>
    constexpr auto make_tuple_reduction_base() {
        return make_tuple_reduction_base_impl<PTuple, agg_ops>(std::make_index_sequence<std::tuple_size_v<PTuple>>());
    }

    template<std::size_t idx, AggOp agg>
    constexpr auto to_operator() {
        if constexpr (agg == AggOp::NONE) {
            return [first=true](auto& base_tuple, const auto& new_val_tuple) mutable {
                if (first) [[unlikely]] {  // at most one assignment; taking the first value
                    std::get<idx>(base_tuple) = std::get<idx>(new_val_tuple);
                    first = false;
                }
            };
        } else if constexpr (agg == AggOp::COUNT or agg == AggOp::SUM) {
            return [](auto& base_tuple, const auto& new_val_tuple) { std::get<idx>(base_tuple) += std::get<idx>(new_val_tuple); };
        } else if constexpr (agg == AggOp::MAX) {
            return [](auto& base_tuple, const auto& new_val_tuple) { if (std::get<idx>(new_val_tuple) > std::get<idx>(base_tuple)) { std::get<idx>(base_tuple) = std::get<idx>(new_val_tuple); } };
        } else {
            static_assert(agg == AggOp::MIN);
            return [](auto& base_tuple, const auto& new_val_tuple) { if (std::get<idx>(new_val_tuple) < std::get<idx>(base_tuple)) { std::get<idx>(base_tuple) = std::get<idx>(new_val_tuple); } };
        }
    }

    template<typename ...Fs>
    constexpr auto to_tuple_operator_impl_impl(Fs ... fs) {
        return [fs...](auto& base_tuple, const auto& new_val_tuple) mutable {
            (..., fs(base_tuple, new_val_tuple));
        };
    }

    template<std::array agg_ops, std::size_t ...Idx>
    constexpr auto to_tuple_operator_impl(std::index_sequence<Idx...>) {
        return to_tuple_operator_impl_impl(to_operator<Idx, agg_ops[Idx]>()...);
    }

    template<std::array agg_ops>
    constexpr auto to_tuple_operator() {
        return to_tuple_operator_impl<agg_ops>(std::make_index_sequence<agg_ops.size()>());
    }
}

#endif //SQL_PROJECTOR_H
