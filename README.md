# bencode.hpp

[![Latest release][release-image]][release-link]
[![Travis build status][travis-image]][travis-link]
[![Appveyor build status][appveyor-image]][appveyor-link]


**bencode.hpp** is a small, header-only C++ library for parsing and generating
[bencoded][wikipedia] data.

## Requirements

This library requires a C++11 compiler and [Boost][boost] 1.23+. If your
standard library supports `string_view`, (either via C++17 or the [Library
Fundamentals TS][library-fundamentals]), bencode.hpp will use that as well. In
addition, the tests require a C++14 compiler and [mettle][mettle].

## Installation

bencode.hpp uses [bfg9000][bfg9000] to build and test itself. Building with
`bfg9000` is straightforward. Just run the following:

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
determined at runtime, these are all stored in a variant type called `data`.

### Decoding

Decoding bencoded data is simple. Just call `decode`, which will return a `data`
object that you can operate on:

```c++
auto data = bencode::decode("i666e");
auto value = boost::get<bencode::integer>(data);
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
auto value = boost::get<bencode::string_view>(data);
```

### Encoding

Encoding data is also straightforward:

```c++
// Encode and store the result in an std::string.
auto str = bencode::encode(666);

// Encode and output to an std::ostream.
bencode::encode(std::cout, 666);
```

You can also construct more-complex data structures:

```c++
bencode::encode(std::cout, bencode::dict{
  {"one", 1},
  {"two", bencode::list{1, "foo", 2},
  {"three", "3"}
});
```

As with encoding, you can use the `*_view` types if you know the underlying
memory will live until the encoding function returns.

## License

This library is licensed under the [BSD 3-Clause license](LICENSE).

[release-image]: https://img.shields.io/github/release/jimporter/bencode.hpp.svg
[release-link]: https://github.com/jimporter/bencode.hpp/releases/latest
[travis-image]: https://travis-ci.org/jimporter/bencode.hpp.svg?branch=master
[travis-link]: https://travis-ci.org/jimporter/bencode.hpp
[appveyor-image]: https://ci.appveyor.com/api/projects/status/sycb8ugc3vo3g1g9?svg=true
[appveyor-link]: https://ci.appveyor.com/project/jimporter/bencode-hpp/branch/master

[wikipedia]: https://en.wikipedia.org/wiki/Bencode
[boost]: https://www.boost.org/
[library-fundamentals]: http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/n4480.html
[mettle]: https://jimporter.github.io/mettle/
[bfg9000]: https://jimporter.github.io/bfg9000/
