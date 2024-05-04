#include <mettle.hpp>
using namespace mettle;

#include "bencode.hpp"

template<typename T>
struct char_type;

template<typename Char>
struct char_type<Char *> { using type = std::remove_cv_t<Char>; };
template<typename Char>
struct char_type<std::basic_string<Char>> { using type = Char; };
template<typename Char>
struct char_type<std::basic_string_view<Char>> { using type = Char; };
template<typename Char>
struct char_type<std::vector<Char>> { using type = Char; };
template<typename Char>
struct char_type<std::basic_istringstream<Char>> { using type = Char; };

template<typename T>
T make_data(const char *data) {
  using Char = typename char_type<T>::type;
  if constexpr(std::is_constructible_v<T, const Char *>)
    return T((Char*)(data));
  else
    return T((Char*)data, (Char*)(data + std::strlen(data)));
}

struct at_eof_base : matcher_tag {
  std::string desc() const {
    return "at eof";
  }
};

template<typename ...T>
struct at_eof;

template<typename Char>
struct at_eof<Char *> : at_eof_base {
  at_eof(const Char *) {}
  bool operator ()(const Char *s) const { return *s == Char('\0'); }
};

template<typename Char>
struct at_eof<Char *, std::size_t> : at_eof_base {
  at_eof(const Char *start, std::size_t len) : end(start + len) {}
  bool operator ()(const Char *s) const { return s == end; }
  Char * end;
};

template<typename Iter>
struct at_eof<Iter, Iter> : at_eof_base {
  at_eof(Iter, Iter end) : end(end) {}
  bool operator ()(Iter i) const { return i == end; }
  Iter end;
};

template<typename Char>
struct at_eof<std::basic_istringstream<Char>> : at_eof_base {
  using ios_type = std::basic_istringstream<Char>;
  at_eof(ios_type&) {}
  bool operator ()(const ios_type &ss) const { return ss.eof(); }
};

template<typename ...T>
at_eof(T &&...t) -> at_eof<std::remove_reference_t<std::remove_cv_t<T>>...>;

template<typename T>
auto within_memory(const T &t) {
  if constexpr(bencode::detail::is_iterable_v<T>) {
    return in_interval(&*std::begin(t), &*std::end(t), interval::closed);
  } else {
    return is_not(anything());
  }
}

template<typename T>
auto within_memory(const T *t) {
  return in_interval(t, t + bencode::detail::any_strlen(t), interval::closed);
}

template<typename Nested, typename Matcher>
auto thrown_nested(Matcher &&matcher) {
  auto what = exception_what(std::forward<Matcher>(matcher));
  std::ostringstream ss;
  ss << "nested: " << type_name<Nested>() << "(" << what.desc() << ")";

  return basic_matcher([matcher = std::move(what)](const auto &actual) -> bool {
    try {
      actual.rethrow_nested();
      return false;
    } catch(const Nested &e) {
      return matcher(e);
    } catch(...) {
      return false;
    }
  }, ss.str());
}

template<typename Nested>
auto decode_error(const std::string &what, std::size_t offset) {
  std::string outer_what = what + ", at offset " + std::to_string(offset);
  return thrown_raw<bencode::decode_error>(
    all(exception_what(outer_what), thrown_nested<Nested>(what))
  );
}

template<typename DecodeArgs, typename DoDecode>
auto mix_decoder(DecodeArgs &&decode_args, DoDecode &&do_decode) {
  return [decode_args, do_decode](auto &&data) {
    return std::apply(
      do_decode, decode_args(std::forward<decltype(data)>(data))
    );
  };
}

template<typename InType, typename Builder, typename Callable>
auto decode_tests(Builder &_, Callable &&decode) {
  using boost::get;
  using std::get;

  using OutType = decltype(decode(std::declval<InType>()));
  using Integer = typename OutType::integer;
  using String = typename OutType::string;
  using List = typename OutType::list;
  using Dict = typename OutType::dict;
  auto S = &make_data<String>;

  _.test("integer", [decode]() {
    auto pos = make_data<InType>("i42e");
    auto pos_value = decode(pos);
    expect(get<Integer>(pos_value), equal_to(42));

    auto neg = make_data<InType>("i-42e");
    auto neg_value = decode(neg);
    expect(get<Integer>(neg_value), equal_to(-42));
  });

  _.test("string", [decode, S]() {
    auto data = make_data<InType>("4:spam");
    // Silence GCC < 10.
    [[maybe_unused]] auto within_data_memory = within_memory(data);

    auto value = decode(data);
    auto str = get<String>(value);
    expect(str, equal_to(S("spam")));
    if constexpr(bencode::detail::is_view_v<String>) {
      expect(&*str.begin(), within_data_memory);
      expect(&*str.end(), within_data_memory);
    }
  });

  _.test("list", [decode]() {
    auto data = make_data<InType>("li42ee");
    auto value = decode(data);
    auto list = get<List>(value);
    expect(get<Integer>(list[0]), equal_to(42));
  });

  _.test("dict", [decode, S]() {
    auto data = make_data<InType>("d4:spami42ee");
    // Silence GCC < 10.
    [[maybe_unused]] auto within_data_memory = within_memory(data);

    auto value = decode(data);
    auto dict = get<Dict>(value);
    expect(get<Integer>(dict[S("spam")]), equal_to(42));

    auto str = dict.find(S("spam"))->first;
    expect(str, equal_to(S("spam")));
    if constexpr (bencode::detail::is_view_v<String>) {
      expect(&*str.begin(), within_data_memory);
      expect(&*str.end(), within_data_memory);
    }
  });

  _.test("nested", [decode, S]() {
    auto data = make_data<InType>(
      "d"
      "3:one" "i1e"
      "5:three" "l" "d" "3:bar" "i0e" "3:foo" "i0e" "e" "e"
      "3:two" "l" "i3e" "3:foo" "i4e" "e"
      "e"
    );
    auto value = decode(data);
    auto dict = get<Dict>(value);
    expect(get<Integer>(dict[S("one")]), equal_to(1));
    expect(get<String>(get<List>(dict[S("two")])[1]), equal_to(S("foo")));
    expect(get<Integer>(get<Dict>(get<List>(dict[S("three")])[0])[S("foo")]),
           equal_to(0));
  });
}

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

suite<> test_decode("test decoder", [](auto &_) {
  using bstring = std::basic_string<std::byte>;
  using bistringstream = std::basic_istringstream<std::byte>;

  auto decode_args = [](auto &&data) {
    return std::tuple<decltype(data)&>(data);
  };
  auto decode_iter_args = [](auto &&data) {
    return std::make_tuple(data.begin(), data.end());
  };
  auto decode_ptr_len_args = [](auto &&data) {
    return std::tuple<decltype(data)&, std::size_t>(
      data, bencode::detail::any_strlen(data)
    );
  };

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

  subsuite<
    const char *, std::istringstream, const std::byte *, bistringstream
  >(_, "decode_some", type_only, decode_some_suites(decode_args));

  subsuite<
    std::string, std::vector<char>
  >(_, "decode_some iterator pair", type_only,
    decode_some_suites([](auto &&data) {
      return std::make_tuple(data.begin(), data.end());
    })
  );

  subsuite<
    const char *, const std::byte *
  >(_, "decode_some pointer/length", type_only,
    decode_some_suites(decode_ptr_len_args));

  subsuite<>(_, "decoding integers", [](auto &_) {
    using udata = bencode::basic_data<
      std::variant, unsigned long long, std::string, std::vector,
      bencode::map_proxy
    >;

    _.test("max value", []() {
      auto value = bencode::decode("i9223372036854775807e");
      expect(std::get<bencode::integer>(value),
             equal_to(9223372036854775807LL));
    });

    _.test("overflow", []() {
      expect([]() { bencode::decode("i9223372036854775808e"); },
             decode_error<std::overflow_error>("integer overflow", 20));
      expect([]() { bencode::decode("i9323372036854775807e"); },
             decode_error<std::overflow_error>("integer overflow", 20));
      expect([]() { bencode::decode("i92233720368547758070e"); },
             decode_error<std::overflow_error>("integer overflow", 20));
    });

    _.test("min value", []() {
      auto value = bencode::decode("i-9223372036854775808e");
      expect(std::get<bencode::integer>(value),
             equal_to(-9223372036854775807LL - 1));
    });

    _.test("underflow", []() {
      expect([]() { bencode::decode("i-9223372036854775809e"); },
             decode_error<std::underflow_error>("integer underflow", 21));
      expect([]() { bencode::decode("i-9323372036854775808e"); },
             decode_error<std::underflow_error>("integer underflow", 21));
      expect([]() { bencode::decode("i-92233720368547758080e"); },
             decode_error<std::underflow_error>("integer underflow", 21));
    });

    _.test("max value (unsigned)", []() {
      auto value = bencode::basic_decode<udata>("i18446744073709551615e");
      expect(std::get<udata::integer>(value),
             equal_to(18446744073709551615ULL));
    });

    _.test("overflow (unsigned)", []() {
      expect([]() {
        bencode::basic_decode<udata>("i18446744073709551616e");
      }, decode_error<std::overflow_error>("integer overflow", 21));
      expect([]() {
        bencode::basic_decode<udata>("i19446744073709551615e");
      }, decode_error<std::overflow_error>("integer overflow", 21));
      expect([]() {
        bencode::basic_decode<udata>("i184467440737095516150e");
      }, decode_error<std::overflow_error>("integer overflow", 21));
    });

    _.test("negative value (unsigned)", []() {
      expect(
        []() { bencode::basic_decode<udata>("i-42e"); },
        decode_error<std::underflow_error>("expected unsigned integer", 1)
      );
    });
  });

  subsuite<>(_, "error handling", [](auto &_) {
    _.test("unexpected type token", []() {
      expect([]() { bencode::decode("x"); },
             decode_error<bencode::syntax_error>("unexpected type token", 0));
    });

    _.test("unexpected end of input", []() {
      auto eos = [](std::size_t offset) {
        return decode_error<bencode::end_of_input_error>(
          "unexpected end of input", offset
        );
      };

      expect([]() { bencode::decode(""); }, eos(0));
      expect([]() { bencode::decode("i123"); }, eos(4));
      expect([]() { bencode::decode("3"); }, eos(1));
      expect([]() { bencode::decode("3:as"); }, eos(4));
      expect([]() { bencode::decode("l"); }, eos(1));
      expect([]() { bencode::decode("li1e"); }, eos(4));
      expect([]() { bencode::decode("d"); }, eos(1));
      expect([]() { bencode::decode("d1:a"); }, eos(4));
      expect([]() { bencode::decode("d1:ai1e"); }, eos(7));
    });

    _.test("extraneous character", []() {
      expect([]() { bencode::decode("i123ei"); },
             decode_error<bencode::syntax_error>("extraneous character", 5));
    });

    _.test("expected 'e' token", []() {
      expect([]() { bencode::decode("i123i"); },
             decode_error<bencode::syntax_error>("expected 'e' token", 4));
    });

    _.test("unexpected 'e' token", []() {
      expect([]() { bencode::decode("e"); },
             decode_error<bencode::syntax_error>("unexpected 'e' token", 0));
    });

    _.test("expected ':' token", []() {
      expect([]() { bencode::decode("1abc"); },
             decode_error<bencode::syntax_error>("expected ':' token", 1));
    });

    _.test("expected string start token", []() {
      expect(
        []() { bencode::decode("di123ee"); },
        decode_error<bencode::syntax_error>(
          "expected string start token for dict key", 1
        )
      );
    });

    _.test("duplicated key", []() {
      expect([]() { bencode::decode("d3:fooi1e3:fooi1ee"); },
             decode_error<bencode::syntax_error>(
               "duplicated key in dict: \"foo\"", 17
             ));
    });
  });
});
