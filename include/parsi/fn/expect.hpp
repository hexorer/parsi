#ifndef PARSI_FN_EXPECT_HPP
#define PARSI_FN_EXPECT_HPP

#include <string>

#include "parsi/base.hpp"
#include "parsi/charset.hpp"
#include "parsi/fixed_string.hpp"

namespace parsi::fn {

struct Negation {
    bool negated = false;
};

/**
 * A parser that expects the stream to start with the given character.
 */
template <Negation NegationV = Negation{.negated = false}>
struct ExpectChar {
    char expected;

    [[nodiscard]] constexpr auto operator()(Stream stream) const noexcept -> Result
    {
        if (stream.size() <= 0) [[unlikely]] {
            return Result{stream, false};
        }
        const bool is_valid = stream.as_string_view().starts_with(expected);
        stream.advance(1);
        if constexpr (NegationV.negated) {
            return Result{stream, !is_valid};
        } else {
            return Result{stream, is_valid};
        }
    }
};

/**
 * A parser that expects the stream to start with a character that is in the given charset.
 */
struct ExpectCharset {
    Charset charset;

    [[nodiscard]] constexpr auto operator()(Stream stream) const noexcept -> Result
    {
        if (stream.size() <= 0) [[unlikely]] {
            return Result{stream, false};
        }
        const bool is_valid = charset.contains(stream.front());
        stream.advance(1);
        return Result{stream, is_valid};
    }
};

/**
 * A parser that expects the stream to start with a character
 * that is in one of the given character ranges.
 */
template <std::size_t SizeV>
struct ExpectCharRangeSet {
    std::array<CharRange, SizeV> charset_ranges;

    [[nodiscard]] constexpr auto operator()(Stream stream) const noexcept -> Result
    {
        if (stream.size() <= 0) [[unlikely]] {
            return Result{stream, false};
        }

        const bool is_valid = check_against_ranges(stream.front(), std::make_index_sequence<SizeV>());
        stream.advance(1);
        return Result{stream, is_valid};
    }

private:
    template <std::size_t ...Is>
    [[nodiscard]] constexpr auto check_against_ranges(char chr, std::index_sequence<Is...>) const noexcept -> bool
    {
        return (false || ... || (charset_ranges[Is].begin <= chr && chr <= charset_ranges[Is].end));
    }
};

template <std::size_t SizeV, typename CharT = const char>
struct ExpectFixedString {
    FixedString<SizeV, CharT> expected;

    [[nodiscard]] constexpr auto operator()(Stream stream) const noexcept -> Result
    {
        const auto expected_strview = expected.as_string_view();
        const bool starts_with = stream.as_string_view().starts_with(expected_strview);
        return Result{stream.advanced(starts_with * expected_strview.size()), starts_with};
    }
};

/**
 * A parser that expects the stream to start with the given string.
 */
struct ExpectString {
    std::string expected;

    [[nodiscard]] auto operator()(Stream stream) const noexcept -> Result
    {
        const bool starts_with = stream.as_string_view().starts_with(expected);
        return Result{stream.advanced(starts_with * expected.size()), starts_with};
    }
};

/**
 * A parser that expects the stream to start with the given string view.
 */
struct ExpectStringView {
    std::string_view expected;

    [[nodiscard]] auto operator()(Stream stream) const noexcept -> Result
    {
        const bool starts_with = stream.as_string_view().starts_with(expected);
        return Result{stream.advanced(starts_with * expected.size()), starts_with};
    }
};

}  // namespace parsi::fn

#endif  // PARSI_FN_EXPECT_HPP
