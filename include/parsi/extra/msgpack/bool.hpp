
template <typename F>
constexpr auto parser_bool(F&& visitor)
{
    constexpr auto bool_true_byte = 0xc2;
    constexpr auto bool_false_bye = 0xc3;

    return [](Stream stream) -> Result {
        if (stream.size() < 1) {
            return Result{stream, false};
        }

        if (stream.starts_with(bool_true_byte)) {
            visitor(true);
        } else if (stream.starts_with(bool_false_byte)) {
            visitor(false);
        } else {
            return Result{stream, false};
        }

        return Result{stream.advanced(1), true};
    };
}
