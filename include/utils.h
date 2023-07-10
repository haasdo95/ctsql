#ifndef SQL_UTILS_H
#define SQL_UTILS_H

#include "refl.hpp"

template<std::size_t N>
constexpr std::size_t count_commas(refl::const_string<N> s) {
    std::size_t count = 0;
    for (std::size_t i=0; i<s.size; ++i) {
        if (s.data[i] == ',') {
            ++count;
        }
    }
    return count;
}

constexpr char to_upper(char ch) {
    return ch >= 'a' && ch <= 'z'
           ? char(ch + ('A' - 'a'))
           : ch;
}

template<std::size_t N, std::size_t M>
constexpr bool substr_eq_case_insensitive(refl::const_string<N> s, refl::const_string<M> ss, std::size_t start) {
    if (start + ss.size > s.size) {
        return false;
    }
    for (size_t i=0; i<ss.size; ++i) {
        if (to_upper(s.data[start + i]) != to_upper(ss.data[i])) {
            return false;
        }
    }
    return true;
}

// find the first occurrence of a substr in string after (including) pos
template<std::size_t N, std::size_t M>
constexpr std::size_t find_substr_case_insensitive(refl::const_string<N> s, refl::const_string<M> ss, std::size_t pos = 0) {
    static_assert(N >= M and M > 0);
    for (size_t i=pos; i<N; ++i) {
        if (substr_eq_case_insensitive(s, ss, i)) {
            return i;
        }
    }
    return decltype(s)::npos;
}

enum class AggOp {
    NONE = 0,
    COUNT, SUM, MAX, MIN, AVG
};


#endif //SQL_UTILS_H
