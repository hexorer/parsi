
template <typename F>
constexpr auto parser_integer(F&& visitor)
{
    constexpr char fixint_positive_type_mask = 0b1000'0000;
    constexpr char fixint_positive_type_byte = 0b0000'0000;
    constexpr char fixint_positive_value_mask = 0b0111'1111;

    constexpr char fixint_negative_type_mask = 0b1110'0000;
    constexpr char fixint_negative_type_byte = 0b1110'0000;
    constexpr char fixint_negative_value_mask = 0b0001'1111;

    constexpr char int8_type_byte = 0xcc;
    constexpr char int16_type_byte = 0xcd;
    constexpr char int32_type_byte = 0xd2;
    constexpr char int64_type_byte = 0xd3;

    constexpr char uint8_type_byte = 0xcc;
    constexpr char uint16_type_byte = 0xcd;
    constexpr char uint32_type_byte = 0xce;
    constexpr char uint64_type_byte = 0xcf;

    return [](Stream stream) -> Result {
        if (stream.size() < 1) {
            return Result{stream, false};
        }

        const auto type_byte = stream.at(0);
        stream.advance(1);

        if (type_byte & fixint_positive_type_mask == fixint_positive_type_byte) {  // fixint positive
            const std::uint8_t value = static_cast<std::uint8_t>(type_byte & fixint_negative_value_mask);
            visitor(value);
            return Result{stream, true};
        } else if (type_byte & fixint_negative_type_mask == fixint_negative_type_byte) {  // fixint negative
            const std::int8_t value = -static_cast<std::int8_t>(type_byte & fixint_negative_value_mask);
            visitor(value);
            return Result{stream, true};
        }

        switch (type_byte) {
            case int8_type_byte: return internal::read_integral<std::int8_t>(stream, std::forward<F>(visitor));
            case int16_type_byte: return internal::read_integral<std::int16_t>(stream, std::forward<F>(visitor));
            case int32_type_byte: return internal::read_integral<std::int32_t>(stream, std::forward<F>(visitor));
            case int64_type_byte: return internal::read_integral<std::int64_t>(stream, std::forward<F>(visitor));

            case uint8_type_byte: return internal::read_integral<std::uint8_t>(stream, std::forward<F>(visitor));
            case uint16_type_byte: return internal::read_integral<std::uint16_t>(stream, std::forward<F>(visitor));
            case uint32_type_byte: return internal::read_integral<std::uint32_t>(stream, std::forward<F>(visitor));
            case uint64_type_byte: return internal::read_integral<std::uint64_t>(stream, std::forward<F>(visitor));

            default:
                return Result{stream, false};
        }
    };
}
