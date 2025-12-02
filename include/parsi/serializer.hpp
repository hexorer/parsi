#ifndef PARSI_SERIALIZER_HPP
#define PARSI_SERIALIZER_HPP

#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>

namespace parsi::ser {

struct WriterResult {
    std::size_t offset;
    bool valid;

    [[nodiscard]] constexpr operator bool() const { return valid; }
};

struct WriterOpSizeTag {};
struct WriterOpFinalizeTag {};

struct Writer {
    std::span<std::uint8_t> span;

    template <typename OutputT>
    [[nodiscard]] WriterResult operator()(OutputT&& output) const {
        output.write(span);
        return WriterResult{.offset = output.offset(), .valid = true};
    }

    template <typename OutputT>
    constexpr void operator()(WriterOpFinalizeTag, OutputT) const {
        return;
    }

    [[nodiscard]] constexpr std::size_t operator()(WriterOpSizeTag) const {
        return span.size();
    }
};

template <typename ...Ts>
struct WriterSequence {
    static_assert(sizeof...(Ts) == 0, "not implemented.");
    static_assert(sizeof...(Ts) != 0, "not implemented.");
};

template <typename T>
struct WriterSequence<T> {
    T write;

    [[nodiscard]] WriterResult operator()(std::span<std::uint8_t> fullspan, std::size_t offset) const {
        return write(fullspan, offset);
    }

    constexpr void operator()(WriterOpFinalizeTag, std::span<std::uint8_t> fullspan, std::size_t offset) const {
        write(WriterOpFinalizeTag{});
        return;
    }

    [[nodiscard]] constexpr std::size_t operator()(WriterOpSizeTag) const {
        return write.size();
    }
};

template <typename T, typename U, typename ...Ts>
struct WriterSequence<T, U, Ts...> {
    T write;
    WriterSequence<U, Ts...> rest;

    [[nodiscard]] WriterResult operator()(std::span<std::uint8_t> fullspan, std::size_t offset) const {
        WriterResult result = write(fullspan, offset);
        if (!result) {
            return result;
        }
        return rest(fullspan, result.offset);
    }

    constexpr void operator()(WriterOpFinalizeTag, std::span<std::uint8_t> fullspan, std::size_t offset) const {
        write(WriterOpFinalizeTag{}, fullspan, offset);
        rest(WriterOpFinalizeTag{}, fullspan, offset + write(WriterOpSizeTag{}));
        return;
    }

    [[nodiscard]] constexpr std::size_t operator()(WriterOpSizeTag) const {
        return write(WriterOpSizeTag{}) + rest(WriterOpSizeTag{});
    }
};

struct MsgPackString {
    std::string_view str;

    [[nodiscard]] WriterResult operator()(std::span<std::uint8_t> fullspan, std::size_t offset) const {
        std::uint8_t* outptr = fullspan.data() + offset;
        if (str.size() >= (1ul << 16)) {
            outptr[0] = 0xdb;
            new (outptr + 1) std::uint32_t(str.size());
            std::memcpy(outptr + 5, str.data(), str.size());
            return WriterResult{offset + 5 + str.size(), true};
        } else if (str.size() >= (1ul << 8)) {
            outptr[0] = 0xda;
            new (outptr + 1) std::uint16_t(str.size());
            std::memcpy(outptr + 3, str.data(), str.size());
            return WriterResult{offset + 3 + str.size(), true};
        } else if (str.size() >= (1ul << 5)) {
            outptr[0] = 0xd9;
            outptr[1] = str.size();
            std::memcpy(outptr + 2, str.data(), str.size());
            return WriterResult{offset + 2 + str.size(), true};
        } else {
            outptr[0] = 0b10100000ul | static_cast<std::uint8_t>(str.size());
            std::memcpy(outptr + 1, str.data(), str.size());
            return WriterResult{offset + 1 + str.size(), true};
        }
    }

    constexpr void operator()(WriterOpFinalizeTag, std::span<std::uint8_t> /* fullspan */, std::size_t /* offset */) const {
        return;
    }

    [[nodiscard]] constexpr std::size_t operator()(WriterOpSizeTag) const {
        const bool more_than_5bit = str.size() >= (1ul << 5);
        const bool more_than_16bit = str.size() >= (1ul << 8);
        const bool more_than_32bit = str.size() >= (1ul << 32);
        return 1ul + more_than_5bit + more_than_16bit + more_than_32bit * 2 + str.size();
    }
};

template <typename WriterT, typename ...WriterTs>
constexpr auto writer_sequence(WriterT&& writer, WriterTs&& ...writers) {
    if constexpr (sizeof...(WriterTs) != 0) {
        return WriterSequence<WriterT, WriterTs...>{std::forward<WriterT>(writer), writer_sequence(std::forward<WriterTs>(writers))...};
    } else {
        return WriterSequence<WriterT>{std::forward<WriterT>(writer)};
    }
}

constexpr auto my_writer = writer_sequence(
    MsgPackString("hello"),
    MsgPackString("world")
);

}  // namespace parsi

#endif  // PARSI_SERIALIZER_HPP
