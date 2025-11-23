#ifndef PARSI_PARSI_HPP
#define PARSI_PARSI_HPP

#include "parsi/base.hpp"
#include "parsi/rtparser.hpp"
#include "parsi/charset.hpp"
#include "parsi/fixed_string.hpp"
#include "parsi/fn/anyof.hpp"
#include "parsi/fn/alters.hpp"
#include "parsi/fn/eos.hpp"
#include "parsi/fn/expect.hpp"
#include "parsi/fn/extract.hpp"
#include "parsi/fn/optional.hpp"
#include "parsi/fn/peek.hpp"
#include "parsi/fn/repeated.hpp"
#include "parsi/fn/sequence.hpp"
#include "parsi/internal/optimizer.hpp"

namespace parsi {

/**
 * Creates a parser that expects the stream to have come to its end.
 */
[[nodiscard]] constexpr auto eos() noexcept -> fn::Eos
{
    return fn::Eos{};
}

/**
 * Creates a parser that expects the stream to start with the given fixed string.
 * Parser's expected string is fixed and cannot be changed. better performance for string literals.
 */
template <std::size_t SizeV>
[[nodiscard]] constexpr auto expect(const char (&str)[SizeV]) noexcept
{
    return fn::ExpectFixedString<SizeV, const char>{FixedString<SizeV, const char>::make(str, SizeV).value()};
}

/**
 * Creates a parser that expects the stream to start with the given fixed string.
 * Parser's expected string is fixed and cannot be changed. better performance for string literals.
 */
template <std::size_t SizeV, typename CharT = const char>
[[nodiscard]] constexpr auto expect(FixedString<SizeV, CharT> expected) noexcept
{
    return fn::ExpectFixedString<SizeV, CharT>{expected};
}

/**
 * Creates a parser that expects the stream to start with the given string.
 */
[[nodiscard]] inline auto expect(std::string expected) noexcept
{
    return fn::ExpectString{std::move(expected)};
}

/**
 * Creates a parser that expects the stream to start with the given character.
 */
[[nodiscard]] constexpr auto expect(char expected) noexcept
{
    return fn::ExpectChar<>{expected};
}

/**
 * Creates a parser that expects the stream to start with the given character.
 */
[[nodiscard]] constexpr auto expect_not(char expected) noexcept
{
    return fn::ExpectChar<fn::Negation{.negated = true}>{expected};
}

/**
 * Creates a parser that expects the stream to
 * start with a character that is in the given charset.
 */
[[nodiscard]] constexpr auto expect(Charset expected) noexcept
{
    return fn::ExpectCharset{expected};
}

/**
 * Creates a parser that expects the stream to
 * start with a character that is in the given charset.
 */
[[nodiscard]] constexpr auto expect_not(Charset expected) noexcept
{
    return fn::ExpectCharset{expected.opposite()};
}

/**
 * Creates a parser that expects the stream to
 * start with a character that is in one the given char ranges.
 */
template <std::same_as<CharRange> ...Ts>
[[nodiscard]] constexpr auto expect(CharRange first, Ts ...rest) noexcept
{
    return fn::ExpectCharRangeSet<1 + sizeof...(Ts)>{.charset_ranges = {first, rest...}};
}

/**
 * Creates an instance of fn::Sequence;
 * a combinator to combine multiple parsers
 * to parse a stream sequentially and consecutively.
 * 
 * @see parsi::fn::Sequence
 */
template <is_parser... Fs>
[[nodiscard]] constexpr auto sequence(Fs&&... parsers) noexcept
{
    return internal::optimize(fn::Sequence<std::remove_cvref_t<Fs>...>(std::forward<Fs>(parsers)...));
}

/**
 * Creates a parser by combining the given `parsers`
 * where the result of only the one that succeeds
 * or the result of last one that fails will be returned.
 * 
 * @see fn::AnyOf
 */
template <is_parser... Fs>
[[nodiscard]] constexpr auto anyof(Fs&&... parsers) noexcept
{
    return internal::optimize(fn::AnyOf<std::remove_cvref_t<Fs>...>(std::forward<Fs>(parsers)...));
}

/**
 * Creates a repeated parser combinator,
 * a combinator which combines the given parser repeatedly
 * for a given minimum and maximum times.
 * 
 * By default, the range is from 0 to almost infinite,
 * expecting the parser to be repeated from none at all to any amount of times.
 * 
 * @see fn::Repeated
 */
template <std::size_t Min = 0, std::size_t Max = std::numeric_limits<std::size_t>::max(),
          is_parser F>
[[nodiscard]] constexpr auto repeat(F&& parser) noexcept
{
    return internal::optimize(fn::Repeated<std::remove_cvref_t<F>, Min, Max>{std::forward<F>(parser)});
}

/**
 * Creates a parser that repeats the given `parser`
 * with itself consecutively for exactly `count` times.
 * 
 * @see fn::RepeatedRanged
 */
template <is_parser F>
[[nodiscard]] constexpr auto repeat(F&& parser, std::size_t count) noexcept
{
    return internal::optimize(fn::RepeatedRanged<std::remove_cvref_t<F>>{std::forward<F>(parser), count, count});
}

/**
 * Creates a parser that repeats the given `parser`
 * with itself consecutively for at least `min` times and maximum `max` times.
 * 
 * If the parser can successfully parse the stream consecutively
 * within `min` and `max` range, then the parsing will be valid,
 * otherwise parsing will fail.
 * 
 * @see fn::RepeatedRanged
 */
template <is_parser F>
[[nodiscard]] constexpr auto repeat(F&& parser, std::size_t min, std::size_t max) noexcept
{
    return internal::optimize(fn::RepeatedRanged<std::remove_cvref_t<F>>{std::forward<F>(parser), min, max});
}

/**
 * Creates a `parser` that extracts (non-owning) the portion
 * that was successfully parsed with the given `parser`,
 * and passes the subspan portion to the given `visitor`.
 * 
 * @see fn::Extract
 */
template <is_parser F, std::invocable<std::string_view> G>
[[nodiscard]] constexpr auto extract(F&& parser, G&& visitor) noexcept
{
    return internal::optimize(fn::Extract<std::remove_cvref_t<F>, std::remove_cvref_t<G>>{
        std::forward<F>(parser),
        std::forward<G>(visitor)
    });
}

/**
 * Creates an optional parser out of given `parser`
 * that will return a valid succeeded result with
 * the original stream if the `parser` fails.
 * 
 * @see fn::Optional
 */
template <is_parser F>
[[nodiscard]] constexpr auto optional(F&& parser) noexcept
{
    return internal::optimize(fn::Optional<std::remove_cvref_t<F>>{std::forward<F>(parser)});
}

/**
 * Creates a parser that only checks whether
 * the given `parser` can parse the current
 * stream, but does not move the stream forward.
 * 
 * @see fn::Peek
 */
template <is_parser F>
[[nodiscard]] constexpr auto peek(F&& parser) noexcept
{
    return internal::optimize(fn::Peek<std::remove_cvref_t<F>>{std::forward<F>(parser)});
}

/**
 * Creates an alternative case for the `alterset` (alternative set) combinator.
 *
 * On itself, passes the result of the `condition` parser to the `successor` parser
 * if it was successful, otherwise returns the result of the `condition` parser.
 * 
 * @see fn::Alter
 */
template <is_parser ConditionF, is_parser SuccessorF>
static constexpr auto alter(ConditionF&& condition, SuccessorF&& successor) {
    return internal::optimize(fn::Alter<ConditionF, SuccessorF>(std::forward<ConditionF>(condition), std::forward<SuccessorF>(successor)));
}

/**
 * Creates a parser out of the given alternatives (`fn::Alter` instances) where
 * if the condition of an alternative fails, it goes to the next alternative,
 * otherwise it passes the result's stream to that alternative's successor parser
 * and returns its result regardless of failure or success.
 * 
 * @see fn::AlterSet
 */
template <is_parser ...Fs>
static constexpr auto alterset(Fs&& ...alternatives) {
    return internal::optimize(fn::AlterSet<Fs...>(std::forward<Fs&&>(alternatives)...));
}

}  // namespace parsi

#endif  // PARSI_PARSI_HPP
