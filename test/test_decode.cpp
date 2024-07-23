#include <mettle.hpp>
using namespace mettle;

#include "bencode.hpp"

template<typename T>
auto make_data(const char *data) ->
std::enable_if_t<std::is_constructible_v<T, const char *>, T> {
  return T((char*)(data));
}

template<typename T>
auto make_data(const char *data) ->
std::enable_if_t<!std::is_constructible_v<T, const char *>, T> {
  return T(data, data + std::strlen(data));
}

struct at_eof : matcher_tag {
  bool operator ()(const char *s) const {
    return *s == '\0';
  }

  template<typename Char, typename Traits>
  bool operator ()(const std::basic_ios<Char, Traits> &ss) const {
    return ss.eof();
  }

  std::string desc() const {
    return "at eof";
  }
};

template<typename T,
         typename std::enable_if_t<bencode::detail::is_iterable_v<T>, int> = 0>
auto within_memory(const T &t) {
  return in_interval(&*std::begin(t), &*std::end(t), interval::closed);
}

template<typename T,
         typename std::enable_if_t<!bencode::detail::is_iterable_v<T>, int> = 0>
auto within_memory(const T &) {
  return is_not(anything());
}

template<typename T>
auto within_memory(const T *t) {
  return in_interval(t, t + std::strlen(t), interval::closed);
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

template<typename InType, typename Builder, typename Callable>
auto decode_tests(Builder &_, Callable &&do_decode) {
  using OutType = fixture_type_t<decltype(_)>;
  using boost::get;
  using std::get;

  _.test("integer", [do_decode]() {
    auto pos = make_data<InType>("i42e");
    auto pos_value = do_decode(pos);
    expect(get<typename OutType::integer>(pos_value), equal_to(42));

    auto neg = make_data<InType>("i-42e");
    auto neg_value = do_decode(neg);
    expect(get<typename OutType::integer>(neg_value), equal_to(-42));
  });

  _.test("string", [do_decode]() {
    auto data = make_data<InType>("4:spam");
    // Silence GCC < 10.
    [[maybe_unused]] auto within_data_memory = within_memory(data);

    auto value = do_decode(data);
    auto str = get<typename OutType::string>(value);
    expect(str, equal_to("spam"));
    if constexpr(bencode::detail::is_view_v<typename OutType::string>) {
      expect(&*str.begin(), within_data_memory);
      expect(&*str.end(), within_data_memory);
    }
  });

  _.test("list", [do_decode]() {
    auto data = make_data<InType>("li42ee");
    auto value = do_decode(data);
    auto list = get<typename OutType::list>(value);
    expect(get<typename OutType::integer>(list[0]), equal_to(42));
  });

  _.test("dict", [do_decode]() {
    auto data = make_data<InType>("d4:spami42ee");
    // Silence GCC < 10.
    [[maybe_unused]] auto within_data_memory = within_memory(data);

    auto value = do_decode(data);
    auto dict = get<typename OutType::dict>(value);
    expect(get<typename OutType::integer>(dict["spam"]), equal_to(42));

    auto str = dict.find("spam")->first;
    expect(str, equal_to("spam"));
    if constexpr(bencode::detail::is_view_v<typename OutType::string>) {
      expect(&*str.begin(), within_data_memory);
      expect(&*str.end(), within_data_memory);
    }
  });

  _.test("nested", [do_decode]() {
    auto data = make_data<InType>(
      "d"
      "3:one" "i1e"
      "5:three" "l" "d" "3:bar" "i0e" "3:foo" "i0e" "e" "e"
      "3:two" "l" "i3e" "3:foo" "i4e" "e"
      "e"
    );
    auto value = do_decode(data);

    auto dict = get<typename OutType::dict>(value);
    expect(get<typename OutType::integer>(dict["one"]), equal_to(1));

    expect(get<typename OutType::string>(
             get<typename OutType::list>(dict["two"])[1]
           ), equal_to("foo"));

    expect(get<typename OutType::integer>(
             get<typename OutType::dict>(
               get<typename OutType::list>(dict["three"])[0]
             )["foo"]
           ), equal_to(0));
  });
}

suite<> test_decode("test decoder", [](auto &_) {
  subsuite<
    const char *, std::string, std::vector<char>, std::istringstream
  >(_, "decode", type_only, [](auto &_) {
    using InType = fixture_type_t<decltype(_)>;

    subsuite<
      bencode::data, bencode::boost_data
    >(_, "decode to", type_only, [](auto &_) {
      using OutType = fixture_type_t<decltype(_)>;
      decode_tests<InType>(_, [](auto &&data) {
        auto value = bencode::basic_decode<OutType>(data);
        if constexpr(std::is_same_v<InType, std::istringstream>) {
          expect(data, at_eof());
        }
        return value;
      });
    });

    if constexpr(!std::is_same_v<InType, std::istringstream>) {
      subsuite<
        bencode::data_view, bencode::boost_data_view
      >(_, "decode to", type_only, [](auto &_) {
        using OutType = fixture_type_t<decltype(_)>;
        decode_tests<InType>(_, [](auto &&data) {
          return bencode::basic_decode<OutType>(data);
        });
      });
    }
  });

  subsuite<
    std::string, std::vector<char>
  >(_, "decode iterator pair", type_only, [](auto &_) {
    using InType = fixture_type_t<decltype(_)>;

    subsuite<
      bencode::data, bencode::boost_data, bencode::data_view,
      bencode::boost_data_view
    >(_, "decode to", type_only, [](auto &_) {
      using OutType = fixture_type_t<decltype(_)>;
      decode_tests<InType>(_, [](auto &&data) {
        return bencode::basic_decode<OutType>(data.begin(), data.end());
      });
    });
  });

  subsuite<>(_, "decode pointer/length", [](auto &_) {
    subsuite<
      bencode::data, bencode::boost_data, bencode::data_view,
      bencode::boost_data_view
    >(_, "decode to", type_only, [](auto &_) {
      using OutType = fixture_type_t<decltype(_)>;
      decode_tests<const char *>(_, [](const char *data) {
        return bencode::basic_decode<OutType>(data, data + std::strlen(data));
      });
    });
  });

  subsuite<
    const char *, std::istringstream
  >(_, "decode_some", type_only, [](auto &_) {
    using InType = fixture_type_t<decltype(_)>;

    subsuite<
      bencode::data, bencode::boost_data
    >(_, "decode to", type_only, [](auto &_) {
      using OutType = fixture_type_t<decltype(_)>;
      decode_tests<InType>(_, [](auto &&data) {
        auto value = bencode::basic_decode_some<OutType>(data);
        expect(data, at_eof());
        return value;
      });

      _.test("successive objects", []() {
        auto data = make_data<InType>("i42e4:goat");

        auto first = bencode::decode_some(data);
        expect(std::get<bencode::integer>(first), equal_to(42));
        expect(data, is_not(at_eof()));

        auto second = bencode::decode_some(data);
        expect(std::get<bencode::string>(second), equal_to("goat"));
        expect(data, at_eof());
      });
    });

    if constexpr(!std::is_same_v<InType, std::istringstream>) {
      subsuite<
        bencode::data_view, bencode::boost_data_view
      >(_, "decode to", type_only, [](auto &_) {
        using OutType = fixture_type_t<decltype(_)>;
        decode_tests<InType>(_, [](auto &&data) {
          auto value = bencode::basic_decode_some<OutType>(data);
          expect(data, at_eof());
          return value;
        });
      });
    }
  });

  subsuite<
    std::string, std::vector<char>
  >(_, "decode_some iterator pair", type_only, [](auto &_) {
    using InType = fixture_type_t<decltype(_)>;

    subsuite<
      bencode::data, bencode::boost_data, bencode::data_view,
      bencode::boost_data_view
    >(_, "decode to", type_only, [](auto &_) {
      using OutType = fixture_type_t<decltype(_)>;
      decode_tests<InType>(_, [](auto &&data) {
        auto begin = data.begin(), end = data.end();
        auto value = bencode::basic_decode_some<OutType>(begin, end);
        expect(begin, equal_to(end));
        return value;
      });

      _.test("successive objects", []() {
        auto data = make_data<InType>("i42e4:goat");
        auto begin = data.begin(), end = data.end();

        auto first = bencode::decode_some(begin, end);
        expect(std::get<bencode::integer>(first), equal_to(42));
        expect(begin, less(end));

        auto second = bencode::decode_some(begin, end);
        expect(std::get<bencode::string>(second), equal_to("goat"));
        expect(begin, equal_to(end));
      });
    });
  });

  subsuite<>(_, "decode_some pointer/length", [](auto &_) {
    subsuite<
      bencode::data, bencode::boost_data, bencode::data_view,
      bencode::boost_data_view
    >(_, "decode to", type_only, [](auto &_) {
      using OutType = fixture_type_t<decltype(_)>;
      decode_tests<const char *>(_, [](const char *data) {
        auto value = bencode::basic_decode_some<OutType>(
          data, data + std::strlen(data)
        );
        expect(data, at_eof());
        return value;
      });
    });
  });

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
      expect(
        []() { bencode::decode("d3:fooi1e3:fooi1ee"); },
        decode_error<bencode::syntax_error>("duplicated key in dict: foo", 17)
      );
    });
  });

});
