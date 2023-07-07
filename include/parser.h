#ifndef SQL_PARSER_H
#define SQL_PARSER_H
#include <string>
#include <array>

#include "refl.hpp"
#include "ctpg.hpp"


//static constexpr char sql_identifier_pattern[] = "[a-zA-Z][a-zA-Z0-9_]*";
static constexpr char sql_identifier_pattern[] = "[a-z]+";
static constexpr ctpg::regex_term<sql_identifier_pattern> sql_identifier("sql_identifier");


template<typename T, std::size_t Capacity>
struct ArrayLike {
    std::array<T, Capacity> arr;
    std::size_t len{};

    constexpr ArrayLike(): arr{}, len{} {}
    constexpr ArrayLike(std::array<T, Capacity> arr, std::size_t len): arr{arr}, len{len} {}

    static constexpr auto add(ArrayLike a, T elem) {
        std::array<T, Capacity> new_arr;
        std::copy_n(a.arr.begin(), a.len, new_arr.begin());
        new_arr[a.len + 1] = elem;
        return ArrayLike<T, Capacity>{new_arr, a.len + 1};
    }
};


struct ColumnName {
    std::string_view table_name;
    std::string_view column_name;
};
static constexpr ctpg::nterm<size_t> col_name("col_name");


//template<std::size_t NColsSel>
//class Parser {
//public:
//    using ColumnNames = ArrayLike<ColumnName, NColsSel>;
//    static constexpr ctpg::nterm<ColumnNames> col_name_list{"col_name_list"};
//
//    static constexpr ctpg::parser p{
//        col_name_list,
//        ctpg::terms(
//            sql_identifier, '.', ',', '*'
//        ),
//        ctpg::nterms(
//            col_name_list,
//            col_name
//        ),
//        ctpg::rules(
//            col_name(sql_identifier) >= [](std::string_view cn) { return ColumnName{.table_name="", .column_name=cn}; },
//            col_name(sql_identifier, '.', sql_identifier) >=
//                [](std::string_view tn, char, std::string_view cn) { return ColumnName{.table_name=tn, .column_name=cn}; },
//            col_name_list('*') >= [](char) { return ColumnNames{}; },
//            col_name_list(col_name_list, ',', col_name) >=
//                [](ColumnNames col_names, char, ColumnName col_name) { return ColumnNames::add(col_names, col_name); }
//        )};
//};

//template<std::size_t NColsSel>
class Parser {
public:
    using ColumnNames = size_t;
    static constexpr ctpg::nterm<ColumnNames> col_name_list{"col_name_list"};

    static constexpr ctpg::parser p{
            col_name_list,
            ctpg::terms(
                    sql_identifier, ','
            ),
            ctpg::nterms(
                    col_name_list,
                    col_name
            ),
            ctpg::rules(
                    col_name(sql_identifier) >= [](std::string_view cn) { return cn.size(); },
//                    col_name_list('*') >= [](char) { return ColumnNames{}; },
                    col_name_list(col_name_list, ',', col_name) >=
                        [](ColumnNames col_names, char, size_t col_name) { return col_names + col_name; }
            )};
};


#endif //SQL_PARSER_H
