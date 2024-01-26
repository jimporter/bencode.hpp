# bencode.hpp

[![Latest release][release-image]][release-link]
[![Build status][ci-image]][ci-link]


**bencode.hpp** is a small, single-header C++ library for parsing and generating
[bencoded][wikipedia] data. You might find it useful as an (extremely!) simple
library for serializing data from your program.

## Requirements

This library has no external dependencies and only requires a C++17 compiler.
It's been tested on [Clang][clang] 7+, [GCC][gcc] 7+, and [MSVC][msvc] 2017+.
The unit tests do depend on [mettle][mettle], however.

**Note:** if Boost is installed, bencode.hpp will provide the ability to use
[`boost::variant`](#boostvariant), which can perform significantly better than
`std::variant` on some data sets (up to 2x faster than libstdc++ or libc++ when
decoding integers).

## Installation

If you're using Ubuntu (or a similar distro), you can install bencode from the
following PPA: [ppa:jimporter/stable][ppa]. If you're not using Ubuntu, you can
also build from source using [bfg9000][bfg9000]. Just run the following:

```sh
$ cd /path/to/bencode.hpp/
$ 9k build/
$ cd build/
$ ninja install
```

*However*, since bencode.hpp is a single-file, header-only library, you can just
copy `include/bencode.hpp` to your destination of choice. (Note that doing this
won't generate a `bencodehpp.pc` file for `pkg-config` to use.)

## Usage

### Data types

Bencode has four data types: `integer`, `string`, `list`, and `dict`. These
correspond to `long long`, `std::string`, `std::vector<bencode::data>`, and
`std::map<std::string, bencode::data>`, respectively. Since the data types are
determined at runtime, these are all stored in a variant type called `data` (a
subclass of `std::variant`).

Note: Technically, `bencode::dict` is a `map_proxy` object, since `std::map`
doesn't support holding elements of incomplete type (though some implementations
do allow this). This type has all the member functions you'd expect, as well as
overloaded `*` and `->` operators to access the proxied `std::map` directly.
However, you can [customize this](#bringing-your-own-variant) if you like.

### Decoding

Decoding bencoded data is simple. Just call `decode`, which will return a `data`
object that you can operate on:

```c++
auto data = bencode::decode("i42e");
auto value = std::get<bencode::integer>(data);
```

`decode` also has an overload that takes an iterator pair:

```c++
auto data = bencode::decode(foo.begin(), foo.end());
```

Finally, you can pass an `std::istream` directly to `decode`. By default, this
overload will set the eof bit on the stream if it reaches the end. However, you
can override this behavior. This is useful if, for instance, you're reading
multiple bencoded messages from a pipe:

```c++
// Defaults to bencode::check_eof.
auto data = bencode::decode(stream, bencode::no_check_eof);
```

#### Views

If the buffer holding the bencoded data is stable (i.e. won't change or be
destroyed until you're done working with the parsed representation), you can
decode the data as a *view* on the buffer to save memory. This results in all
parsed strings being nothing more than pointers pointing to slices of your
buffer. Simply append `_view` to the functions/types to take advantage of this:

```c++
std::string buf = "3:foo";
auto data = bencode::decode_view(buf);
auto value = std::get<bencode::string_view>(data);
```

#### Errors

If there's an error trying to decode some bencode data, a `decode_error` will be
thrown. This provides information about where the error occurred via the
`offset()` member function, as well as access to the underlying exception that
caused the error, via either `nested_ptr()` or `rethrow_nested()`:

```c++
  try {
    auto data = bencode::decode(input);
  } catch(const bencode::decode_error &e) {
    // Throw the underlying exception. Maybe catch it and do something with it.
    e.rethrow_nested();
  }
```

### Reading Data

Once you have a `data` (or `data_view`) object, it's easy to read from it. For
simple cases, you can just use `std::get` to retrieve the value out of the
variant:

```c++
auto data = bencode::decode("i42e");
auto value = std::get<bencode::integer>(data);
```

In addition, you can use the `operator []` or `at` member functions to get the
requested element from a `list` value (if you pass an integer) or `dict` value
(if you pass a string):

```c++
auto data = bencode::decode("d3:fooi42ee");
auto elem = data["foo"];
auto value = std::get<bencode::integer>(elem);
```

These member functions simply forward on to the corresponding functions for the
underlying container, and are (roughly) equivalent to:

```c++
auto elem = std::get<bencode::dict>(data)["foo"];
```

#### Visiting

Since `bencode::data` type is simply a subclass of `std::variant` (likewise
`bencode::data_view`), you can usually just call `std::visit` on it.
Unfortunately, due to a [quirk][inheriting-variant] in the C++ specification
(resolved in C++23), not all standard libraries support passing `bencode::data`
to `std::visit`. To get around this issue, you can call the `base()` method to
cast `bencode::data` to a `std::variant`:

```c++
std::visit(visitor_fn, my_data.base());
```

### Encoding

Encoding data is also straightforward:

```c++
// Encode and store the result in an std::string.
auto str = bencode::encode(42);

// Encode and output to an std::ostream.
bencode::encode(std::cout, 42);
```

You can also construct more-complex data structures:

```c++
bencode::encode(std::cout, bencode::dict{
  {"one", 1},
  {"two", bencode::list{1, "foo", 2}},
  {"three", "3"}
});
```

As with encoding, you can use the `*_view` types if you know the underlying
memory will live until the encoding function returns.

### `boost::variant`

If Boost is installed, bencode.hpp will provide functions to decode data into a
`boost::variant`. This can be particularly useful for some data sets, since
`boost::variant` is consistently faster than most `std::variant`
implementations, especially when storing integers.

These functions work the same as the regular bencode.hpp versions, but are
prefixed with `boost_`:

```c++
bencode::boost_data d = bencode::boost_decode(msg);
bencode::boost_data_view dv = bencode::boost_decode_view(msg);
// ...
```

### Bringing Your Own Variant

In addition to using the built-in data types `bencode::data` and
`bencode::data_view`, you can define your own with the `bencode::basic_data`
class template. This can be useful if you want different alternative types in
your variant (e.g. using `std::map` instead of `bencode::map_proxy` if your
standard library supports that) or to use a different variant type altogether:

```c++
using cool_data = bencode::basic_data<
  cool_variant, long long, std::string, std::vector, bencode::map_proxy
>;

auto result = bencode::basic_decode<cool_data>(message);
```

Note that when using a different variant type, you'll likely want to create a
specialization of `bencode::variant_traits` so that bencode.hpp knows how to
call the visitor function for your type:

```c++
template<>
struct bencode::variant_traits<cool_variant> {
  template<typename Visitor, typename ...Variants>
  static decltype(auto) visit(Visitor &&visitor, Variants &&...variants) {
    return cool_visit(std::forward<Visitor>(visitor),
                      std::forward<Variants>(variants).base()...);
  }

  template<typename Type, typename Variant>
  inline static decltype(auto) get_if(Variant *variant) {
    return cool_get_if<Type>(&variant->base());
  }

  template<typename Variant>
  inline static auto index(const Variant &variant) {
    return variant.cool_index();
  }
};
```

## License

This library is licensed under the [BSD 3-Clause license](LICENSE).

[release-image]: https://img.shields.io/github/release/jimporter/bencode.hpp.svg
[release-link]: https://github.com/jimporter/bencode.hpp/releases/latest
[ci-image]: https://github.com/jimporter/bencode.hpp/actions/workflows/build.yml/badge.svg
[ci-link]: https://github.com/jimporter/bencode.hpp/actions/workflows/build.yml?query=branch%3Amaster
[wikipedia]: https://en.wikipedia.org/wiki/Bencode
[clang]: http://clang.llvm.org/
[gcc]: https://gcc.gnu.org/
[msvc]: https://www.visualstudio.com/
[library-fundamentals]: https://wg21.link/n4480
[mettle]: https://jimporter.github.io/mettle/
[ppa]: https://launchpad.net/~jimporter/+archive/ubuntu/stable
[bfg9000]: https://jimporter.github.io/bfg9000/
[inheriting-variant]: https://wg21.link/p2162
