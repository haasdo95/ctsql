#ifndef SQL_COMMON_H
#define SQL_COMMON_H

#include "ctpg.hpp"

namespace ctsql {
    static constexpr std::size_t MaxNCols = 4096;
    static constexpr std::size_t MaxNTables = 4096;

    enum class AggOp {
        NONE = 0,
        COUNT, SUM, MAX, MIN, AVG
    };

    enum class CompOp {
        EQ, GT, LT, GEQ, LEQ, NEQ
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
    };
    using ColumnNames = ctpg::stdex::cvector<ColumnName, MaxNCols>;

    struct TableName {
        std::string_view name;
        std::string_view alias;
        constexpr TableName() = default;
        constexpr TableName(std::string_view name, std::string_view alias): name{name}, alias{alias} {}
    };
    using TableNames = ctpg::stdex::cvector<TableName, MaxNTables>;
}



#endif //SQL_COMMON_H
