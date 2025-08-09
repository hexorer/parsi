#include <catch2/catch_all.hpp>

#include "parsi/parsi-c.h"

static std::string_view to_strview(parsi_stream_t stream)
{
    return std::string_view(stream.cursor, stream.size);
}

struct TResult {
    bool is_valid;
    std::string_view strview;
};

static constexpr parsi_stream_t make_stream(std::string_view strview)
{
    return parsi_stream_t{ .cursor = strview.data(), .size = strview.size() };
}

static constexpr bool operator==(const parsi_result_t& lhs, const TResult& rhs)
{
    return lhs.is_valid == rhs.is_valid && to_strview(lhs.stream) == rhs.strview;
}

static std::ostream& operator<<(std::ostream& stream, const parsi_result_t& result)
{
    stream << '{';
    stream << (result.is_valid ? "valid" : "invalid");
    stream << ',' << ' ';
    stream << '"' << std::string_view(result.stream.cursor, result.stream.size) << '"';
    stream << '}';
    return stream;
}

static std::ostream& operator<<(std::ostream& stream, const TResult& result)
{
    stream << '{';
    stream << (result.is_valid ? "valid" : "invalid");
    stream << ',' << ' ';
    stream << '"' << result.strview << '"';
    stream << '}';
    return stream;
}

TEST_CASE("c expect char")
{
    SECTION("type and value")
    {
        auto parser = parsi_expect_char('a');
        REQUIRE(parser.type == parsi_parser_type_char);
        CHECK(parser.expect_char.expected == 'a');
    }

    SECTION("whitespace")
    {
        auto parser = parsi_expect_char(' ');
        REQUIRE(parser.type == parsi_parser_type_char);
        CHECK(parser.expect_char.expected == ' ');
    }

    SECTION("NUL character")
    {
        auto parser = parsi_expect_char('\0');
        REQUIRE(parser.type == parsi_parser_type_char);
        CHECK(parser.expect_char.expected == '\0');
    }

    SECTION("parse")
    {
        auto parser = parsi_expect_char('X');
        auto compiled_parser = parsi_compile(&parser);
        CHECK(parsi_parse(compiled_parser, parsi_stream_t{ .cursor = "X", .size = 1 }).is_valid);
        CHECK(!parsi_parse(compiled_parser, parsi_stream_t{ .cursor = "x", .size = 1 }).is_valid);
        CHECK(!parsi_parse(compiled_parser, parsi_stream_t{ .cursor = "xX", .size = 2 }).is_valid);
        CHECK(!parsi_parse(compiled_parser, parsi_stream_t{ .cursor = "", .size = 0 }).is_valid);
        parsi_free_compiled_parser(compiled_parser);
    }
}

TEST_CASE("c expect charset")
{
    SECTION("charset and charset_n")
    {
        std::string_view charset_chars = "0123456789ABCDEFabcdef";
        auto first = parsi_charset(charset_chars.data());
        auto second = parsi_charset_n(charset_chars.data(), charset_chars.size());
        for (std::size_t index = 0; index < std::size(first.bitset); ++index)
        {
            CHECK(first.bitset[index] == second.bitset[index]);
        }
    }

    SECTION("NUL-character")
    {
        auto first_parser = parsi_expect_charset(parsi_charset("a\0b"));
        auto second_parser = parsi_expect_charset(parsi_charset_n("a\0b", 3));

        auto first_compiled_parser = parsi_compile(&first_parser);
        auto second_compiled_parser = parsi_compile(&second_parser);

        // TODO
        // CHECK(!parsi_parse(first_compiled_parser, parsi_stream_t{ .cursor = "\0", .size = 1 }).is_valid);
        CHECK(parsi_parse(second_compiled_parser, parsi_stream_t{ .cursor = "a", .size = 1 }).is_valid);
        // CHECK(parsi_parse(second_compiled_parser, parsi_stream_t{ .cursor = "\0", .size = 1 }).is_valid);

        parsi_free_compiled_parser(first_compiled_parser);
        parsi_free_compiled_parser(second_compiled_parser);
    }

    SECTION("match")
    {
        std::string_view charset_chars = "0123456789ABCDEFabcdef";
        auto parser = parsi_expect_charset(parsi_charset(charset_chars.data()));
        auto compiled_parser = parsi_compile(&parser);

        for (std::size_t index = 0; index < charset_chars.size() - 1; ++index)
        {
            CHECK(parsi_parse(compiled_parser, parsi_stream_t{ .cursor = charset_chars.substr(index).data(), .size = 1 }).is_valid);
        }

        std::string_view some_non_charset_chars = "SGHJKIsghji []'\"\\|.,";
        for (std::size_t index = 0; index < some_non_charset_chars.size(); ++index)
        {
            CHECK(!parsi_parse(compiled_parser, parsi_stream_t{ .cursor = some_non_charset_chars.substr(index).data(), .size = 1 }).is_valid);
        }

        parsi_free_compiled_parser(compiled_parser);
    }
}

TEST_CASE("c expect static string")
{
    SECTION("parse")
    {
        auto parser = parsi_expect_static_string("hello");
        auto compiled_parser = parsi_compile(&parser);
        CHECK(parsi_parse(compiled_parser, parsi_stream_t{ .cursor = "hello", .size = 5 }).is_valid);
        CHECK(parsi_parse(compiled_parser, parsi_stream_t{ .cursor = "hello world", .size = 11 }).is_valid);
        CHECK(!parsi_parse(compiled_parser, parsi_stream_t{ .cursor = "bello world", .size = 11 }).is_valid);
        CHECK(!parsi_parse(compiled_parser, parsi_stream_t{ .cursor = "hell", .size = 4 }).is_valid);
        CHECK(!parsi_parse(compiled_parser, parsi_stream_t{ .cursor = " hello", .size = 4 }).is_valid);
        parsi_free_compiled_parser(compiled_parser);
    }
}

TEST_CASE("c extract")
{
    SECTION("parse")
    {
        struct context_type {
            std::string str = "";
            std::size_t visitor_call_counter = 0;
            std::size_t free_context_call_counter = 0;
        };

        context_type ctx;

        constexpr auto visitor_fn = [](void* context, const char* str, size_t size) -> bool {
            context_type& ctx = *reinterpret_cast<context_type*>(context);
            auto str_view = std::string_view(str, size);
            CHECK(str_view == "X");
            ctx.str += str_view;
            ++ctx.visitor_call_counter;
            return true;
        };

        constexpr auto free_context_fn = [](void* context) {
            context_type& ctx = *reinterpret_cast<context_type*>(context);
            ++ctx.free_context_call_counter;
        };

        constexpr auto free_parser_fn = [](parsi_parser_t* parser) {
            CHECK(parser->type == parsi_parser_type_char);
            CHECK(parser->expect_char.expected == 'X');
        };

        auto sub_parser = parsi_expect_char('X');
        auto parser = parsi_alloc_parser(parsi_combine_extract(&sub_parser, visitor_fn, &ctx, free_context_fn, free_parser_fn));
        auto compiled_parser = parsi_compile(parser);

        CHECK(parsi_parse(compiled_parser, parsi_stream_t{ .cursor = "Xx12", .size = 4 }).is_valid);
        CHECK(parsi_parse(compiled_parser, parsi_stream_t{ .cursor = "X", .size = 1 }).is_valid);
        CHECK(!parsi_parse(compiled_parser, parsi_stream_t{ .cursor = "x", .size = 1 }).is_valid);
        CHECK(!parsi_parse(compiled_parser, parsi_stream_t{ .cursor = "xX", .size = 2 }).is_valid);

        parsi_free_parser(parser);
        parsi_free_compiled_parser(compiled_parser);

        CHECK(ctx.str == "XX");
        CHECK(ctx.visitor_call_counter == 2);
        CHECK(ctx.free_context_call_counter == 1);
    }
}

TEST_CASE("c eos")
{
    auto parser = parsi_expect_eos();
    CHECK(parser.type == parsi_parser_type_eos);

    auto compiled_parser = parsi_compile(&parser);

    CHECK(parsi_parse(compiled_parser, parsi_stream_t{ .cursor = "", .size = 0 }).is_valid);
    CHECK(parsi_parse(compiled_parser, parsi_stream_t{ .cursor = "test", .size = 0 }).is_valid);

    CHECK(!parsi_parse(compiled_parser, parsi_stream_t{ .cursor = "x", .size = 1 }).is_valid);
    CHECK(!parsi_parse(compiled_parser, parsi_stream_t{ .cursor = "test", .size = 4 }).is_valid);
    CHECK(!parsi_parse(compiled_parser, parsi_stream_t{ .cursor = "\0", .size = 1 }).is_valid);

    parsi_free_compiled_parser(compiled_parser);
}

TEST_CASE("c custom")
{
    struct context_t {
        std::string viewed = "";
        std::size_t parser_call_counter = 0;
        std::size_t free_context_call_counter = 0;
    };

    context_t ctx;

    constexpr auto parser_fn = [](void* context, parsi_stream_t stream) -> parsi_result_t {
        context_t& ctx = *reinterpret_cast<context_t*>(context);
        std::string_view strview = to_strview(stream);
        ctx.viewed += strview;
        ctx.parser_call_counter++;
        bool is_valid = strview == "World";
        if (is_valid) {
            stream.cursor += stream.size;
            stream.size = 0;
        }
        return parsi_result_t{ .is_valid = is_valid, .stream = stream };
    };

    constexpr auto free_context_fn = [](void* context) {
        context_t& ctx = *reinterpret_cast<context_t*>(context);
        ctx.free_context_call_counter++;
    };

    auto parser = parsi_alloc_parser(parsi_custom_parser(parser_fn, &ctx, free_context_fn));
    auto compiled_parser = parsi_compile(parser);

    {
        auto result = parsi_parse(compiled_parser, parsi_stream_t{ .cursor = "Hello", .size = 5 });
        CHECK(!result.is_valid);
        CHECK(to_strview(result.stream) == "Hello");
    }

    {
        auto result = parsi_parse(compiled_parser, parsi_stream_t{ .cursor = "World", .size = 5 });
        CHECK(result.is_valid);
        CHECK(to_strview(result.stream) == "");
    }

    parsi_free_compiled_parser(compiled_parser);
    parsi_free_parser(parser);

    CHECK(ctx.viewed == "HelloWorld");
    CHECK(ctx.parser_call_counter == 2);
    CHECK(ctx.free_context_call_counter == 1);
}

TEST_CASE("c expect string")
{
    char str[] = "hello";

    constexpr auto free_string_fn = [](char* str, size_t size) {
        str[0] = '1';
        str[1] = '2';
        str[2] = '3';
        str[3] = '4';
        str[4] = '5';
    };

    auto parser = parsi_alloc_parser(parsi_expect_string(str, std::strlen(str), free_string_fn));
    auto compiled_parser = parsi_compile(parser);

    CHECK(parsi_parse(compiled_parser, make_stream("hello")) == TResult{true, ""});
    CHECK(parsi_parse(compiled_parser, make_stream("Hello")) == TResult{false, "Hello"});
    CHECK(parsi_parse(compiled_parser, make_stream("hellO")) == TResult{false, "hellO"});
    CHECK(parsi_parse(compiled_parser, make_stream(" hello")) == TResult{false, " hello"});

    parsi_free_compiled_parser(compiled_parser);
    parsi_free_parser(parser);

    CHECK(std::string_view(str, std::strlen(str)) == "12345");
}

TEST_CASE("c repeat")
{
    SECTION("zero times")
    {
        auto inner_parser = parsi_expect_char('X');
        auto parser = parsi_combine_repeat(&inner_parser, 0, 0, NULL);
        auto compiled_parser = parsi_compile(&parser);

        CHECK(parsi_parse(compiled_parser, make_stream("XXXX")) == TResult{false, "XXX"});
        CHECK(parsi_parse(compiled_parser, make_stream("YXXX")) == TResult{true, "YXXX"});

        parsi_free_compiled_parser(compiled_parser);
    }

    SECTION("never or once")
    {
        auto inner_parser = parsi_expect_char('X');
        auto parser = parsi_combine_repeat(&inner_parser, 0, 1, NULL);
        auto compiled_parser = parsi_compile(&parser);

        CHECK(parsi_parse(compiled_parser, make_stream("XXXX")) == TResult{false, "XX"});
        CHECK(parsi_parse(compiled_parser, make_stream("XYZZ")) == TResult{true, "YZZ"});
        CHECK(parsi_parse(compiled_parser, make_stream("YXXX")) == TResult{true, "YXXX"});

        parsi_free_compiled_parser(compiled_parser);
    }

    SECTION("impossible min max")
    {
        auto inner_parser = parsi_expect_char('X');
        auto parser = parsi_combine_repeat(&inner_parser, 4, 2, NULL);
        auto compiled_parser = parsi_compile(&parser);

        CHECK(parsi_parse(compiled_parser, make_stream("")) == TResult{false, ""});
        CHECK(parsi_parse(compiled_parser, make_stream("XX")) == TResult{false, "XX"});
        CHECK(parsi_parse(compiled_parser, make_stream("XXX")) == TResult{false, "XXX"});
        CHECK(parsi_parse(compiled_parser, make_stream("XXXX")) == TResult{false, "XXXX"});
        CHECK(parsi_parse(compiled_parser, make_stream("XXXXX")) == TResult{false, "XXXXX"});
        CHECK(parsi_parse(compiled_parser, make_stream("XYZX")) == TResult{false, "XYZX"});
        CHECK(parsi_parse(compiled_parser, make_stream("YXXX")) == TResult{false, "YXXX"});

        parsi_free_compiled_parser(compiled_parser);
    }

    SECTION("same min max")
    {
        auto inner_parser = parsi_expect_char('X');
        auto parser = parsi_combine_repeat(&inner_parser, 4, 4, NULL);
        auto compiled_parser = parsi_compile(&parser);


        // going through 1*'X' to 6*'X'
        CHECK(parsi_parse(compiled_parser, make_stream("")) == TResult{false, ""});
        CHECK(parsi_parse(compiled_parser, make_stream("X")) == TResult{false, ""});
        CHECK(parsi_parse(compiled_parser, make_stream("XX")) == TResult{false, ""});
        CHECK(parsi_parse(compiled_parser, make_stream("XXX")) == TResult{false, ""});
        CHECK(parsi_parse(compiled_parser, make_stream("XXXX")) == TResult{true, ""}); // NOTE: 4*'X'
        CHECK(parsi_parse(compiled_parser, make_stream("XXXXX")) == TResult{false, ""});
        CHECK(parsi_parse(compiled_parser, make_stream("XXXXXX")) == TResult{false, "X"});
        CHECK(parsi_parse(compiled_parser, make_stream("XXXXXXX")) == TResult{false, "XX"});

        CHECK(parsi_parse(compiled_parser, make_stream("")) == TResult{false, ""});
        CHECK(parsi_parse(compiled_parser, make_stream("YXXXX")) == TResult{false, "YXXXX"});

        parsi_free_compiled_parser(compiled_parser);
    }

    SECTION("min max diff by one")
    {
        auto inner_parser = parsi_expect_char('X');
        auto parser = parsi_combine_repeat(&inner_parser, 3, 4, NULL);
        auto compiled_parser = parsi_compile(&parser);

        CHECK(parsi_parse(compiled_parser, make_stream("XXX")) == TResult{true, ""});
        CHECK(parsi_parse(compiled_parser, make_stream("XXXX")) == TResult{true, ""});

        CHECK(parsi_parse(compiled_parser, make_stream("XXXY")) == TResult{true, "Y"});
        CHECK(parsi_parse(compiled_parser, make_stream("XXXXY")) == TResult{true, "Y"});

        CHECK(parsi_parse(compiled_parser, make_stream("")) == TResult{false, ""});
        CHECK(parsi_parse(compiled_parser, make_stream("X")) == TResult{false, ""});
        CHECK(parsi_parse(compiled_parser, make_stream("XX")) == TResult{false, ""});
        CHECK(parsi_parse(compiled_parser, make_stream("XXXXX")) == TResult{false, ""});
        CHECK(parsi_parse(compiled_parser, make_stream("XXXXXX")) == TResult{false, "X"});
        CHECK(parsi_parse(compiled_parser, make_stream("XXXXXXX")) == TResult{false, "XX"});

        CHECK(parsi_parse(compiled_parser, make_stream("Y")) == TResult{false, "Y"});
        CHECK(parsi_parse(compiled_parser, make_stream("XY")) == TResult{false, "Y"});
        CHECK(parsi_parse(compiled_parser, make_stream("XXY")) == TResult{false, "Y"});
        CHECK(parsi_parse(compiled_parser, make_stream("XXXXXY")) == TResult{false, "Y"});

        CHECK(parsi_parse(compiled_parser, make_stream("YXXX")) == TResult{false, "YXXX"});
        CHECK(parsi_parse(compiled_parser, make_stream("YXXXX")) == TResult{false, "YXXXX"});

        parsi_free_compiled_parser(compiled_parser);
    }

    SECTION("fixed range")
    {
        auto inner_parser = parsi_expect_char('X');
        auto parser = parsi_combine_repeat(&inner_parser, 4, 6, NULL);
        auto compiled_parser = parsi_compile(&parser);

        CHECK(parsi_parse(compiled_parser, make_stream("XXXX")) == TResult{true, ""});
        CHECK(parsi_parse(compiled_parser, make_stream("XXXXX")) == TResult{true, ""});
        CHECK(parsi_parse(compiled_parser, make_stream("XXXXXX")) == TResult{true, ""});

        CHECK(parsi_parse(compiled_parser, make_stream("XXXXY")) == TResult{true, "Y"});
        CHECK(parsi_parse(compiled_parser, make_stream("XXXXXY")) == TResult{true, "Y"});
        CHECK(parsi_parse(compiled_parser, make_stream("XXXXXXY")) == TResult{true, "Y"});

        CHECK(parsi_parse(compiled_parser, make_stream("")) == TResult{false, ""});
        CHECK(parsi_parse(compiled_parser, make_stream("X")) == TResult{false, ""});
        CHECK(parsi_parse(compiled_parser, make_stream("XX")) == TResult{false, ""});
        CHECK(parsi_parse(compiled_parser, make_stream("XXX")) == TResult{false, ""});
        CHECK(parsi_parse(compiled_parser, make_stream("XXXXXXX")) == TResult{false, ""});
        CHECK(parsi_parse(compiled_parser, make_stream("XXXXXXXX")) == TResult{false, "X"});
        CHECK(parsi_parse(compiled_parser, make_stream("XXXXXXXXX")) == TResult{false, "XX"});

        CHECK(parsi_parse(compiled_parser, make_stream("Y")) == TResult{false, "Y"});
        CHECK(parsi_parse(compiled_parser, make_stream("XY")) == TResult{false, "Y"});
        CHECK(parsi_parse(compiled_parser, make_stream("XXY")) == TResult{false, "Y"});
        CHECK(parsi_parse(compiled_parser, make_stream("XXXY")) == TResult{false, "Y"});
        CHECK(parsi_parse(compiled_parser, make_stream("XXXXXXXY")) == TResult{false, "Y"});

        CHECK(parsi_parse(compiled_parser, make_stream("YXXXX")) == TResult{false, "YXXXX"});
        CHECK(parsi_parse(compiled_parser, make_stream("YXXXXX")) == TResult{false, "YXXXXX"});
        CHECK(parsi_parse(compiled_parser, make_stream("YXXXXXX")) == TResult{false, "YXXXXXX"});

        parsi_free_compiled_parser(compiled_parser);
    }

    SECTION("free parser")
    {
        constexpr auto free_parser_fn = [](parsi_parser_t* parser) {
            REQUIRE(parser);
            REQUIRE(parser->type == parsi_parser_type_char);
            REQUIRE(parser->expect_char.expected == 'F');
            parser->expect_char.expected = 'T';
        };

        auto inner_parser = parsi_expect_char('F');
        auto parser = parsi_alloc_parser(parsi_combine_repeat(&inner_parser, 1, 1, free_parser_fn));
        auto compiled_parser = parsi_compile(parser);

        CHECK(parsi_parse(compiled_parser, make_stream("F")) == TResult{true, ""});
        CHECK(parsi_parse(compiled_parser, make_stream("T")) == TResult{false, "T"});

        parsi_free_parser(parser);
        parsi_free_compiled_parser(compiled_parser);

        REQUIRE(inner_parser.type == parsi_parser_type_char);
        CHECK(inner_parser.expect_char.expected == 'T');
    }
}

TEST_CASE("c sequence")
{
    SECTION("null parsers")
    {
        auto parser = parsi_alloc_parser(parsi_combine_sequence_n(NULL, 0, NULL));
        auto compiled_parser = parsi_compile(parser);

        CHECK(parsi_parse(compiled_parser, make_stream("")) == TResult{true, ""});
        CHECK(parsi_parse(compiled_parser, make_stream("X")) == TResult{true, "X"});
        CHECK(parsi_parse(compiled_parser, make_stream("XX")) == TResult{true, "XX"});

        parsi_free_compiled_parser(compiled_parser);
        parsi_free_parser(parser);
    }

    SECTION("zero parsers")
    {
        parsi_parser_t subparsers[] = {parsi_none()};
        auto parser = parsi_alloc_parser(parsi_combine_sequence(subparsers, NULL));
        auto compiled_parser = parsi_compile(parser);

        CHECK(parsi_parse(compiled_parser, make_stream("")) == TResult{true, ""});
        CHECK(parsi_parse(compiled_parser, make_stream("X")) == TResult{true, "X"});
        CHECK(parsi_parse(compiled_parser, make_stream("XX")) == TResult{true, "XX"});

        parsi_free_compiled_parser(compiled_parser);
        parsi_free_parser(parser);
    }

    SECTION("one parser")
    {
        parsi_parser_t subparsers[] = {parsi_expect_char('X'), parsi_none()};
        auto parser = parsi_alloc_parser(parsi_combine_sequence(subparsers, NULL));
        auto compiled_parser = parsi_compile(parser);

        CHECK(parsi_parse(compiled_parser, make_stream("")) == TResult{false, ""});
        CHECK(parsi_parse(compiled_parser, make_stream("X")) == TResult{true, ""});
        CHECK(parsi_parse(compiled_parser, make_stream("XX")) == TResult{true, "X"});

        parsi_free_compiled_parser(compiled_parser);
        parsi_free_parser(parser);
    }

    SECTION("multiple parsers")
    {
        parsi_parser_t subparsers[] = {parsi_expect_char('X'), parsi_expect_char('Y'), parsi_none()};
        auto parser = parsi_alloc_parser(parsi_combine_sequence(subparsers, NULL));
        auto compiled_parser = parsi_compile(parser);

        CHECK(parsi_parse(compiled_parser, make_stream("")) == TResult{false, ""});
        CHECK(parsi_parse(compiled_parser, make_stream("X")) == TResult{false, ""});
        CHECK(parsi_parse(compiled_parser, make_stream("XX")) == TResult{false, "X"});
        CHECK(parsi_parse(compiled_parser, make_stream("XXY")) == TResult{false, "XY"});

        CHECK(parsi_parse(compiled_parser, make_stream("XY")) == TResult{true, ""});
        CHECK(parsi_parse(compiled_parser, make_stream("XYX")) == TResult{true, "X"});
        CHECK(parsi_parse(compiled_parser, make_stream("XYXY")) == TResult{true, "XY"});

        parsi_free_compiled_parser(compiled_parser);
        parsi_free_parser(parser);
    }

    SECTION("array free callback")
    {
        struct context_type {
            parsi_parser* parsers = nullptr;
            std::size_t size = 0;
        };
        constexpr auto free_list_fn = [](parsi_parser* parsers, size_t size) {
            static std::size_t visit_count = 0;
            REQUIRE(++visit_count == 1);
            REQUIRE(parsers);
            REQUIRE(size > 0);
            REQUIRE(parsers[0].type == parsi_parser_type_custom);
            context_type& ctx = *reinterpret_cast<context_type*>(parsers[0].custom.context);
            CHECK(!ctx.parsers);
            CHECK(ctx.size == 0);
            ctx.parsers = parsers;
            ctx.size = size;
        };
        auto ctx = context_type{};
        parsi_parser_t subparsers[] = {parsi_custom_parser(NULL, &ctx, NULL), parsi_none()};
        auto parser = parsi_alloc_parser(parsi_combine_sequence(subparsers, free_list_fn));
        parsi_free_parser(parser);
        CHECK(ctx.parsers == subparsers);
        CHECK(ctx.size == 1);
    }
}

TEST_CASE("c anyof")
{
    SECTION("null parsers")
    {
        auto parser = parsi_alloc_parser(parsi_combine_anyof_n(NULL, 0, NULL));
        auto compiled_parser = parsi_compile(parser);

        CHECK(parsi_parse(compiled_parser, make_stream("")) == TResult{false, ""});
        CHECK(parsi_parse(compiled_parser, make_stream("X")) == TResult{false, "X"});
        CHECK(parsi_parse(compiled_parser, make_stream("XX")) == TResult{false, "XX"});

        parsi_free_compiled_parser(compiled_parser);
        parsi_free_parser(parser);
    }

    SECTION("zero parsers")
    {
        parsi_parser_t subparsers[] = {parsi_none()};
        auto parser = parsi_alloc_parser(parsi_combine_anyof(subparsers, NULL));
        auto compiled_parser = parsi_compile(parser);

        CHECK(parsi_parse(compiled_parser, make_stream("")) == TResult{false, ""});
        CHECK(parsi_parse(compiled_parser, make_stream("X")) == TResult{false, "X"});
        CHECK(parsi_parse(compiled_parser, make_stream("ZX")) == TResult{false, "ZX"});

        parsi_free_compiled_parser(compiled_parser);
        parsi_free_parser(parser);
    }

    SECTION("one parser")
    {
        parsi_parser_t subparsers[] = {parsi_expect_char('X'), parsi_none()};
        auto parser = parsi_alloc_parser(parsi_combine_anyof(subparsers, NULL));
        auto compiled_parser = parsi_compile(parser);

        CHECK(parsi_parse(compiled_parser, make_stream("")) == TResult{false, ""});
        CHECK(parsi_parse(compiled_parser, make_stream("ZYX")) == TResult{false, "ZYX"});
        CHECK(parsi_parse(compiled_parser, make_stream("ZXY")) == TResult{false, "ZXY"});

        CHECK(parsi_parse(compiled_parser, make_stream("X")) == TResult{true, ""});
        CHECK(parsi_parse(compiled_parser, make_stream("XX")) == TResult{true, "X"});
        CHECK(parsi_parse(compiled_parser, make_stream("XXX")) == TResult{true, "XX"});

        parsi_free_compiled_parser(compiled_parser);
        parsi_free_parser(parser);
    }

    SECTION("multiple parsers")
    {
        parsi_parser_t subparsers[] = {parsi_expect_char('X'), parsi_expect_char('Y'), parsi_none()};
        auto parser = parsi_alloc_parser(parsi_combine_anyof(subparsers, NULL));
        auto compiled_parser = parsi_compile(parser);

        CHECK(parsi_parse(compiled_parser, make_stream("")) == TResult{false, ""});
        CHECK(parsi_parse(compiled_parser, make_stream("Z")) == TResult{false, "Z"});
        CHECK(parsi_parse(compiled_parser, make_stream("ZZ")) == TResult{false, "ZZ"});
        CHECK(parsi_parse(compiled_parser, make_stream("ZX")) == TResult{false, "ZX"});
        CHECK(parsi_parse(compiled_parser, make_stream("ZY")) == TResult{false, "ZY"});

        CHECK(parsi_parse(compiled_parser, make_stream("X")) == TResult{true, ""});
        CHECK(parsi_parse(compiled_parser, make_stream("XX")) == TResult{true, "X"});
        CHECK(parsi_parse(compiled_parser, make_stream("XXY")) == TResult{true, "XY"});
        CHECK(parsi_parse(compiled_parser, make_stream("Y")) == TResult{true, ""});
        CHECK(parsi_parse(compiled_parser, make_stream("YX")) == TResult{true, "X"});
        CHECK(parsi_parse(compiled_parser, make_stream("YXY")) == TResult{true, "XY"});

        parsi_free_compiled_parser(compiled_parser);
        parsi_free_parser(parser);
    }

    SECTION("array free callback")
    {
        struct context_type {
            parsi_parser* parsers = nullptr;
            std::size_t size = 0;
        };
        constexpr auto free_list_fn = [](parsi_parser* parsers, size_t size) {
            static std::size_t visit_count = 0;
            REQUIRE(++visit_count == 1);
            REQUIRE(parsers);
            REQUIRE(size > 0);
            REQUIRE(parsers[0].type == parsi_parser_type_custom);
            context_type& ctx = *reinterpret_cast<context_type*>(parsers[0].custom.context);
            CHECK(!ctx.parsers);
            CHECK(ctx.size == 0);
            ctx.parsers = parsers;
            ctx.size = size;
        };
        auto ctx = context_type{};
        parsi_parser_t subparsers[] = {parsi_custom_parser(NULL, &ctx, NULL), parsi_none()};
        auto parser = parsi_alloc_parser(parsi_combine_anyof(subparsers, free_list_fn));
        parsi_free_parser(parser);
        CHECK(ctx.parsers == subparsers);
        CHECK(ctx.size == 1);
    }
}

TEST_CASE("c optional")
{
    SECTION("always valid")
    {
        auto subparser = parsi_expect_char('X');
        auto parser = parsi_combine_optional(&subparser, NULL);
        auto compiled_parser = parsi_compile(&parser);

        CHECK(parsi_parse(compiled_parser, make_stream("")) == TResult{true, ""});
        CHECK(parsi_parse(compiled_parser, make_stream("Y")) == TResult{true, "Y"});
        CHECK(parsi_parse(compiled_parser, make_stream("YY")) == TResult{true, "YY"});

        CHECK(parsi_parse(compiled_parser, make_stream("X")) == TResult{true, ""});
        CHECK(parsi_parse(compiled_parser, make_stream("XY")) == TResult{true, "Y"});

        parsi_free_compiled_parser(compiled_parser);
    }

    SECTION("free callback")
    {
        auto free_parser_fn = [](parsi_parser* parser) {
            REQUIRE(parser);
            REQUIRE(parser->type == parsi_parser_type_char);
            CHECK(parser->expect_char.expected == 'X');
            parser->expect_char.expected = 'Y';
        };
        auto subparser = parsi_expect_char('X');
        auto parser = parsi_alloc_parser(parsi_combine_optional(&subparser, free_parser_fn));
        auto compiled_parser = parsi_compile(parser);
        parsi_free_compiled_parser(compiled_parser);
        parsi_free_parser(parser);
        CHECK(subparser.expect_char.expected == 'Y');
    }
}
