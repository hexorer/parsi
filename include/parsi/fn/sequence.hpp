#ifndef PARSI_FN_SEQUENCE_HPP
#define PARSI_FN_SEQUENCE_HPP

#include <tuple>
#include <type_traits>
#include <utility>

#include "parsi/base.hpp"

namespace parsi::fn {

/**
 * Combines multiple parsers in consecutive order.
 * 
 * It starts by passing the incoming stream to the first parser,
 * and on success, its result stream to the second
 * and goes on up to the last parser.
 * If any of the parsers fail, it would return the failed result.
 */
template <is_parser... Fs>
struct Sequence {
    [[nodiscard]] constexpr auto operator()(Stream stream) const noexcept -> Result
    {
        return Result{stream, true};
    }
};

template <is_parser F>
struct Sequence<F> {
    F parser;

    [[nodiscard]] constexpr auto operator()(Stream stream) const noexcept -> Result
    {
        return parser(stream);
    }
};

template <is_parser F1, is_parser F2, is_parser... Fs>
struct Sequence<F1, F2, Fs...> {
    F1 parser;
    Sequence<F2, Fs...> parsers;

    constexpr explicit Sequence(F1 parser1, F2 parser2, Fs... parsers) noexcept
        : parser(std::move(parser1))
        , parsers(std::move(parser2), std::move(parsers)...)
    {
    }

    [[nodiscard]] constexpr auto operator()(Stream stream) const noexcept -> Result
    {
        auto res = parser(stream);
        if (!res) [[unlikely]] {
            return res;
        }
        return parsers(res.stream());
    }
};

}  // namespace parsi::fn

#endif  // PARSI_FN_SEQUENCE_HPP
