#include <cassert>
#include <ctime>
#include <random>
#include <optional>

#include <benchmark/benchmark.h>

#include <parsi/parsi.hpp>
#include <parsi/parsi-c.h>
#include <ctre.hpp>

namespace helpers {

class ParsiCParser {
public:
    ParsiCParser(parsi_parser_t* parser) : _parser(parser)
    {
        _compiled_parser = parsi_compile(_parser);
    }

    ParsiCParser(const ParsiCParser& other) = delete;
    ParsiCParser& operator=(const ParsiCParser& other) = delete;

    ParsiCParser(ParsiCParser&& other) noexcept
        : _parser(std::exchange(other._parser, nullptr))
        , _compiled_parser(std::exchange(other._compiled_parser, nullptr))
    {}

    ParsiCParser& operator=(ParsiCParser&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }
        std::swap(_parser, other._parser);
        std::swap(_compiled_parser, other._compiled_parser);
        return *this;
    }

    ~ParsiCParser()
    {
        if (_compiled_parser)
        {
            parsi_free_compiled_parser(_compiled_parser);
        }

        if (_parser) {
            parsi_free_parser(_parser);
        }
    }

    bool operator()(std::string_view str) const
    {
        parsi_result_t result = parsi_parse(_compiled_parser, parsi_stream_t{.cursor = str.data(), .size = str.size()});
        return result.is_valid;
    }

private:
    parsi_parser_t* _parser = nullptr;
    parsi_compiled_parser_t* _compiled_parser = nullptr;
};

template <typename ...Ts>
static parsi_parser_t* new_parser_list(Ts ...parsers)
{
    return new (parsi_parser_t[]) { parsers... };
}

static void delete_parser_list(parsi_parser_t* list, size_t /* size */)
{
    delete[] list;
}

}  // namespace helpers

struct Color {
    std::uint8_t red = 0;
    std::uint8_t green = 0;
    std::uint8_t blue = 0;
};

constexpr auto convert_hex_digit = [](char digit) -> std::uint8_t {
    if ('0' <= digit && digit <= '9') {
        return digit - '0';
    }
    if ('a' <= digit && digit <= 'f') {
        return 10 + (digit - 'a');
    }
    if ('A' <= digit && digit <= 'F') {
        return 10 + (digit - 'A');
    }

    // unreachables as parser has verified it
    return 0;
};

constexpr auto raw_color_from_string(std::string_view str) -> std::optional<Color>
{
    if (str.size() != 7 || str[0] != '#') {
        return std::nullopt;
    }

    bool valid = true;
    for (std::size_t i = 1; i < 7; ++i) {
        const bool is_hex = ('0' <= str[i] && str[i] <= '9') || ('a' <= str[i] && str[i] <= 'f') || ('A' <= str[i] && str[i] <= 'F');
        if (!is_hex) [[unlikely]] {
            return std::nullopt;
        }
    }

    Color color;

    color.red = convert_hex_digit(str[1]) * 16 + convert_hex_digit(str[2]);
    color.green = convert_hex_digit(str[3]) * 16 + convert_hex_digit(str[4]);
    color.blue = convert_hex_digit(str[5]) * 16 + convert_hex_digit(str[6]);

    return color;
}

constexpr auto parsi_color_from_string(std::string_view str) -> std::optional<Color>
{
    Color color;

    constexpr auto color_parser = parsi::sequence(
        parsi::expect('#'),
        // a color code with 6 hex digits like `#C3A3BB` is equivalent to
        // Color{ .red = 0xC3, .green = 0xA3, .blue = 0xBB }
        parsi::repeat<6, 6>(parsi::expect(parsi::CharRange{'0', '9'}, parsi::CharRange{'a', 'f'}, parsi::CharRange{'A', 'F'})),  // min=6 and max=6
        parsi::eos()  // end of stream
    );

    const auto parser = parsi::extract(
        color_parser,
        [&](std::string_view str) {
            // str's length is guaranteed to be 7, with first character being '#',
            // and the rest of them are guaranteed to be in hex_charset.
            color.red = convert_hex_digit(str[1]) * 16 + convert_hex_digit(str[2]);
            color.green = convert_hex_digit(str[3]) * 16 + convert_hex_digit(str[4]);
            color.blue = convert_hex_digit(str[5]) * 16 + convert_hex_digit(str[6]);
        }
    );

    if (!parser(str)) {
        return std::nullopt;
    }

    return color;
}

constexpr auto ctre_color_from_string(std::string_view str) -> std::optional<Color>
{
    constexpr auto matcher = ctre::match<R"(^#([0-9a-fA-F]{2})([0-9a-fA-F]{2})([0-9a-fA-F]{2})$)">;

    auto match = matcher(str);
    if (!match) {
        return std::nullopt;
    }

    auto r_str = match.get<1>().to_view();
    auto g_str = match.get<2>().to_view();
    auto b_str = match.get<3>().to_view();

    Color color;

    color.red = convert_hex_digit(r_str[0]) * 16 + convert_hex_digit(r_str[1]);
    color.green = convert_hex_digit(b_str[0]) * 16 + convert_hex_digit(b_str[1]);
    color.blue = convert_hex_digit(g_str[0]) * 16 + convert_hex_digit(g_str[1]);

    return color;
}

static auto parsi_c_color_from_string(std::string_view str) -> std::optional<Color>
{
    static thread_local Color color;
    static helpers::ParsiCParser parser = [] {
        static parsi_charset_t hex_charset = parsi_charset("0123456789abcdefABCDEF");
        static parsi_parser_t hex_charset_parser = parsi_expect_charset(hex_charset);
        static parsi_parser_t sequence_subparsers[] = {
            parsi_expect_char('#'),
            // a color code with 6 hex digits like `#C3A3BB` is equivalent to
            // Color{ .red = 0xC3, .green = 0xA3, .blue = 0xBB }
            parsi_combine_repeat(&hex_charset_parser, 6, 6, nullptr),  // min=6 and max=6
            parsi_expect_eos(),  // end of stream
            parsi_none()
        };
        static parsi_parser_t color_parser = parsi_combine_sequence(sequence_subparsers, nullptr);
        static parsi_extract_visitor_fn_t extract_visitor_fn = [](void* context, const char* str, size_t size) -> bool {
            auto color = reinterpret_cast<Color*>(context);
            // str's length is guaranteed to be 7, with first character being '#',
            // and the rest of them are guaranteed to be in hex_charset.
            color->red = convert_hex_digit(str[1]) * 16 + convert_hex_digit(str[2]);
            color->green = convert_hex_digit(str[3]) * 16 + convert_hex_digit(str[4]);
            color->blue = convert_hex_digit(str[5]) * 16 + convert_hex_digit(str[6]);
            return true;
        };
        static parsi_parser_t raw_parser = parsi_combine_extract(&color_parser, extract_visitor_fn, &color, nullptr, nullptr);
        return helpers::ParsiCParser(parsi_alloc_parser(raw_parser));
    }();

    if (!parser(str))
    {
        return std::nullopt;
    }

    return color;
}

static void bench_color_hex(benchmark::State& state, auto&& parser)
{
    std::srand(std::time(nullptr));

    std::vector<std::string> colors;
    colors.reserve(1'000);
    for (std::size_t count = 0; count < colors.capacity(); ++count)
    {
        constexpr auto to_02x = [](std::uint8_t x) -> std::string {
            constexpr auto hex_chr_arr = "0123456789abcdef";
            return std::string("") + hex_chr_arr[x >> 4] + hex_chr_arr[x & 0x0f];
        };
        auto color = std::string("#") + to_02x(std::rand() % 256) + to_02x(std::rand() % 256) + to_02x(std::rand() % 256);
        colors.push_back(std::move(color));
    }

    std::size_t byte_count = 0;
    for (auto _ : state) {
        for (auto& color_str : colors) {
            auto res = parser(std::string_view(color_str));
            assert(!!res);
            benchmark::DoNotOptimize(res);
        }
        byte_count += colors.size() * 7;
    }

    state.SetBytesProcessed(byte_count);
}
BENCHMARK_CAPTURE(bench_color_hex, raw, raw_color_from_string);
BENCHMARK_CAPTURE(bench_color_hex, parsi, parsi_color_from_string);
BENCHMARK_CAPTURE(bench_color_hex, parsi-c, parsi_c_color_from_string);
BENCHMARK_CAPTURE(bench_color_hex, ctre, ctre_color_from_string);


static void bench_digits(benchmark::State& state, auto&& parser)
{
    std::string str;
    str.reserve(1'000'000);
    for (std::size_t i = 0; i < str.capacity(); ++i) {
        str += '9' - (i % 10);
    }

    std::size_t bytes_count = 0;

    for (auto _ : state) {
        auto res = parser(str.c_str());
        assert(!!res);
        benchmark::DoNotOptimize(res);
        bytes_count += str.size();
    }

    state.SetBytesProcessed(bytes_count);
}
BENCHMARK_CAPTURE(bench_digits, raw, [](parsi::Stream stream) {
    const char* const str = stream.data();
    const std::size_t size = stream.size();

    std::size_t index = 0;
    while (index < size && '0' <= str[index] && str[index] <= '9') {
        ++index;
    }
    return parsi::Result{parsi::Stream(str, index), true};
});
BENCHMARK_CAPTURE(bench_digits, parsi, parsi::repeat(parsi::expect(parsi::CharRange{'0', '9'})));
BENCHMARK_CAPTURE(bench_digits, parsi-c, helpers::ParsiCParser(
    parsi_alloc_parser(parsi_combine_repeat(parsi_alloc_parser(parsi_expect_charset(parsi_charset("0123456789"))), 0, -1, parsi_free_parser))
));
BENCHMARK_CAPTURE(bench_digits, ctre, ctre::match<R"(^[0-9]*)">);


constexpr auto optional_whitespaces = parsi::repeat(parsi::expect(parsi::Charset(" \n\t")));
constexpr auto expect_digits = parsi::repeat<1>(parsi::expect(parsi::CharRange{'0', '9'}));
constexpr auto expect_identifer = []() {
    return parsi::sequence(
        parsi::expect(parsi::CharRange{'a', 'z'}, parsi::CharRange{'A', 'Z'}, parsi::CharRange{'_', '_'}),
        parsi::repeat(parsi::expect(parsi::CharRange{'a', 'z'}, parsi::CharRange{'A', 'Z'}, parsi::CharRange{'0', '9'}, parsi::CharRange{'_', '_'}))
    );
}();
constexpr auto expect_item = parsi::anyof(
    parsi::extract(expect_digits, [](std::string_view token) { benchmark::DoNotOptimize(token); }),
    parsi::extract(expect_identifer, [](std::string_view token) { benchmark::DoNotOptimize(token); })
);
constexpr auto parsi_parser = parsi::sequence(
    parsi::expect('['),
    optional_whitespaces,
    parsi::optional(parsi::sequence(
        expect_item,
        parsi::repeat(parsi::sequence(
            optional_whitespaces,
            parsi::expect(','),
            optional_whitespaces,
            expect_item,
            optional_whitespaces
        ))
    )),
    parsi::expect(']'),
    parsi::eos()
);

static helpers::ParsiCParser parsi_c_parser = []() -> helpers::ParsiCParser {
    constexpr std::size_t size_t_max = std::numeric_limits<std::size_t>::max();
    constexpr auto dont_optimize_extract_visitor_fn = [](void*, const char* token, size_t) {
        benchmark::DoNotOptimize(token);
        return true;
    };

    static parsi_parser_t expect_whitespace = parsi_expect_charset(parsi_charset(" \n\t"));
    static parsi_parser_t expect_digit = parsi_expect_charset(parsi_charset("0123456789"));
    static parsi_parser_t optional_whitespaces = parsi_combine_repeat(&expect_whitespace, 0, size_t_max, nullptr);
    static parsi_parser_t expect_digits = parsi_combine_repeat(&expect_digit, 1, size_t_max, nullptr);

    static const parsi_charset_t identifier_first_charset = parsi_charset("_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
    static const parsi_charset_t identifier_rest_charset = parsi_charset("_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
    static parsi_parser_t expect_identifier_rest_charset = parsi_expect_charset(identifier_rest_charset);
    static parsi_parser_t expect_identifer = parsi_combine_sequence(
        helpers::new_parser_list(
            parsi_expect_charset(identifier_first_charset),
            parsi_combine_repeat(&expect_identifier_rest_charset, 0, size_t_max, nullptr),
            parsi_none()
        ),
        helpers::delete_parser_list
    );
    static parsi_parser_t expect_item = parsi_combine_anyof(
        helpers::new_parser_list(
            parsi_combine_extract(&expect_digits, dont_optimize_extract_visitor_fn, nullptr, nullptr, nullptr),
            parsi_combine_extract(&expect_identifer, dont_optimize_extract_visitor_fn, nullptr, nullptr, nullptr),
            parsi_none()
        ),
        helpers::delete_parser_list
    );
    static parsi_parser_t inner_inner_parser = parsi_combine_sequence(
        helpers::new_parser_list(
            optional_whitespaces,
            parsi_expect_char(','),
            optional_whitespaces,
            expect_item,
            optional_whitespaces,
            parsi_none()
        ),
        helpers::delete_parser_list
    );
    static parsi_parser_t inner_parser = parsi_combine_sequence(
        helpers::new_parser_list(
            expect_item,
            parsi_combine_repeat(&inner_inner_parser, 0, size_t_max, nullptr),
            parsi_none()
        ),
        helpers::delete_parser_list
    );
    static parsi_parser_t raw_parser = parsi_combine_sequence(
        helpers::new_parser_list(
            parsi_expect_char('['),
            optional_whitespaces,
            parsi_combine_optional(&inner_parser, nullptr),
            parsi_expect_char(']'),
            parsi_expect_eos(),
            parsi_none()
        ),
        helpers::delete_parser_list
    );
    return helpers::ParsiCParser(parsi_alloc_parser(raw_parser));
}();

constexpr auto ctre_parser = ctre::match<R"(^\[\s*(([0-9]+|[A-Za-z_]+[A-Za-z0-9_]*)\s*(,\s*([0-9]+|[A-Za-z_]+[A-Za-z0-9_]*)\s*)*)?\]$)">;

static void bench_many_items(benchmark::State& state, auto&& parser)
{
    auto str = [&state]() {
        std::string ret;
        ret.reserve(state.range(0));
        ret += '[';
        for (std::size_t i=0; i < state.range(0); ++i) {
            ret += "1234567890,   test,";
        }
        ret += "1";
        ret += ']';
        return ret;
    }();

    std::size_t bytes_count = 0;

    for (auto _ : state) {
        auto res = parser(str.c_str());
        assert(!!res);
        benchmark::DoNotOptimize(res);
        bytes_count += str.size();
    }

    state.SetBytesProcessed(bytes_count);
}
BENCHMARK_CAPTURE(bench_many_items, parsi, parsi_parser)->RangeMultiplier(10)->Range(100, 10'000'000);
BENCHMARK_CAPTURE(bench_many_items, parsi-c, parsi_c_parser)->RangeMultiplier(10)->Range(100, 10'000'000);
BENCHMARK_CAPTURE(bench_many_items, ctre, ctre_parser)->RangeMultiplier(10)->Range(100, 10'000'000);

BENCHMARK_MAIN();
