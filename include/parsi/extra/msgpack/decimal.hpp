
template <typename F>
constexpr auto parser_decimal(F&& visitor)
{
    constexpr char f32_type_byte = 0xca;
    constexpr char f64_type_byte = 0xcb;

    return [](Stream stream) -> Result {
        if (stream.size() < 1) {
            return Result{stream, false};
        }

        const auto type_byte = stream.at(0);
        stream.advance(1);

        switch (type_byte) {
            case f32_type_byte: return read_integer<float>(stream, std::forward<F>(visitor));
            case f64_type_byte: return read_integer<double>(stream, std::forward<F>(visitor));
            default:
                return Result{stream, false};
        }
    };
}
