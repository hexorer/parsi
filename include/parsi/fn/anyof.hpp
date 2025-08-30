#ifndef PARSI_FN_ANYOF_HPP
#define PARSI_FN_ANYOF_HPP

#include <tuple>
#include <type_traits>
#include <utility>

#include "parsi/base.hpp"

namespace parsi::fn {

/**
 * A parser combinator where it tries the given `parsers`
 * on the given stream and at least one of them must succeed
 * which its result will be returned,
 * otherwise the result of the last one to fail will be returned.
 */
template <is_parser... Fs>
struct AnyOf {
    [[nodiscard]] constexpr auto operator()(Stream stream) const noexcept -> Result
    {
        return Result{stream, false};
    }
};

template <is_parser F>
struct AnyOf<F> {
    F parser;

    [[nodiscard]] constexpr auto operator()(Stream stream) const noexcept -> Result
    {
        auto res = parser(stream);
        if (!res) [[unlikely]] {
            return Result{stream, false};
        }
        return res;
    }
};

template <is_parser F1, is_parser F2, is_parser... Fs>
struct AnyOf<F1, F2, Fs...> {
    F1 parser;
    AnyOf<F2, Fs...> parsers;

    constexpr explicit AnyOf(F1 parser1, F2 parser2, Fs... parsers) noexcept
        : parser(std::move(parser1))
        , parsers(std::move(parser2), std::move(parsers)...)
    {
    }

    [[nodiscard]] constexpr auto operator()(Stream stream) const noexcept -> Result
    {
        auto res = parser(stream);
        if (!res) [[unlikely]] {
            return parsers(stream);
        }
        return res;
    }
};

}  // namespace parsi::fn

#endif  // PARSI_FN_ANYOF_HPP
