#ifndef PARSI_PARSI_HPP
#define PARSI_PARSI_HPP

#include <iostream>  // TODO remove

#include <concepts>
#include <type_traits>
#include <string_view>
#include <numeric>
#include <bitset>
#include <utility>

namespace parsi {

struct Stream {
    // or std::span<char>
    std::string_view buffer;
    std::size_t cursor;

    constexpr Stream() noexcept
        : buffer()
        , cursor{0}
    {}

    constexpr Stream(const char* str) noexcept
        : Stream(std::string_view(str))
    {}

    constexpr Stream(std::string_view str) noexcept
        : buffer(str)
        , cursor(0)
    {}

    constexpr Stream(std::string_view str, std::size_t offset) noexcept
        : buffer(str)
        , cursor(offset)
    {}
};

struct Result {
    Stream stream;
    bool valid = false;

    constexpr operator bool() const noexcept { return valid; }
};

struct Charset {
    std::bitset<256> map;

    constexpr explicit Charset(std::string_view charset) noexcept
    {
        for (char character : charset) {
            map.set(static_cast<unsigned char>(character), true);
        }
    }
};

template <typename T>
concept is_parser = requires(T instance) {
    { std::forward<T>(instance)(std::declval<Stream>()) } -> std::convertible_to<Result>;
};

// visitors
namespace fn {

struct ExpectChar {
    char expected;

    constexpr Result operator()(Stream stream) const noexcept
    {
        if (stream.cursor >= stream.buffer.size()
                || stream.buffer[stream.cursor] != expected) {
            return Result{stream, false};
        }

        return Result{
            .stream = Stream(stream.buffer, stream.cursor + 1),
            .valid = true
        };
    }
};

struct ExpectCharset {
    Charset charset;

    constexpr Result operator()(Stream stream) const noexcept
    {
        if (stream.cursor >= stream.buffer.size()) {
            return Result{stream, false};
        }

        const char character = stream.buffer[stream.cursor];
        if (!charset.map.test(static_cast<unsigned char>(character))) {
            return Result{stream, false};
        }

        return Result{
            .stream = Stream(stream.buffer, stream.cursor + 1),
            .valid = true
        };
    }
};

struct ExpectString {
    std::string expected;

    Result operator()(Stream stream) const noexcept
    {
        if (!stream.buffer.substr(stream.cursor).starts_with(expected)) {
            return Result{stream, false};
        }

        return Result{
            .stream = Stream(stream.buffer, stream.cursor + expected.size()),
            .valid = true
        };
    }
};

template <is_parser ...Fs>
struct Sequence {
    std::tuple<Fs...> parsers;

    constexpr explicit Sequence(Fs ...parsers) noexcept
        : parsers(std::move(parsers)...)
    {}

    constexpr Result operator()(Stream stream) const noexcept
    {
        return std::apply([stream]<typename ...Ts>(Ts&& ...parsers) {
            return parse<Ts...>(stream, std::forward<Ts>(parsers)...);
        }, parsers);
    }

private:
    template <typename First, typename ...Rest>
    static constexpr Result parse(Stream stream, First&& first, Rest&& ...rest) noexcept
    {
        auto result = first(stream);
        if (!result) {
            return Result{result.stream, false};
        }

        if constexpr (sizeof...(Rest) == 0) {
            return result;
        } else {
            return parse<Rest...>(result.stream, std::forward<Rest>(rest)...);
        }
    }
};

template <is_parser F, std::invocable<std::string_view> G>
struct Visit {
    F parser;
    G visitor;

    constexpr Result operator()(Stream stream) const noexcept
    {
        auto result = parser(stream);
        if (result) {
            std::string_view buffer = stream.buffer;
            auto start = stream.cursor;
            auto end = result.stream.cursor;

            std::string_view substr = buffer.substr(start, end - start);

            if constexpr (requires { { visitor(substr) } -> std::same_as<bool>; }) {
                if (!visitor(substr)) {
                    return Result{result.stream, false};
                }
            } else {
                visitor(substr);
            }
        }

        return result;
    }
};

template <is_parser F>
struct Optional {
    F parser;

    constexpr Result operator()(Stream stream) const noexcept
    {
        auto result = parser(stream);
        if (!result) {
            return Result{stream, true};
        }

        return result;
    }
};

template <is_parser F, std::size_t Min = 0, std::size_t Max = std::numeric_limits<std::size_t>::max()>
struct Repeated {
    F parser;

    constexpr Result operator()(Stream stream) const noexcept
    {
        if constexpr (Max == 0) {
            return Result{stream, true};
        }

        Result last{stream, false};
        std::size_t count = 0;

        Result result = parser(stream);
        while (result.valid) {
            ++count;
            last = result;

            if (Max < count) {
                break;
            }

            result = parser(result.stream);
        }

        if (count < Min || Max < count) {
            return Result{last.stream, false};
        }

        return Result{last.stream, true};
    }
};

template <is_parser F>
struct RepeatedRanged {
    F parser;
    std::size_t min = 0;
    std::size_t max = std::numeric_limits<std::size_t>::max();

    constexpr Result operator()(Stream stream) const noexcept
    {
        if (min > max) {
            return Result{stream, false};
        }

        Result last;
        std::size_t count = 0;

        auto result = parser(stream);
        while (result.valid) {
            last = result;

            if (count > max) {
                break;
            }

            result = parser(stream);
        }

        if (count < min || max < count) {
            return Result{last.stream, false};
        }

        return Result{last.stream, true};
    }
};

}  // namespace fn

inline auto expect(std::string_view expected)
{
    return fn::ExpectString(std::string(expected));
}

inline auto expect(char expected)
{
    return fn::ExpectChar(expected);
}

inline auto expect(Charset expected)
{
    return fn::ExpectCharset(expected);
}

template <is_parser ...Fs>
inline auto sequence(Fs&& ...parsers)
{
    return fn::Sequence<Fs...>(std::forward<Fs>(parsers)...);
}

template <is_parser F>
inline auto optional(F&& parser)
{
    return fn::Optional<F>{std::forward<F>(parser)};
}

template <std::size_t Min = 0, std::size_t Max = std::numeric_limits<std::size_t>::max(), is_parser F>
inline auto repeat(F&& parser)
{
    return fn::Repeated<F, Min, Max>{std::forward<F>(parser)};
}

template <is_parser F>
inline auto repeat(F&& parser, std::size_t count)
{
    return fn::RepeatedRanged<F>{std::forward<F>(parser), count, count};
}

template <is_parser F>
inline auto repeat(F&& parser, std::size_t min, std::size_t max)
{
    return fn::RepeatedRanged<F>{std::forward<F>(parser), min, max};
}

template <is_parser F, std::invocable<std::string_view> G>
inline auto extract(F&& parser, G&& visitor)
{
    return fn::Visit{std::forward<F>(parser), std::forward<G>(visitor)};
}

}  // namespace parsi

#endif  // PARSI_PARSI_HPP
