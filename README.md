# bencode.hpp

``bencode.hpp`` is a small, header-only C++ library for parsing
[bencoded](http://en.wikipedia.org/wiki/Bencode) data.

## Requirements

Currently, this code is tested in C++14 (clang 3.4). The tests themselves
*require* C++14 (see [mettle](http://jimporter.github.io/mettle/)), but the
library itself should be happy with C++03. You'll also need Boost (or N4082) for
the `any` type.

## License

This library is licensed under the BSD 3-Clause license.
