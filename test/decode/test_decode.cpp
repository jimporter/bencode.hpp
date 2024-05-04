#include <mettle.hpp>
using namespace mettle;

#include "bencode.hpp"
#include "../matchers.hpp"
#include "decode.hpp"

template<typename DecodeArgs>
struct decode_suites {
  decode_suites(DecodeArgs decode_args)
    : decode_args(std::move(decode_args)) {}
  DecodeArgs decode_args;

  template<typename Builder>
  void operator ()(Builder &_) const {
    using InType = fixture_type_t<decltype(_)>;
    using Char = typename char_type<InType>::type;
    auto &decode_args = this->decode_args;

    subsuite<>(_, "decode", [decode_args](auto &_) {
      decode_tests<InType>(_, mix_decoder(decode_args,
                                          [](auto &&data, auto &&...rest) {
        auto value = bencode::decode(data, rest...);
        if constexpr(std::is_base_of_v<std::ios_base, InType>)
          expect(data, at_eof(data));
        return value;
      }));
    });

    if constexpr(!std::is_base_of_v<std::ios_base, InType>) {
      subsuite<>(_, "decode_view", [decode_args](auto &_) {
        decode_tests<InType>(_, mix_decoder(decode_args,
                                            [](auto &&data, auto &&...rest) {
          return bencode::decode_view(data, rest...);
        }));
      });
    }

    subsuite<>(_, "boost_decode", [decode_args](auto &_) {
      decode_tests<InType>(_, mix_decoder(decode_args,
                                          [](auto &&data, auto &&...rest) {
        auto value = bencode::boost_decode(data, rest...);
        if constexpr(std::is_base_of_v<std::ios_base, InType>)
          expect(data, at_eof(data));
        return value;
      }));
    });

    if constexpr(!std::is_base_of_v<std::ios_base, InType>) {
      subsuite<>(_, "boost_decode_view", [decode_args](auto &_) {
        decode_tests<InType>(_, mix_decoder(decode_args,
                                            [](auto &&data, auto &&...rest) {
          return bencode::boost_decode_view(data, rest...);
        }));
      });
    }

    subsuite<
      bencode::data_for_char_t<Char>, bencode::data_view_for_char_t<Char>,
      bencode::boost_data_for_char_t<Char>,
      bencode::boost_data_view_for_char_t<Char>
    >(_, "basic_decode to", type_only, [decode_args](auto &_) {
      using OutType = fixture_type_t<decltype(_)>;
      if constexpr(!(bencode::detail::is_view_v<typename OutType::string> &&
                     std::is_base_of_v<std::ios_base, InType>)) {
        decode_tests<InType>(_, mix_decoder(decode_args,
                                            [](auto &&data, auto &&...rest) {
          auto value = bencode::basic_decode<OutType>(data, rest...);
          if constexpr(std::is_base_of_v<std::ios_base, InType>)
            expect(data, at_eof(data));
          return value;
        }));
      }
    });
  }
};

suite<> test_decode("decode", [](auto &_) {
  subsuite<
    const char *,      std::string, std::vector<char>,      std::istringstream,
    const std::byte *, bstring,     std::vector<std::byte>, bistringstream
  >(_, "decode", type_only, decode_suites(decode_args));

  subsuite<
    std::string, std::vector<char>, bstring, std::vector<std::byte>
  >(_, "decode iterator pair", type_only, decode_suites(decode_iter_args));

  subsuite<
    const char *, const std::byte *
  >(_, "decode pointer/length", type_only, decode_suites(decode_ptr_len_args));
});
