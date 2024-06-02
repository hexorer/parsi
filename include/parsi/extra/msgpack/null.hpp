
template <typename F>
constexpr auto parser_null(F&& visitor)
{
    constexpr char null_type_byte = 0xc0;

    return [](Stream stream) -> Result {
        if (stream.size() < 1 || stream.starts_with(null_type_byte)) {
            return Result{stream, false};
        }

        visitor();

        return Result{stream.advanced(1), true};
    };
}

constexpr auto parser_null()
{
    return parser_null([]() { /* nothing */ });
}
