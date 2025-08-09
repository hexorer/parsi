#include "parsi/parsi-c.h"

#include <cstddef>
#include <cstdlib>
#include <cstring>

struct parsi_compiled_parser {
    parsi_parser_t* parser;
};

static bool parsi_charset_check(parsi_charset_t* charset, char chr)
{
    const std::size_t idx = static_cast<std::size_t>(chr);
    const std::size_t bit = static_cast<std::size_t>(1) << (idx & 63ull);  // idx % 64
    return charset->bitset[idx >> 6ull] & bit;  // idx / 64
}

parsi_charset_t parsi_charset(const char* str)
{
    return parsi_charset_n(str, std::strlen(str));
}

parsi_charset_t parsi_charset_n(const char* str, size_t size)
{
    std::size_t bytes[4] = {0};
    for (std::size_t index = 0; index < size; ++index) {
        const unsigned char chr = static_cast<unsigned char>(str[index]);
        const std::size_t bit = static_cast<std::size_t>(1) << (chr & 63);  // chr % 64
        bytes[chr >> 6] |= bit;  // chr / 64
    }
    return parsi_charset_t{ .bitset = { bytes[0], bytes[1], bytes[2], bytes[3] } };
}

parsi_parser_t* parsi_alloc_parser(parsi_parser_t parser)
{
    parsi_parser_t* parser_ptr = (parsi_parser_t*)std::malloc(sizeof(parser));
    if (!parser_ptr)
    {
        return NULL;
    }
    std::memcpy(parser_ptr, &parser, sizeof(parser));
    return parser_ptr;
}

static void parsi_free_impl(parsi_parser_t* parser)
{
    switch (parser->type) {
        case parsi_parser_type_string:
            parser->expect_string.free_string_fn(parser->expect_string.string, parser->expect_string.size);
            break;

        case parsi_parser_type_sequence:
            for (std::size_t index = 0; index < parser->sequence.size; ++index) {
                parsi_free_impl(&(parser->sequence.parsers[index]));
            }
            if (parser->sequence.free_list_fn) {
                parser->sequence.free_list_fn(parser->sequence.parsers, parser->sequence.size);
            }
            break;

        case parsi_parser_type_anyof:
            for (std::size_t index = 0; index < parser->anyof.size; ++index) {
                parsi_free_impl(&(parser->anyof.parsers[index]));
            }
            if (parser->anyof.free_list_fn) {
                parser->anyof.free_list_fn(parser->anyof.parsers, parser->anyof.size);
            }
            break;

        case parsi_parser_type_repeat:
            if (parser->repeat.free_parser_fn)
            {
                parser->repeat.free_parser_fn(parser->repeat.parser);
            }
            break;

        case parsi_parser_type_optional:
            if (parser->optional.free_parser_fn)
            {
                parser->optional.free_parser_fn(parser->optional.parser);
            }
            break;

        case parsi_parser_type_extract:
            if (parser->extract.free_context_fn)
            {
                parser->extract.free_context_fn(parser->extract.context);
            }
            if (parser->extract.free_parser_fn)
            {
                parser->extract.free_parser_fn(parser->extract.parser);
            }
            break;

        case parsi_parser_type_custom:
            if (parser->custom.free_context_fn)
            {
                parser->custom.free_context_fn(parser->custom.context);
            }
            break;

        case parsi_parser_type_eos:
        case parsi_parser_type_char:
        case parsi_parser_type_charset:
        case parsi_parser_type_static_string:
        default:
            break;
    }
}

void parsi_free_parser(parsi_parser_t* parser)
{
    parsi_free_impl(parser);
    std::free(parser);
}

parsi_compiled_parser_t* parsi_compile(parsi_parser_t* parser)
{
    return new parsi_compiled_parser_t{parser};
}

void parsi_free_compiled_parser(parsi_compiled_parser_t* compiled_parser)
{
    delete compiled_parser;
}

static parsi_result_t parsi_parse_impl(parsi_parser_t* parser, parsi_stream_t stream)
{
    if (!parser) {
        return parsi_result_t{ .is_valid = false, .stream = stream };
    }

    switch (parser->type) {
        case parsi_parser_type_none:
            return parsi_result_t{ .is_valid = false, .stream = stream };

        case parsi_parser_type_custom:
            return parser->custom.func(parser->custom.context, stream);

        case parsi_parser_type_eos:
            return parsi_result_t{ .is_valid = (stream.size == 0), .stream = stream };

        case parsi_parser_type_char:
            if (stream.size >= 1 && *stream.cursor == parser->expect_char.expected) {
                return parsi_result_t{
                    .is_valid = true,
                    .stream = parsi_stream_t{ .cursor = stream.cursor + 1, .size = stream.size - 1 }
                };
            }
            return parsi_result_t{ .is_valid = false, .stream = stream };

        case parsi_parser_type_charset:
            if (stream.size >= 1 && parsi_charset_check(&parser->expect_charset.expected, *stream.cursor)) {
                return parsi_result_t{
                    .is_valid = true,
                    .stream = parsi_stream_t{ .cursor = stream.cursor + 1, .size = stream.size - 1 }
                };
            }
            return parsi_result_t{ .is_valid = false, .stream = stream };

        case parsi_parser_type_string:
            if (stream.size >= parser->expect_string.size
                    && strncmp(stream.cursor, parser->expect_string.string, parser->expect_string.size) == 0) {
                return parsi_result_t{
                    .is_valid = true,
                    .stream = parsi_stream_t{
                        .cursor = stream.cursor + parser->expect_string.size,
                        .size = stream.size - parser->expect_string.size
                    }
                };
            }
            return parsi_result_t{ .is_valid = false, .stream = stream };

        case parsi_parser_type_static_string:
            if (stream.size >= parser->expect_static_string.size
                    && strncmp(stream.cursor, parser->expect_static_string.string, parser->expect_static_string.size) == 0) {
                return parsi_result_t{
                    .is_valid = true,
                    .stream = parsi_stream_t{
                        .cursor = stream.cursor + parser->expect_static_string.size,
                        .size = stream.size - parser->expect_static_string.size
                    }
                };
            }
            return parsi_result_t{ .is_valid = false, .stream = stream };

        case parsi_parser_type_extract: {
            parsi_result_t result = parsi_parse_impl(parser->extract.parser, stream);
            if (result.is_valid) {
                result.is_valid = parser->extract.func(parser->extract.context, stream.cursor, stream.size - result.stream.size);
                return result;
            }
            return parsi_result_t{ .is_valid = false, .stream = result.stream };
        }

        case parsi_parser_type_sequence: {
            if (!parser->sequence.parsers) {
                return parsi_result_t{ .is_valid = true, .stream = stream };
            }

            parsi_result_t result = { .is_valid = true, .stream = stream };
            for (std::size_t index = 0; index < parser->sequence.size; ++index) {
                result = parsi_parse_impl(&parser->sequence.parsers[index], result.stream);
                if (!result.is_valid) {
                    return result;
                }
            }
            return result;
        }

        case parsi_parser_type_anyof: {
            if (!parser->anyof.parsers) {
                return parsi_result_t{ .is_valid = false, .stream = stream };
            }

            for (std::size_t index = 0; index < parser->anyof.size; ++index) {
                if (auto result = parsi_parse_impl(&parser->anyof.parsers[index], stream); result.is_valid) {
                    return result;
                }
            }

            return parsi_result_t{ .is_valid = false, .stream = stream };
        }

        case parsi_parser_type_repeat: {
            if (parser->repeat.min > parser->repeat.max) [[unlikely]] {
                return parsi_result_t{ .is_valid = false, .stream = stream };
            }

            std::size_t count = 0;
            parsi_result_t result = parsi_parse_impl(parser->repeat.parser, stream);
            while (result.is_valid) {
                ++count;

                if (count > parser->repeat.max) [[unlikely]] {
                    break;
                }

                stream = result.stream;
                result = parsi_parse_impl(parser->repeat.parser, stream);
            }

            if (count < parser->repeat.min || parser->repeat.max < count) [[unlikely]] {
                return parsi_result_t{ .is_valid = false, .stream = result.stream };
            }

            return parsi_result_t{ .is_valid = true, .stream = stream };
        }

        case parsi_parser_type_optional: {
            parsi_result_t result = parsi_parse_impl(parser->optional.parser, stream);
            if (result.is_valid) {
                return result;
            }
            return parsi_result_t{ .is_valid = true, .stream = stream };
        }
    }

    // this should be unreachable.
    return parsi_result_t{ .is_valid = false, .stream = stream };
}

parsi_result_t parsi_parse(parsi_compiled_parser_t* compiled_parser, parsi_stream_t stream)
{
    return parsi_parse_impl(compiled_parser->parser, stream);
}

//-- helpers

parsi_parser_t parsi_none()
{
    return parsi_parser_t{ .type = parsi_parser_type_none };
}

parsi_parser_t parsi_custom_parser(parsi_parser_fn_t func, void* context, parsi_free_fn_t free_context_fn)
{
    return parsi_parser_t{
        .type = parsi_parser_type_custom,
        .custom = {
            .func = func,
            .context = context,
            .free_context_fn = free_context_fn
        }
    };
}

parsi_parser_t parsi_expect_eos()
{
    return parsi_parser_t{ .type = parsi_parser_type_eos };
}

parsi_parser_t parsi_expect_char(char chr)
{
    return parsi_parser_t{
        .type = parsi_parser_type_char,
        .expect_char = { .expected = chr }
    };
}

parsi_parser_t parsi_expect_charset(parsi_charset_t charset)
{
    return parsi_parser_t{
        .type = parsi_parser_type_charset,
        .expect_charset = { .expected = charset }
    };
}

parsi_parser_t parsi_expect_charset_str(const char* charset_str)
{
    return parsi_parser_t{
        .type = parsi_parser_type_charset,
        .expect_charset = { .expected = parsi_charset(charset_str) }
    };
}

parsi_parser_t parsi_expect_string(char* str, size_t size, parsi_string_free_fn_t free_string_fn)
{
    return parsi_parser_t{
        .type = parsi_parser_type_string,
        .expect_string = {
            .string = str,
            .size = size,
            .free_string_fn = free_string_fn
        }
    };
}

parsi_parser_t parsi_expect_static_string(const char* str)
{
    return parsi_parser_t{
        .type = parsi_parser_type_static_string,
        .expect_static_string = { .string = str, .size = strlen(str) }
    };
}

parsi_parser_t parsi_combine_extract(parsi_parser_t* parser, parsi_extract_visitor_fn_t func, void* context, parsi_free_fn_t free_context_fn, parsi_parser_free_fn_t free_parser_fn)
{
    return parsi_parser_t{
        .type = parsi_parser_type_extract,
        .extract = {
            .parser = parser,
            .func = func,
            .context = context,
            .free_context_fn = free_context_fn,
            .free_parser_fn = free_parser_fn
        }
    };
}

parsi_parser_t parsi_combine_sequence(parsi_parser_t* parsers, parsi_parser_list_free_fn_t free_list_fn)
{
    if (!parsers) {
        return parsi_parser_t{ .type = parsi_parser_type_none };
    }

    size_t size = 0;
    parsi_parser_t* head = parsers;
    while (head->type != parsi_parser_type_none) {
        head = &parsers[++size];
    }

    return parsi_parser_t{
        .type = parsi_parser_type_sequence,
        .sequence = { .parsers = parsers, .size = size, .free_list_fn = free_list_fn }
    };
}

parsi_parser_t parsi_combine_sequence_n(parsi_parser_t* parsers, size_t size, parsi_parser_list_free_fn_t free_list_fn)
{
    return parsi_parser_t{
        .type = parsi_parser_type_sequence,
        .sequence = { .parsers = parsers, .size = size, .free_list_fn = free_list_fn }
    };
}

parsi_parser_t parsi_combine_anyof(parsi_parser_t* parsers, parsi_parser_list_free_fn_t free_list_fn)
{
    if (!parsers) {
        return parsi_parser_t{ .type = parsi_parser_type_none };
    }

    size_t size = 0;
    parsi_parser_t* head = parsers;
    while (head->type != parsi_parser_type_none) {
        head = &parsers[++size];
    }

    return parsi_parser_t{
        .type = parsi_parser_type_anyof,
        .anyof = { .parsers = parsers, .size = size, .free_list_fn = free_list_fn }
    };
}

parsi_parser_t parsi_combine_anyof_n(parsi_parser_t* parsers, size_t size, parsi_parser_list_free_fn_t free_list_fn)
{
    return parsi_parser_t{
        .type = parsi_parser_type_anyof,
        .anyof = { .parsers = parsers, .size = size, .free_list_fn = free_list_fn }
    };
}

parsi_parser_t parsi_combine_repeat(parsi_parser_t* parser, size_t min, size_t max, parsi_parser_free_fn_t free_parser_fn)
{
    return parsi_parser_t{
        .type = parsi_parser_type_repeat,
        .repeat = { .parser = parser, .min = min, .max = max, .free_parser_fn = free_parser_fn }
    };
}

parsi_parser_t parsi_combine_optional(parsi_parser_t* parser, parsi_parser_free_fn_t free_parser_fn)
{
    return parsi_parser_t{
        .type = parsi_parser_type_optional,
        .optional = { .parser = parser, .free_parser_fn = free_parser_fn }
    };
}
