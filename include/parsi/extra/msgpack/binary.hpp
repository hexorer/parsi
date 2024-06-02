
template <typename F>
constexpr auto parser_binary(F&& visitor)
{
    constexpr char bin8_type_byte = 0xc4;
    constexpr char bin16_type_byte = 0xc5;
    constexpr char bin32_type_byte = 0xc6;

    return [](Stream stream) -> Result {
        if (stream.size() < 1) {
            return Result{stream, false};
        }

        const auto type_byte = stream.at(0);
        stream.advance(1);

        switch (type_byte) {
            case bin8_type_byte: return internal::read_sized_bytes<std::int8_t>(stream, std::forward<F>(visitor));
            case bin16_type_byte: return internal::read_sized_bytes<std::int16_t>(stream, std::forward<F>(visitor));
            case bin32_type_byte: return internal::read_sized_bytes<std::int32_t>(stream, std::forward<F>(visitor));

            default:
                return Result{stream, false};
        }
    };
}
