#ifndef PARSI_EXTRA_MSGPACK_HPP
#define PARSI_EXTRA_MSGPACK_HPP

#include <concepts>
#include <string>
#include <string_view>

#include <parsi/base.hpp>
#include <parsi/internal/utils.hpp>

namespace parsi::msgpack {

/** parse_map; expected abilities:
 * - given list of pairs of key+parser (compile-time, different type of parsers)
 *   - with inclusive specifier (default) call the right parser on sight and ignore others,
 *     if one of them isn't visited, then error.
 *     and if there are duplicate keys, then error.
 *   - with exclusive specifier, the same as inclusive but error on sight of keys that aren't in the list.
 * - for runtime variable length pairs of key+parser,
 *   the inclusive/exclusive specifiers remain the same,
 *   which could be provided by a range of key+parser pairs where key is string and parsers should have same type,
 *   or use a `visitor(key) -> parser` for more complex situations
 *   where the read key is provided to visitor to get specific parser for it.
 */

template <typename KeyCheckerF, typename ValueParserF>
struct FieldMatch {
    KeyCheckerF checker;
    ValueParserF parser;
    constexpr auto operator()(std::string_view key, Stream stream) const -> Result
    {
        if (!checker(key)) {
            return Result{stream, false};
        }
        return parser(stream);
    }
};

template <typename ParserF>
struct FieldAny {
    ParserF parser;
    constexpr auto operator()(std::string_view, Stream stream) const -> Result
    {
        return parser(stream);
    }
};

template <typename ...FieldParsers>
struct FieldParserAnyof {
    // TODO

    constexpr auto operator()(Stream stream) const -> Result
    {
        // TODO
    }
};

struct ExactKeyChecker {
    std::string expected_key;
    constexpr auto operator()(std::string_view key) const -> bool
    {
        return expected_key == key;
    }
};

template <typename KeyCheckerF, typename ValueParserF>
constexpr auto parser_field(KeyCheckerF&& checker, ValueParserF&& parser)
{
    using key_checker_type = std::remove_cvref_t<KeyCheckerF>;
    using value_parser_type = std::remove_cvref_t<ValueParserF>;
    return FieldMatch<key_checker_type, value_parser_type>{
        std::forward<KeyCheckerF>(checker),
        std::forward<ValueParserF>(parser)
    };
}

template <typename ValueParserF>
constexpr auto parser_field(std::string keystr, ValueParserF&& parser)
{
    using value_parser_type = std::remove_cvref_t<ValueParserF>;
    return FieldMatch<value_parser_type>{
        ExactKeyChecker{ .expected_key = std::move(keystr) },
        std::forward<ValueParserF>(parser)
    };
}

template <typename ValueParserF>
constexpr auto parser_field_any(ValueParserF&& parser)
{
    using value_parser_type = std::remove_cvref_t<ValueParserF>;
    return FieldAny{std::forward<ValueParserF>(parser)};
}

template <typename SizeVisitorF, typename ...FieldParserFs>
    // TODO require `size_visitor(size) -> bool`
    // TODO require `field_parser(std::string_view, Stream) -> Result`
constexpr auto parser_map(sizeVisitorF&& size_visitor, FieldParserFs&& ...field_parsers)
{
    // return sequence(
    //     FieldParserAnyof(std::forward<FieldParserFs>(field_parsers)...)
    // );
    return;
}

}  // namespace parsi::msgpack

#endif  // PARSI_EXTRA_MSGPACK_HPP
