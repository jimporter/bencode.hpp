# Changes

## v2.0.0 (in progress)

### New features
- `bencode::data` (and `bencode::basic_data`, etc) now support `operator []` and
  `at` member functions to get list/dictionary elements
- Add `bencode::decode_all` and friends to throw an exception if there's any
  data available after the bencoded object

### Breaking changes
- To decode strings, you must now call `bencode::decode_all` (or pass an
  iterator pair to `bencode::decode`)

---

## v1.0.1 (2023-07-01)

### Bug fixes
- Fix deprecation warning when configuring

---

## v1.0.0 (2023-05-21)

### New features
- Use `std::variant` by default (thus requiring C++17 or newer)
- Allow customizing the variant type used via `bencode::basic_data`
- Improve performance of `decode`; decoding is now ~2x as fast for most data
  (~1.5x when using views)!
- When unable to decode data, throw `bencode::decode_error` with the offset
  where the error occurred

### Bug fixes
- Parse bencoded data iteratively to prevent stack buffer overflows
- Throw exceptions for integer over/underflow

---

## v0.2.1 (2020-12-03)

### Bug fixes
- Require `bfg9000` v0.6 for builds/tests

---

## v0.2 (2019-11-26)

### New features
- Use `std::string_view` if available
- Install a `pkg-config` `.pc` file that sets the compiler's include path as
  needed

---

## v0.1 (2016-07-01)

Initial release
