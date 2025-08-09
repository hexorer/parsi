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
    std::tuple<std::remove_cvref_t<Fs>...> parsers;

    constexpr explicit Sequence(std::remove_cvref_t<Fs>... parsers) noexcept
        : parsers(std::move(parsers)...)
    {
    }

    [[nodiscard]] constexpr auto operator()(Stream stream) const noexcept -> Result
    {
        if constexpr (sizeof...(Fs) == 0) {
            return Result{stream, true};
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
            if (!res) {
                return Result{stream, false};
            }
            return res;
        } else {
            auto res = std::get<I>(parsers)(stream);
            if (!res) {
                return Result{stream, false};
            }
            return parse_rec<I+1>(res.stream());
        }
    }
};

}  // namespace parsi::fn

#endif  // PARSI_FN_SEQUENCE_HPP
