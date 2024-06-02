#ifndef PARSI_INTERNAL_UTILS_HPP
#define PARSI_INTERNAL_UTILS_HPP

#include <concepts>

template <std::integral T>
constexpr auto byteswap(T value) noexcept -> T
{
    static_assert(std::has_unique_object_representations_v<T>, "T may not have padding bits");
    auto value_representation = std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
    std::ranges::reverse(value_representation);
    return std::bit_cast<T>(value_representation);
}

#endif  // PARSI_INTERNAL_UTILS_HPP
