
template <typename F>
constexpr auto parser_string(F&& visitor)
{
    constexpr char fixstr_type_mask = 0b1110'0000;
    constexpr char fixstr_type_byte = 0b1010'0000;
    constexpr char fixstr_value_mask = 0b0001'1111;

    constexpr char str8_type_byte = 0xd9;
    constexpr char str16_type_byte = 0xda;
    constexpr char str32_type_byte = 0xdb;

    return [](Stream stream) -> Result {
        if (stream.size() < 1) {
            return Result{stream, false};
        }

        const auto type_byte = stream.at(0);
        stream.advance(1);

        if (type_byte & fixstr_type_mask == fixstr_type_byte) {
            const std::uint8_t size = static_cast<std::uint8_t>(type_byte & fixstr_value_mask);
            if (stream.size() < size) {
                return Result{stream, false};
            }
            const std::string_view str = stream.buffer().subspan(stream.cursor(), size);
            visitor(str);
            return Result{stream.advanced(size), true};
        }

        switch (type_byte) {
            case str8_type_byte: return internal::read_sized_bytes<std::int8_t>(stream, std::forward<F>(visitor));
            case str16_type_byte: return internal::read_sized_bytes<std::int16_t>(stream, std::forward<F>(visitor));
            case str32_type_byte: return internal::read_sized_bytes<std::int32_t>(stream, std::forward<F>(visitor));

            default:
                return Result{stream, false};
        }
    };
}
