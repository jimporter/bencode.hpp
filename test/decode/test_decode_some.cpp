#include <mettle.hpp>
using namespace mettle;

#include "bencode.hpp"
#include "../matchers.hpp"
#include "decode.hpp"

template<typename InType, typename Builder, typename DecodeArgs,
         typename DoDecode>
auto decode_some_tests(Builder &_, DecodeArgs &&decode_args,
                       DoDecode &&do_decode) {
  using boost::get;
  using std::get;

  _.test("successive objects", [decode_args, do_decode]() {
    auto data = make_data<InType>("i42e4:goat");
    auto args = decode_args(data);
    auto at_end = std::apply([](auto &&...t) {
      return at_eof(std::forward<decltype(t)>(t)...);
    }, args);

    auto first = std::apply(do_decode, args);
    using OutType = decltype(first);
    using Integer = typename OutType::integer;
    using String = typename OutType::string;
    auto S = &make_data<String>;

    expect(get<Integer>(first), equal_to(42));
    expect(std::get<0>(args), is_not(at_end));

    auto second = std::apply(do_decode, args);
    expect(get<String>(second), equal_to(S("goat")));
    expect(std::get<0>(args), at_end);
  });
}

template<typename DecodeArgs>
struct decode_some_suites {
  decode_some_suites(DecodeArgs decode_args)
    : decode_args(std::move(decode_args)) {}
  DecodeArgs decode_args;

  template<typename Builder>
  void operator ()(Builder &_) const {
    using InType = fixture_type_t<decltype(_)>;
    using Char = typename char_type<InType>::type;
    auto &decode_args = this->decode_args;

    subsuite<>(_, "decode_some", [decode_args](auto &_) {
      decode_tests<InType>(
        _, mix_decoder(decode_args, [](auto &&data, auto &&...rest) {
          auto at_end = at_eof(data, rest...);
          auto value = bencode::decode_some(data, rest...);
          expect(data, at_end);
          return value;
        })
      );
      decode_some_tests<InType>(
        _, decode_args, [](auto &&data, auto &&...rest) {
          return bencode::decode_some(data, rest...);
        }
      );
    });

    if constexpr(!std::is_base_of_v<std::ios_base, InType>) {
      subsuite<>(_, "decode_view_some", [decode_args](auto &_) {
        decode_tests<InType>(
          _, mix_decoder(decode_args, [](auto &&data, auto &&...rest) {
            auto at_end = at_eof(data, rest...);
            auto value = bencode::decode_view_some(data, rest...);
            expect(data, at_end);
            return value;
          })
        );
        decode_some_tests<InType>(
          _, decode_args, [](auto &&data, auto &&...rest) {
            return bencode::decode_view_some(data, rest...);
          }
        );
      });
    }

    subsuite<>(_, "boost_decode_some", [decode_args](auto &_) {
      decode_tests<InType>(
        _, mix_decoder(decode_args, [](auto &&data, auto &&...rest) {
          auto at_end = at_eof(data, rest...);
          auto value = bencode::boost_decode_some(data, rest...);
          expect(data, at_end);
          return value;
        })
      );
      decode_some_tests<InType>(
        _, decode_args, [](auto &&data, auto &&...rest) {
          return bencode::boost_decode_some(data, rest...);
        }
      );
    });

    if constexpr(!std::is_base_of_v<std::ios_base, InType>) {
      subsuite<>(_, "boost_decode_view_some", [decode_args](auto &_) {
        decode_tests<InType>(
          _, mix_decoder(decode_args, [](auto &&data, auto &&...rest) {
            auto at_end = at_eof(data, rest...);
            auto value = bencode::boost_decode_view_some(data, rest...);
            expect(data, at_end);
            return value;
          })
        );
        decode_some_tests<InType>(
          _, decode_args, [](auto &&data, auto &&...rest) {
            return bencode::boost_decode_view_some(data, rest...);
          }
        );
      });
    }

    subsuite<
      bencode::data_for_char_t<Char>, bencode::data_view_for_char_t<Char>,
      bencode::boost_data_for_char_t<Char>,
      bencode::boost_data_view_for_char_t<Char>
    >(_, "basic_decode_some to", type_only, [decode_args](auto &_) {
      using OutType = fixture_type_t<decltype(_)>;
      if constexpr(!(bencode::detail::is_view_v<typename OutType::string> &&
                     std::is_base_of_v<std::ios_base, InType>)) {
        decode_tests<InType>(
          _, mix_decoder(decode_args, [](auto &&data, auto &&...rest) {
            auto at_end = at_eof(data, rest...);
            auto value = bencode::basic_decode_some<OutType>(data, rest...);
            expect(data, at_end);
            return value;
          })
        );
        decode_some_tests<InType>(
          _, decode_args, [](auto &&data, auto &&...rest) {
            return bencode::basic_decode_some<OutType>(data, rest...);
          }
        );
      }
    });
  }
};

suite<> test_decode_some("decode_some", [](auto &_) {
  subsuite<
    const char *, std::istringstream, const std::byte *, bistringstream
  >(_, "decode_some", type_only, decode_some_suites(decode_args));

  subsuite<
    std::string, std::vector<char>, bstring, std::vector<std::byte>
  >(_, "decode_some iterator pair", type_only,
    decode_some_suites([](auto &&data) {
      return std::make_tuple(data.begin(), data.end());
    })
  );

  subsuite<
    const char *, const std::byte *
  >(_, "decode_some pointer/length", type_only,
    decode_some_suites(decode_ptr_len_args));
});
