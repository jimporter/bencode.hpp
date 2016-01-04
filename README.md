# bencode.hpp

**bencode.hpp** is a small, header-only C++ library for parsing
[bencoded](http://en.wikipedia.org/wiki/Bencode) data.

## Requirements

This library requires a C++11 compiler, plus either
[Boost](http://www.boost.org/) 1.23+ or the [Library Fundamentals
TS](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2014/n4082.pdf) (for
the `any` type). The tests additionally require a C++14 compiler and
[mettle](http://jimporter.github.io/mettle/).

## Installation

bencode.hpp uses [bfg9000](https://github.com/jimporter/bfg9000) to build and
test itself. Building with `bfg9000` is straightforward. Just run the following:

```sh
$ bfg9000 /path/to/mettle build
$ cd build
$ ninja install
```

However, since bencode.hpp is a single-file, header-only library, you can just
copy `include/bencode.hpp` to your destination of choice.

## License

This library is licensed under the BSD 3-Clause license.
