#ifndef SQL_PARSER_H
#define SQL_PARSER_H
#include <string>
#include <array>
#include <iostream>

#include <cassert>
#include "refl.hpp"
#include "logical.h"
#include "keywords.h"
#include "identifiers.h"
#include "numeral.h"


namespace ctsql {
//    template<std::size_t MaxNCols, std::size_t MaxNTables>
    class SelectParser {
    public:
        static constexpr ctpg::parser p{
            impl::col_name_list,
            std::tuple_cat(impl_exp::kw_terms, impl_exp::ident_terms, impl_exp::numeral_terms),
            std::tuple_cat(impl_exp::kw_nterms, impl_exp::ident_nterms, impl_exp::numeral_nterms),
            std::tuple_cat(impl_exp::kw_rules, impl_exp::ident_rules, impl_exp::numeral_rules)
        };
    };

}


//template<refl::const_string query>
//class Query {
//private:
//    static constexpr std::size_t npos = decltype(query)::npos;
//    static constexpr auto select_kw = refl::make_const_string("SELECT");
//    static constexpr std::size_t select_pos = find_substr_case_insensitive(query, select_kw);
//    static_assert(select_pos != npos);
//    static constexpr std::size_t select_kw_end = select_pos + select_kw.size;
//    static constexpr auto from_kw = refl::make_const_string("FROM");
//    static constexpr std::size_t from_pos = find_substr_case_insensitive(query, from_kw);
//    static_assert(from_pos != npos);
//    static constexpr std::size_t from_kw_end = from_pos + from_kw.size;
//
//    static constexpr auto column_names = query.template substr<select_kw_end, from_pos-select_kw_end>();
//    static constexpr auto c_buf = ctpg::buffers::cstring_buffer(column_names.data);
//    static constexpr std::size_t num_column_names = count_commas(column_names) + 1;
//    static constexpr auto select_p = SelectParser<num_column_names>::p;
//
//    static constexpr auto table_names = query.template substr<from_kw_end>();
//    static constexpr auto t_buf = ctpg::buffers::cstring_buffer(table_names.data);
//    static constexpr std::size_t num_table_names = count_commas(table_names) + 1;
//    static constexpr auto from_p = FromParser<num_table_names>::p;
//
//public:
//    static constexpr auto select_res = select_p.parse(c_buf).value();
//    static constexpr auto from_res = from_p.parse(t_buf).value();
//
//
//};

#endif //SQL_PARSER_H
