# Changes

## v0.3 (in progress)

- Use `std::variant` by default (thus requiring C++17 or newer)
- Allow customizing the variant type used via `bencode::basic_data`
- Improve performance of `decode`; decoding is now ~2x as fast for most data
  (~1.5x when using views)!

## v0.2.1 (2020-12-03)

- Require `bfg9000` v0.6 for builds/tests

## v0.2 (2019-11-26)

- Use `std::string_view` if available
- Install a `pkg-config` `.pc` file that sets the compiler's include path as
  needed

## v0.1 (2016-07-01)

Initial release
