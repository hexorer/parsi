#include <catch2/catch_all.hpp>

#include <concepts>
#include <type_traits>

#include "parsi/parsi.hpp"

namespace data_cases {

//-------- writer

/**
 * What is expected from a writer?
 *  - write byte or bytes
 *  - reserve/hint for upcoming write size to destination write driver ?
 *  - writable size at the moment (used to check success of the write) ?
 */

namespace internal {

template <typename StateT, auto CheckFV = [](const StateT&) { return true; }>
struct GenericState {
    StateT state;

    constexpr operator const StateT&() const noexcept { return state; }
    constexpr operator StateT&&() const noexcept { return std::move(state); }

    constexpr operator bool() const noexcept { return CheckFV(state); }
};

template <auto FV>
struct CF {
    template <typename ...Args>
    constexpr auto operator()(Args&& ...args) noexcept
    {
        return FV(std::forward<Args>(args)...);
    }

    template <typename ...Args>
    constexpr auto operator()(Args&& ...args) const noexcept
    {
        return FV(std::forward<Args>(args)...);
    }
};

template <typename T>
struct CP {
    T _value;
    consteval CP(T value) noexcept : _value(std::move(value)) {}
    consteval auto value() const noexcept { return CF<value>(); }
};

}  // namespace internal

template <typename T>
constexpr auto writer_of()
{
    //
}

template <typename StateT>
constexpr auto state_of(StateT state)
{
    if constexpr (!std::convertible_to<StateT, bool>) {
        return internal::GenericState<StateT>{std::move(state)};
    } else {
        return state;
    }
}

template <auto CheckFV, typename StateT>
constexpr auto state_of(StateT state)
{
    return internal::GenericState<StateT, CheckFV>{std::move(state)};
}

//-------- //writer

template <std::size_t Size>
using bytearray = std::array<std::uint8_t, Size>;

template <std::integral T>
constexpr auto nth_byte(T value, std::size_t index) noexcept -> std::uint8_t
{
    return (value & (0xff << (index * 8))) >> (index * 8);
}

template <std::floating_point T>
constexpr auto nth_byte(T value, std::size_t index) noexcept -> std::uint8_t
{
    if (index >= sizeof(T)) [[unlikely]] {
        return 0;
    }

    const auto bytes = std::bit_cast<std::array<std::uint8_t, sizeof(T)>>(value);
    return bytes[(std::endian::native == std::endian::little) ? (sizeof(T) - index) : index];
}

constexpr auto writer(std::convertible_to<std::uint8_t> auto ...bytes)
{
    return [bytes...]<std::output_iterator<std::uint8_t> Iterator>(Iterator iterator) {
        auto write = [&iterator](std::uint8_t byte) {
            *iterator = byte;
            ++iterator;
        };

        (write(bytes), ...);

        return iterator;
    };
}

constexpr auto writer(std::ranges::input_range auto range)
{
    return [range]<std::output_iterator<std::uint8_t> Iterator>(Iterator iterator) {
        for (const auto& item : range) {
            *iterator = item;
            ++iterator;
        }
        return iterator;
    };
}

template <typename ...Fs>
constexpr auto combine_writers(Fs&& ...writers)
{
    static_assert(sizeof...(Fs) > 0);

    return [writers...]<std::output_iterator<std::uint8_t> Iterator>(Iterator iterator) {
        (writers(iterator++), ...);  // TODO double-check
    };
}

constexpr auto calc_size(std::nullptr_t) -> std::size_t { return 1; }
constexpr auto calc_size(bool value) -> std::size_t { return 1; }
constexpr auto calc_size(std::int8_t value) -> std::size_t { return 1+1; }
constexpr auto calc_size(std::int16_t value) -> std::size_t { return 1+2; }
constexpr auto calc_size(std::int32_t value) -> std::size_t { return 1+4; }
constexpr auto calc_size(std::int64_t value) -> std::size_t { return 1+8; }
constexpr auto calc_size(std::uint8_t value) -> std::size_t { return 1+1; }
constexpr auto calc_size(std::uint16_t value) -> std::size_t { return 1+2; }
constexpr auto calc_size(std::uint32_t value) -> std::size_t { return 1+4; }
constexpr auto calc_size(std::uint64_t value) -> std::size_t { return 1+8; }
constexpr auto calc_size(float value) -> std::size_t { return 1+4; }
constexpr auto calc_size(double value) -> std::size_t { return 1+8; }
constexpr auto calc_size(std::string_view str) -> std::size_t
{
    if (str.size() <= std::numeric_limits<std::uint8_t>::max()) {
        return 1+1+str.size();
    } else if (str.size() <= std::numeric_limits<std::uint16_t>::max()) {
        return 1+2+str.size();
    } else if (str.size() <= std::numeric_limits<std::uint32_t>::max()) {
        return 1+4+str.size();
    }

    return 1+4+(str.size() & 0xffffffff);  // trim to fit
}
constexpr auto calc_size(std::span<std::byte> bytes) -> std::size_t
{
    if (bytes.size() <= std::numeric_limits<std::uint8_t>::max()) {
        return 1+1+bytes.size();
    } else if (bytes.size() <= std::numeric_limits<std::uint16_t>::max()) {
        return 1+2+bytes.size();
    } else if (bytes.size() <= std::numeric_limits<std::uint32_t>::max()) {
        return 1+4+bytes.size();
    }

    return 1+4+(bytes.size() & 0xffffffff);  // trim to fit
}

constexpr auto make(std::same_as<std::nullopt_t> auto _)
{
    constexpr auto null_byte = 0xc0;
    return writer(null_byte);
}

constexpr auto make(std::same_as<bool> auto value)
{
    constexpr auto true_byte = 0xc2;
    constexpr auto false_byte = 0xc3;
    return writer(value ? true_byte : false_byte);
}

constexpr auto make(std::same_as<std::int8_t> auto value) noexcept
{
    constexpr auto type_byte = 0xc4;
    return writer(
        type_byte,
        nth_byte(value, 0)
    );
}
constexpr auto make(std::same_as<std::int16_t> auto value) noexcept
{
    constexpr auto type_byte = 0xc5;
    return writer(
        type_byte,
        nth_byte(value, 0),
        nth_byte(value, 1)
    );
}
constexpr auto make(std::same_as<std::int32_t> auto value) noexcept
{
    constexpr auto type_byte = 0xc6;
    return writer(
        type_byte,
        nth_byte(value, 0),
        nth_byte(value, 1),
        nth_byte(value, 2),
        nth_byte(value, 3)
    );
}
constexpr auto make(std::same_as<std::int64_t> auto value) noexcept
{
    constexpr auto type_byte = 0xc7;
    return writer(
        type_byte,
        nth_byte(value, 0),
        nth_byte(value, 1),
        nth_byte(value, 2),
        nth_byte(value, 3),
        nth_byte(value, 4),
        nth_byte(value, 5),
        nth_byte(value, 6),
        nth_byte(value, 7)
    );
}
constexpr auto make(std::same_as<std::uint8_t> auto value) noexcept
{
    constexpr auto type_byte = 0xc4;
    return writer(
        type_byte,
        nth_byte(value, 0)
    );
}
constexpr auto make(std::same_as<std::uint16_t> auto value) noexcept
{
    constexpr auto type_byte = 0xc5;
    return writer(
        type_byte,
        nth_byte(value, 0),
        nth_byte(value, 1)
    );
}
constexpr auto make(std::same_as<std::uint32_t> auto value) noexcept
{
    constexpr auto type_byte = 0xc6;
    return writer(
        type_byte,
        nth_byte(value, 0),
        nth_byte(value, 1),
        nth_byte(value, 2),
        nth_byte(value, 3)
    );
}
constexpr auto make(std::same_as<std::uint64_t> auto value) noexcept
{
    constexpr auto type_byte = 0xc7;
    return writer(
        type_byte,
        nth_byte(value, 0),
        nth_byte(value, 1),
        nth_byte(value, 2),
        nth_byte(value, 3),
        nth_byte(value, 4),
        nth_byte(value, 5),
        nth_byte(value, 6),
        nth_byte(value, 7)
    );
}

constexpr auto make(std::same_as<float> auto value)
{
    constexpr auto type_byte = 0xca;
    return writer(
        type_byte,
        nth_byte(value, 0),
        nth_byte(value, 1),
        nth_byte(value, 2),
        nth_byte(value, 3)
    );
}

constexpr auto make(std::same_as<double> auto value)
{
    constexpr auto type_byte = 0xcb;
    return writer(
        type_byte,
        nth_byte(value, 0),
        nth_byte(value, 1),
        nth_byte(value, 2),
        nth_byte(value, 3),
        nth_byte(value, 4),
        nth_byte(value, 5),
        nth_byte(value, 6),
        nth_byte(value, 7)
    );
}

constexpr auto make(std::string_view str)
{
    constexpr auto str8_type_byte = 0xd9;
    constexpr auto str16_type_byte = 0xda;
    constexpr auto str32_type_byte = 0xdb;

    const auto type_byte = [&str] {
        if (str.size() < std::numeric_limits<std::uint8_t>::max()) {
            return str8_type_byte;
        } else if (str.size() < std::numeric_limits<std::uint16_t>::max()) {
            return str16_type_byte;
        } else if (str.size() < std::numeric_limits<std::uint32_t>::max()) {
            return str32_type_byte;
        }
        return str32_type_byte;
    }();

    const auto size_writer = [type_byte, size=str.size()](auto iterator) {
        switch (type_byte) {
            case str8_type_byte:
                return writer(nth_byte(size, 0))(std::move(iterator));
            case str16_type_byte:
                return writer(
                    nth_byte(size, 0),
                    nth_byte(size, 1)
                )(std::move(iterator));
            case str32_type_byte:
                return writer(
                    nth_byte(size, 0),
                    nth_byte(size, 1),
                    nth_byte(size, 2),
                    nth_byte(size, 3)
                )(std::move(iterator));
            default:
                std::abort();
        };
    };

    return combine_writers(
        writer(type_byte),
        std::move(size_writer),
        writer(str)
    );
}

constexpr auto make(std::span<const std::uint8_t> bytes)
{
    constexpr auto bin8_type_byte = 0xc4;
    constexpr auto bin16_type_byte = 0xc5;
    constexpr auto bin32_type_byte = 0xc6;

    const auto type_byte = [&bytes] {
        if (bytes.size() < std::numeric_limits<std::uint8_t>::max()) {
            return bin8_type_byte;
        } else if (bytes.size() < std::numeric_limits<std::uint16_t>::max()) {
            return bin16_type_byte;
        } else if (bytes.size() < std::numeric_limits<std::uint32_t>::max()) {
            return bin32_type_byte;
        }
        return bin32_type_byte;
    }();

    // truncate the size to 32 bits unsigned integer if larger.
    bytes = bytes.subspan(0, bytes.size() & 0xffffffff);
    
    const auto size_writer = [type_byte, size=bytes.size()](auto iterator) {
        switch (type_byte) {
            case bin8_type_byte:
                return writer(nth_byte(size, 0))(std::move(iterator));
            case bin16_type_byte:
                return writer(
                    nth_byte(size, 0),
                    nth_byte(size, 1)
                )(std::move(iterator));
            case bin32_type_byte:
                return writer(
                    nth_byte(size, 0),
                    nth_byte(size, 1),
                    nth_byte(size, 2),
                    nth_byte(size, 3)
                )(std::move(iterator));
            default:
                std::abort();
        };
    };

    return combine_writers(
        writer(type_byte),
        size_writer,
        writer(bytes)
    );
}

template <typename F>
constexpr auto make_array(std::size_t size, F&& generator)
{
    constexpr auto bin8_type_byte = 0xc4;
    constexpr auto bin16_type_byte = 0xc5;
    constexpr auto bin32_type_byte = 0xc6;

    const auto type_byte = [size] {
        if (size < std::numeric_limits<std::uint8_t>::max()) {
            return bin8_type_byte;
        } else if (size < std::numeric_limits<std::uint16_t>::max()) {
            return bin16_type_byte;
        } else if (size < std::numeric_limits<std::uint32_t>::max()) {
            return bin32_type_byte;
        }
        return bin32_type_byte;
    }();

    const auto size_writer = [type_byte, size](auto iterator) {
        switch (type_byte) {
            case arrfixed_type_byte:
                return writer(0b1001 | (nth_byte(size, 0))(std::move(iterator) & 0xf));
            case arr16_type_byte:
                return writer(
                    nth_byte(size, 0),
                    nth_byte(size, 1)
                )(std::move(iterator));
            case arr32_type_byte:
                return writer(
                    nth_byte(size, 0),
                    nth_byte(size, 1),
                    nth_byte(size, 2),
                    nth_byte(size, 3)
                )(std::move(iterator));
            default:
                std::abort();
        };
    };

    return [=, gen=std::forward<F>(generator)](auto iterator) {
        size_writer(iterator);
        for (const auto index : std::ranges::iota(size)) {
            gen(index, iterator);
        }
    };
}

template <std::ranges::sized_range RangeT>
constexpr auto make_array(const RangeT& range)
{
    return make_array(std::size(range), [iter=std::begin(range), end=std::end(range)](auto idx, auto iterator) mutable {
        if (iter == end) {
            return false;
        }
        *iterator = *iter;
        ++iter;
        return true;
    });
}

template <typename T, typename ...Ts>
constexpr auto make_tuple(T first, Ts ...rest)
{
    //
}

template <typename Iterator, typename Sentinel>
constexpr auto make_map(Iterator iterator, Sentinel sentinel)
{
    return [=](auto output_iterator) {
        for (auto&& [key, writer] : std::range(iterator, sentinel)) {
            // write key: ...
            // invoke writer: writer(output_iterator);
        }
    }
}

template <typename T, typename ...Ts>
constexpr auto make_serializer(keyvalue<T> first, keyvalue<Ts>... rest)
{
    //
}

}  // namespace cases

struct Date {
    unsigned short year;
    unsigned short month;
    unsigned short day;

    constexpr auto writer = string_writer();

    constexpr auto output = initiate()
        | begin_document()
        | field("year", year)
        | field("month", month)
        | field("day", day)
        | end_document()
        | finalize();

    constexpr auto writer = string_writer();

    constexpr auto output = document(
        writer,
        field(writer, "year", )
    );
};

struct Person {
    std::string name;
    std::size_t age;
    Date birth;
    std::unique_ptr<Person[]> children;

    constexpr auto output = string_writer()
        | begin_document()
        | end_document()
        | finalize();

    constexpr auto serializer = make_serializer<Person>(
        {"name", &Person::name},
        {"age", &Person::age},
        {"birth", &Person::birth},
        // {"children", &Person::children}
        {"children", [](const Person& person) { return make_array(person.children); }}
    );
};

TEST_CASE("msgpack-sample")
{
    std::array<int, 12> arr{};
    auto writer = make_map(fields(
        {"x", make(12)},
        {"y", make(13.0)},
        {"str", make("string")},
        {"arr", make_array(arr)},
        {"map", make_map(/* what to use as <source> api? */)}
    ));
    CHECK(true);
}
