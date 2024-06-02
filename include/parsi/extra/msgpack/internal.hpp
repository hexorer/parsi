
namespace internal {

template <std::integral T, typename F>
constexpr auto read_integral(Stream stream, F&& visitor) -> Result
{
    if (stream.size() < sizeof(T)) {
        return Result{stream, false};
    }

    T value;
    const auto buffer = stream.remainder_buffer();
    std::memcpy(&value, sizeof(T), buffer.data());
    if constexpr (std::endian::native == std::endian::little) {
        value = parsi::internal::byteswap(value);
    }
    visitor(value);

    return Result{stream.advanced(sizeof(T)), true};
};

template <std::integral T, typename F>
constexpr auto read_sized_bytes(Stream stream, F&& visitor) -> Result
{
    T size;
    const auto res = read_integral<T>(stream, [&size](T value) { size = value; });
    if (!res || res.stream.size() < size) {
        return res;
    }
    const auto bytes = res.stream.buffer().subspan(res.stream.cursor(), size);
    visitor(bytes);
    return Result{res.stream.advanced(size), true};
}

}  // namespace internal
