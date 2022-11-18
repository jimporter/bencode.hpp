#include <mettle.hpp>
using namespace mettle;

#include "bencode.hpp"

struct at_eof : matcher_tag {
  bool operator ()(const std::string &) const {
    return true;
  }

  template<typename Char, typename Traits>
  bool operator ()(const std::basic_ios<Char, Traits> &ss) const {
    return ss.eof();
  }

  std::string desc() const {
    return "at eof";
  }
};

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

suite<> test_decode("test decoder", [](auto &_) {

  subsuite<
    bencode::data, bencode::boost_data
  >(_, "decoding", type_only, [](auto &_) {
    using DataType = fixture_type_t<decltype(_)>;
    using boost::get;
    using std::get;

    subsuite<
      const char *, std::string, std::istringstream
    >(_, "decoding from", type_only, [](auto &_) {
      using Fixture = fixture_type_t<decltype(_)>;

      _.test("integer", []() {
        Fixture pos("i42e");
        auto pos_value = bencode::basic_decode<DataType>(pos);
        expect(pos, at_eof());
        expect(get<typename DataType::integer>(pos_value), equal_to(42));

        Fixture neg("i-42e");
        auto neg_value = bencode::basic_decode<DataType>(neg);
        expect(neg, at_eof());
        expect(get<typename DataType::integer>(neg_value), equal_to(-42));
      });

      _.test("string", []() {
        Fixture data("4:spam");
        auto value = bencode::basic_decode<DataType>(data);
        expect(data, at_eof());
        expect(get<typename DataType::string>(value), equal_to("spam"));
      });

      _.test("list", []() {
        Fixture data("li42ee");
        auto value = bencode::basic_decode<DataType>(data);
        auto list = get<typename DataType::list>(value);
        expect(data, at_eof());
        expect(get<typename DataType::integer>(list[0]), equal_to(42));
      });

      _.test("dict", []() {
        Fixture data("d4:spami42ee");
        auto value = bencode::basic_decode<DataType>(data);
        auto dict = get<typename DataType::dict>(value);
        expect(data, at_eof());
        expect(get<typename DataType::integer>(dict["spam"]), equal_to(42));
      });

      _.test("nested", []() {
        Fixture data("d"
          "3:one" "i1e"
          "5:three" "l" "d" "3:bar" "i0e" "3:foo" "i0e" "e" "e"
          "3:two" "l" "i3e" "3:foo" "i4e" "e"
        "e");

        auto value = bencode::basic_decode<DataType>(data);
        expect(data, at_eof());
        auto dict = get<typename DataType::dict>(value);
        expect(get<typename DataType::integer>(dict["one"]), equal_to(1));

        expect(get<typename DataType::string>(
          get<typename DataType::list>(dict["two"])[1]
        ), equal_to("foo"));

        expect(get<typename DataType::integer>(
          get<typename DataType::dict>(
            get<typename DataType::list>(dict["three"])[0]
          )["foo"]
        ), equal_to(0));
      });
    });
  });

  subsuite<>(_, "decode successive objects", [](auto &_) {
    _.test("from string", []() {
      std::string data("i42e4:goat");
      auto begin = data.begin(), end = data.end();

      auto first = bencode::decode(begin, end);
      expect(std::get<bencode::integer>(first), equal_to(42));

      auto second = bencode::decode(begin, end);
      expect(std::get<bencode::string>(second), equal_to("goat"));
    });

    _.test("from stream", []() {
      std::stringstream data("i42e4:goat");

      auto first = bencode::decode(data);
      expect(std::get<bencode::integer>(first), equal_to(42));
      expect(data, is_not(at_eof()));

      auto second = bencode::decode(data);
      expect(std::get<bencode::string>(second), equal_to("goat"));
      expect(data, at_eof());
    });
  });

  subsuite<
    bencode::data_view, bencode::boost_data_view
  >(_, "decoding (view)", type_only, [](auto &_) {
    using DataType = fixture_type_t<decltype(_)>;
    using boost::get;
    using std::get;

    _.test("integer", []() {
      std::string pos("i42e");
      auto pos_value = bencode::basic_decode<DataType>(pos);
      expect(get<typename DataType::integer>(pos_value), equal_to(42));

      std::string neg("i-42e");
      auto neg_value = bencode::basic_decode<DataType>(neg);
      expect(get<typename DataType::integer>(neg_value), equal_to(-42));
    });

    _.test("string", []() {
      std::string data("4:spam");
      auto in_range = all(
        greater_equal(data.data()),
        less_equal(data.data() + data.size())
      );

      auto value = bencode::basic_decode<DataType>(data);
      auto str = get<typename DataType::string>(value);
      expect(&*str.begin(), in_range);
      expect(&*str.end(), in_range);
      expect(str, equal_to("spam"));
    });

    _.test("list", []() {
      std::string data("li42ee");
      auto value = bencode::basic_decode<DataType>(data);
      auto list = get<typename DataType::list>(value);
      expect(get<typename DataType::integer>(list[0]), equal_to(42));
    });

    _.test("dict", []() {
      std::string data("d4:spami42ee");
      auto in_range = all(
        greater_equal(data.data()),
        less_equal(data.data() + data.size())
      );

      auto value = bencode::basic_decode<DataType>(data);
      auto dict = get<typename DataType::dict>(value);
      auto str = dict.find("spam")->first;
      expect(&*str.begin(), in_range);
      expect(&*str.end(), in_range);
      expect(str, equal_to("spam"));

      expect(get<typename DataType::integer>(dict["spam"]), equal_to(42));
    });

    _.test("nested", []() {
      std::string data("d"
        "3:one" "i1e"
        "5:three" "l" "d" "3:bar" "i0e" "3:foo" "i0e" "e" "e"
        "3:two" "l" "i3e" "3:foo" "i4e" "e"
      "e");

      auto value = bencode::basic_decode<DataType>(data);
      auto dict = get<typename DataType::dict>(value);
      expect(get<typename DataType::integer>(dict["one"]), equal_to(1));

      expect(get<typename DataType::string>(
        get<typename DataType::list>(dict["two"])[1]
      ), equal_to("foo"));

      expect(get<typename DataType::integer>(
        get<typename DataType::dict>(
          get<typename DataType::list>(dict["three"])[0]
        )["foo"]
      ), equal_to(0));
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
             decode_error<std::invalid_argument>("integer overflow", 20));
      expect([]() { bencode::decode("i9323372036854775807e"); },
             decode_error<std::invalid_argument>("integer overflow", 20));
      expect([]() { bencode::decode("i92233720368547758070e"); },
             decode_error<std::invalid_argument>("integer overflow", 20));
    });

    _.test("min value", []() {
      auto value = bencode::decode("i-9223372036854775808e");
      expect(std::get<bencode::integer>(value),
             equal_to(-9223372036854775807LL - 1));
    });

    _.test("underflow", []() {
      expect([]() { bencode::decode("i-9223372036854775809e"); },
             decode_error<std::invalid_argument>("integer underflow", 21));
      expect([]() { bencode::decode("i-9323372036854775808e"); },
             decode_error<std::invalid_argument>("integer underflow", 21));
      expect([]() { bencode::decode("i-92233720368547758080e"); },
             decode_error<std::invalid_argument>("integer underflow", 21));
    });

    _.test("max value (unsigned)", []() {
      auto value = bencode::basic_decode<udata>("i18446744073709551615e");
      expect(std::get<udata::integer>(value),
             equal_to(18446744073709551615ULL));
    });

    _.test("overflow (unsigned)", []() {
      expect([]() { bencode::basic_decode<udata>("i18446744073709551616e"); },
             decode_error<std::invalid_argument>("integer overflow", 21));
      expect([]() { bencode::basic_decode<udata>("i19446744073709551615e"); },
             decode_error<std::invalid_argument>("integer overflow", 21));
      expect([]() { bencode::basic_decode<udata>("i184467440737095516150e"); },
             decode_error<std::invalid_argument>("integer overflow", 21));
    });

    _.test("negative value (unsigned)", []() {
      expect(
        []() { bencode::basic_decode<udata>("i-42e"); },
        decode_error<std::invalid_argument>("expected unsigned integer", 1)
      );
    });
  });

  subsuite<>(_, "error handling", [](auto &_) {
    _.test("unexpected type", []() {
      expect([]() { bencode::decode("x"); },
             decode_error<std::invalid_argument>("unexpected type", 0));
    });

    _.test("unexpected end of string", []() {
      auto eos = [](std::size_t offset) {
        return decode_error<std::invalid_argument>("unexpected end of string",
                                                   offset);
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

    _.test("expected 'e'", []() {
      expect([]() { bencode::decode("i123i"); },
             decode_error<std::invalid_argument>("expected 'e'", 4));
    });

    _.test("expected ':'", []() {
      expect([]() { bencode::decode("1abc"); },
             decode_error<std::invalid_argument>("expected ':'", 1));
    });

    _.test("expected string token", []() {
      expect([]() { bencode::decode("di123ee"); },
             decode_error<std::invalid_argument>("expected string token", 1));
    });

    _.test("duplicated key", []() {
      expect(
        []() { bencode::decode("d3:fooi1e3:fooi1ee"); },
        decode_error<std::invalid_argument>("duplicated key in dict: foo", 17)
      );
    });
  });

});
