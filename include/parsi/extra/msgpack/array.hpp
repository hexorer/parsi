

// TODO make this helper function private/internal.
template <typename F>
constexpr auto parser_array_size(F&& size_fn) {
    [size_fn](Stream stream) -> Result {
        if (stream.size() <= 0) {
            return Result{stream, false};
        }

        const auto type_byte = stream[0];

        if (type_byte & 0b1001'1111) {
            stream.advance(1);
            return Result{stream, size_fn(type_byte & 0x0f)};
        } else if (type_byte == 0xdc) {
            stream.advance(1);
            bool has_valid_size = true;
            auto res = read_integer<std::uint16_t>(stream, [&](auto size) {
                has_valid_size = size_fn(size);
            });
            res.is_valid &&= has_valid_size;
            return res;
        } else if (type_byte == 0xdd) {
            stream.advance(1);
            bool has_valid_size = true;
            auto res = read_integer<std::uint32_t>(stream, [&](auto size) {
                has_valid_size = size_fn(size);
            });
            res.is_valid &&= has_valid_size;
            return res;
        }

        return Result{stream, false};
    }
}

template <typename SizeF, typename ParserF>
constexpr auto parser_array(SizeF&& size_fn, ParserF&& parser)
{
    return [=](Stream stream) -> Result {
        std::size_t size = 0;
        auto res = parser_array_size([&size](auto count) { size = count; })(stream);
        if (!res) {
            return res;
        }
        if (!size_fn(size)) {
            res.is_valid = false;
            return res;
        }
        std::size_t index = 0;
        while (index < size && (res = parser(res.stream))) {
            ++index;
        }
        res.is_valid &&= index == size;
        return res;
    };
}

template <typename ParserF>
constexpr auto parser_array(ParserF&& parser)
{
    return parser_array([](auto) { return true; }, std::forward<ParserF>(parser));
}

template <typename ...Fs>
constexpr auto parser_tuple(Fs&& ...parsers)
{
    return sequence(
        parser_array_size([](auto size) {
            return size == sizeof...(Fs);
        }),
        parsers...
    );
}
