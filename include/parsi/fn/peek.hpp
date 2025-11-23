#ifndef PARSI_FN_PEEK_HPP
#define PARSI_FN_PEEK_HPP

#include <type_traits>

#include "parsi/base.hpp"

namespace parsi::fn {

/**
 * A parser combinator that checks whether the given parser is valid or not
 * without changing the stream cursor.
 */
template <is_parser F>
struct Peek {
    std::remove_cvref_t<F> parser;

    [[nodiscard]] constexpr auto operator()(Stream stream) const noexcept -> Result
    {
        return Result{stream, parser(stream).is_valid()};
    };
};

}  // namespace parsi::fn

#endif  // PARSI_FN_PEEK_HPP
