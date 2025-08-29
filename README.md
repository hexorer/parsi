# parsi

[![Build:Linux](https://github.com/cthulhu-irl/parsi/actions/workflows/linux.yml/badge.svg)](https://github.com/cthulhu-irl/parsi/actions?query=workflow%3ALinux)
[![Build:Windows](https://github.com/cthulhu-irl/parsi/actions/workflows/windows.yml/badge.svg)](https://github.com/cthulhu-irl/parsi/actions?query=workflow%3AWindows)
[![codecov](https://codecov.io/gh/cthulhu-irl/parsi/branch/build/coverage/graph/badge.svg?token=U2QVK5MRNW)](https://codecov.io/gh/cthulhu-irl/parsi)

parsi is a [parser combinator](https://en.wikipedia.org/wiki/Parser_combinator) library that provides basic parser blocks and a common way to define and combine parsers into more complex parsers.

It can be used as an alternative to complex regex, especially when data needs to be extracted.

The minimum required standard is currently `C++20`.

A simple hex color parser would look like this:
```cpp
struct Color {
    std::uint8_t red = 0;
    std::uint8_t green = 0;
    std::uint8_t blue = 0;
};

constexpr auto color_from_string(std::string_view str) -> std::optional<Color>
{
    constexpr auto convert_hex_digit = [](char digit) -> std::uint8_t {
        if ('0' <= digit && digit <= '9') {
            return digit - '0';
        }

        if ('a' <= digit && digit <= 'f') {
            return 10 + (digit - 'a');
        }

        if ('A' <= digit && digit <= 'F') {
            return 10 + (digit - 'A');
        }

        // unreachable as parser has verified it
        return 0;
    };

    Color color;

    constexpr auto hex_charset = parsi::Charset("0123456789abcdefABCDEF");

    auto parser = parsi::sequence(
        parsi::expect('#'),
        // a color code with 6 hex digits like `#C3A3BB` is equivalent to
        // Color{ .red = 0xC3, .green = 0xA3, .blue = 0xBB }
        parsi::extract(
            parsi::repeat<6, 6>(parsi::expect(hex_charset)),  // min=6 and max=6
            [&](std::string_view str) {
                // str's length is guaranteed to be 6,
                // and characters are guaranteed to be in hex_charset.
                color.red = convert_hex_digit(str[0]) * 16
                          + convert_hex_digit(str[1]);

                color.green = convert_hex_digit(str[2]) * 16
                            + convert_hex_digit(str[3]);

                color.blue = convert_hex_digit(str[4]) * 16
                           + convert_hex_digit(str[5]);
            }
        ),
        parsi::eos()  // end of stream
    );

    if (!parser(str)) {
        return std::nullopt;
    }

    return color;
}
```

### Installation/Dependency

#### CMake System Install

Configure, build, and install from the terminal:
```bash
cmake -S . -B build
cmake --build build
cmake --install build --prefix /usr/local
```

Then just find package and link against `parsi::parsi`:
```cmake
find_package(parsi CONFIG REQUIRED)
target_link_libraries(main PRIVATE parsi::parsi)
```

#### CMake FetchContent

In CMakeLists.txt:
```cmake
include(FetchContent)

FetchContent_Declare(
    parsi
    GIT_REPOSITORY https://github.com/hexorer/parsi
    GIT_TAG main  # or v<version> like v0.1.0
)
FetchContent_MakeAvailable(parsi)

target_link_libraries(your-target PRIVATE parsi::parsi)
```

#### vcpkg

Simply install and link against it.

bash:
```bash
vcpkg install parsi
```

CMakeLists.txt:
```cmake
find_package(parsi CONFIG REQUIRED)
target_link_libraries(your-target PRIVATE parsi::parsi)
```

### Building Examples

Example executables will be built with `example_` prefix into the `bin/` directory:
```
cmake -S . -B build -DPARSI_EXAMPLES=ON
cmake --build build
```

### Roadmap

 - [x] core: base types, parsers, and combinators (Stream, Result, expect, sequence, anyof, etc.)
 - [x] core: runtime friendly api (polymorphism)
 - [ ] core: inline parsing (parse on call instead of creating a parser to be called later)
 - [x] core: optimized parsers for fixed length and constexpr-able strings (e.g. literals)
 - [ ] extra: json
 - [ ] extra: msgpack
 - [ ] extra: toml
