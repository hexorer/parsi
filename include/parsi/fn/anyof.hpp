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
    std::tuple<std::remove_cvref_t<Fs>...> parsers;

    constexpr explicit AnyOf(std::remove_cvref_t<Fs>... parsers) noexcept
        : parsers(std::move(parsers)...)
    {
    }

    [[nodiscard]] constexpr auto operator()(Stream stream) const noexcept -> Result
    {
        if constexpr (sizeof...(Fs) == 0) {
            return Result{stream, false};
        } else {
            return parse_rec<0>(stream);
        }
    }

private:
    template <std::size_t I>
        requires (I < sizeof...(Fs))
    [[nodiscard]] constexpr auto parse_rec(Stream stream) const noexcept -> Result
    {
        if constexpr (I == sizeof...(Fs)-1) {
            auto res = std::get<I>(parsers)(stream);
            if (res) [[likely]] {
                return res;
            }
            return Result{stream, false};
        } else {
            auto res = std::get<I>(parsers)(stream);
            if (res) [[likely]] {
                return res;
            }
            return parse_rec<I+1>(stream);
        }
    }
};

}  // namespace parsi::fn

#endif  // PARSI_FN_ANYOF_HPP
