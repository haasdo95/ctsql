#ifndef SQL_COMMON_H
#define SQL_COMMON_H

#include <__generator.hpp>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <cassert>
#include <concepts>
#include "ctpg.hpp"
#include "refl.hpp"

namespace ctsql {
    static constexpr std::size_t MaxNCols = 32;

    static constexpr std::size_t MaxAndTerms = 8;
    static constexpr std::size_t MaxOrTerms = 8;

    enum class AggOp {
        NONE = 0,
        COUNT, SUM, MAX, MIN
    };
    constexpr std::string_view to_str(AggOp agg) {
        switch (agg) {
            case AggOp::NONE:
                return "NONE";
            case AggOp::COUNT:
                return "COUNT";
            case AggOp::SUM:
                return "SUM";
            case AggOp::MAX:
                return "MAX";
            case AggOp::MIN:
                return "MIN";
        }
        throw std::runtime_error("unknown agg");
    }
    std::ostream& operator<<(std::ostream& os, AggOp agg) {
        os << to_str(agg);
        return os;
    }

    enum class CompOp {
        EQ = 0, GT, LT, GEQ, LEQ, NEQ
    };
    template<CompOp cop>
    constexpr auto to_operator() {
        if constexpr (cop == CompOp::EQ)
            return [](const auto& lhs, const auto& rhs){ return lhs == rhs; };
        else if constexpr (cop == CompOp::GT)
            return [](const auto& lhs, const auto& rhs){ return lhs > rhs; };
        else if constexpr (cop == CompOp::LT)
            return [](const auto& lhs, const auto& rhs){ return lhs < rhs; };
        else if constexpr (cop == CompOp::GEQ)
            return [](const auto& lhs, const auto& rhs){ return lhs >= rhs; };
        else if constexpr (cop == CompOp::LEQ)
            return [](const auto& lhs, const auto& rhs){ return lhs <= rhs; };
        else {
            static_assert(cop == CompOp::NEQ);
            return [](const auto& lhs, const auto& rhs){ return lhs != rhs; };
        }
    }

    constexpr CompOp invert(CompOp cop) {
        switch (cop) {
            case CompOp::EQ:
                return CompOp::EQ;
            case CompOp::GT:
                return CompOp::LT;
            case CompOp::LT:
                return CompOp::GT;
            case CompOp::GEQ:
                return CompOp::LEQ;
            case CompOp::LEQ:
                return CompOp::GEQ;
            case CompOp::NEQ:
                return CompOp::NEQ;
        }
        throw std::runtime_error("unknown comp op");
    }
    constexpr CompOp negate(CompOp cop) {
        switch (cop) {
            case CompOp::EQ:
                return CompOp::NEQ;
            case CompOp::GT:
                return CompOp::LEQ;
            case CompOp::LT:
                return CompOp::GEQ;
            case CompOp::GEQ:
                return CompOp::LT;
            case CompOp::LEQ:
                return CompOp::GT;
            case CompOp::NEQ:
                return CompOp::EQ;
        }
        throw std::runtime_error("unknown comp op");
    }
    constexpr std::string_view to_str(CompOp cop) {
        switch (cop) {
            case CompOp::EQ:
                return "=";
            case CompOp::GT:
                return ">";
            case CompOp::LT:
                return "<";
            case CompOp::GEQ:
                return ">=";
            case CompOp::LEQ:
                return "<=";
            case CompOp::NEQ:
                return "<>";
        }
        throw std::runtime_error("unknown comparison operator");
    }
    std::ostream& operator<<(std::ostream& os, CompOp cop) {
        os << to_str(cop);
        return os;
    }

    struct BasicColumnName {
        constexpr BasicColumnName(std::string_view table_name, std::string_view column_name): table_name{table_name}, column_name{column_name} {}
        constexpr BasicColumnName() = default;
        constexpr BasicColumnName(const BasicColumnName&) = default;
        std::string_view table_name;
        std::string_view column_name;
        friend std::ostream& operator<<(std::ostream& os, BasicColumnName bcn) {
            if (not bcn.table_name.empty()) {
                os << bcn.table_name << ".";
            }
            os << bcn.column_name;
            return os;
        }
    };

    struct ColumnName {
        constexpr ColumnName(std::string_view table_name, std::string_view column_name, std::string_view alias, AggOp agg)
                : table_name{table_name}, column_name{column_name}, alias{alias}, agg{agg} {}
        constexpr ColumnName() = default;
        constexpr ColumnName(const ColumnName&) = default;
        std::string_view table_name;
        std::string_view column_name;
        std::string_view alias;
        AggOp agg{};
        friend std::ostream& operator<<(std::ostream& os, ColumnName cn) {
            if (cn.agg != AggOp::NONE) {
                os << cn.agg << "(";
            }
            if (not cn.table_name.empty()) {
                os << cn.table_name << ".";
            }
            if (cn.column_name.empty()) {
                os << "*";
            } else {
                os << cn.column_name;
            }
            if (cn.agg != AggOp::NONE) {
                os << ")";
            }
            if (not cn.alias.empty()) {
                os << " AS \"" << cn.alias << "\"";
            }
            return os;
        }
    };
    using ColumnNames = ctpg::stdex::cvector<ColumnName, MaxNCols>;


    struct TableName {
        std::string_view name;
        std::string_view alias;
        constexpr TableName() = default;
        constexpr TableName(std::string_view name, std::string_view alias): name{name}, alias{alias} {}
        friend std::ostream& operator<<(std::ostream& os, TableName tn) {
            os << tn.name;
            if (not tn.alias.empty()) {
                os << " AS \"" << tn.alias << "\"";
            }
            return os;
        }
    };
    // simplifying assumption: joining at most 2 tables
    struct TableNames {
        constexpr TableNames() = default;
        constexpr explicit TableNames(TableName first): first{first}, second{std::nullopt} {}
        constexpr TableNames(TableName first, TableName second): first{first}, second{second} {}
        constexpr TableNames(const TableNames&) = default;
        TableName first;
        std::optional<TableName> second;
    };

    std::ostream& operator<<(std::ostream& os, const TableNames& tns) {
        os << tns.first << ", ";
        if (tns.second) {
            os << tns.second.value();
        }
        return os;
    }

    template<typename Vec>
    std::ostream& print(std::ostream& os, const Vec& vec, const std::string& sep) {
        if (vec.empty()) {
            os << "*";
        } else {
            for (size_t i = 0; i < vec.size()-1; i++) {
                os << vec[i] << sep;
            }
            os << vec.back();
        }
        return os;
    }

    // we assume that one side of the clause will be a column name reference
    template<bool one_side=true>
    struct BooleanFactor {
        using RHSType = std::conditional_t<one_side, std::variant<std::string_view, int64_t, double>, BasicColumnName>;
        CompOp cop{};
        BasicColumnName lhs;
        RHSType rhs;
        constexpr BooleanFactor() = default;
        constexpr BooleanFactor(const BooleanFactor&) = default;
        constexpr BooleanFactor(CompOp cop, BasicColumnName lhs, RHSType rhs): cop{cop}, lhs{lhs}, rhs{rhs} {}
    };
    template<bool one_side>
    std::ostream& operator<<(std::ostream& os, BooleanFactor<one_side> be) {
        os << be.lhs << ' ' << be.cop << ' ';
        if constexpr (one_side) {
            std::visit([&os](auto v) {
            if constexpr (std::is_same_v<decltype(v), std::string_view>) {
                os << '\"' << v << '\"';
            } else {
                os << v;
            }}, be.rhs);
        } else {
            os << be.rhs;
        }
        return os;
    }

    template<bool one_side>
    using BooleanAndTerms = ctpg::stdex::cvector<BooleanFactor<one_side>, MaxAndTerms>;
    template<bool one_side>
    using BooleanOrTerms = ctpg::stdex::cvector<BooleanAndTerms<one_side>, MaxOrTerms>;

    struct Query {
        ColumnNames cns;
        TableNames tns;
        BooleanOrTerms<false> join_condition;
        BooleanOrTerms<true> where_condition;
        ColumnNames group_by_keys;
        constexpr Query() = default;
        constexpr Query(ColumnNames cns, TableNames tns, BooleanOrTerms<false> join_condition, BooleanOrTerms<true> where_condition, ColumnNames group_by_keys):
            cns{cns}, tns{tns}, join_condition{join_condition}, where_condition{where_condition}, group_by_keys{group_by_keys} {}
    };

    // for selector-making
    template<typename T>
    concept Reflectable = refl::is_reflectable<T>() or std::is_void_v<T>;
    
    template<typename T>
    concept ColumnReference = requires(const T& cr) {
        requires requires { cr.table_name; cr.column_name;};
    };

    enum class RHSTypeTag {
        INT=0, DOUBLE, STRING, COLNAME
    };

    // schema object => tuple with field values
    template<Reflectable Schema>
    constexpr auto schema_to_tuple(const Schema& schema) {
        if constexpr (std::is_void_v<Schema>) {
            return std::make_tuple();
        } else {
             return refl::util::map_to_tuple(refl::reflect<Schema>().members, [&schema](auto td){
                return td(schema);
            });
        }
    }

    template<Reflectable Schema>
    using SchemaTuple = decltype(schema_to_tuple(std::declval<Schema>()));

    template<Reflectable S1, Reflectable S2>
    constexpr auto schema_to_tuple_2(const S1& s1, const S2& s2) {
        return std::tuple_cat(schema_to_tuple(s1), schema_to_tuple(s2));
    }

    template<Reflectable S1, Reflectable S2> requires requires {not std::is_void_v<S2>;}
    using SchemaTuple2 = decltype(schema_to_tuple_2(std::declval<S1>(), std::declval<S2>()));

    template<Reflectable Schema>
    static constexpr auto member_list = refl::util::map_to_array<std::string_view>(refl::reflect<Schema>().members,
                                                                                   [](auto td){return td.name.str_view();});

    template<Reflectable S1, Reflectable S2>
    constexpr std::size_t get_index(const ColumnReference auto& bcn) {
        if constexpr (std::is_void_v<S2>) {
            const auto& ml = member_list<S1>;
            auto pos = std::find(ml.begin(), ml.end(), bcn.column_name);
            return std::distance(ml.begin(), pos);
        } else {
            if (bcn.table_name == "0") {
                return get_index<S1, void>(bcn);
            } else {
                return member_list<S1>.size() + get_index<S2, void>(bcn);  // with offset
            }
        }
    }

    // rules of determining size:
    //  - if the input is already sized, use that; otherwise use the user provided estimated size
    constexpr inline std::size_t get_input_size(const std::ranges::range auto& input, std::size_t estimated_size) {
        if constexpr (std::ranges::sized_range<decltype(input)>) {
            return std::ranges::size(input);
        } else {
            return estimated_size;
        }
    }

    // generate a filtered range
    template<auto pred>
    static auto filter(std::ranges::range auto& input) -> std::generator<std::ranges::range_value_t<decltype(input)>> {
        for (auto&& inp: input) {
            if (pred(inp)) {
                co_yield inp;
            }
        }
    }

}



#endif //SQL_COMMON_H
