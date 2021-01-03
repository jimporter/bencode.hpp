#include <mettle.hpp>
using namespace mettle;

#include "bencode.hpp"
#include "boost_variant.hpp"

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

suite<> test_decode("test decoder", [](auto &_) {

  subsuite<bencode::data, bdata>(_, "decoding", type_only, [](auto &_) {
    using DataType = fixture_type_t<decltype(_)>;
    using boost::get;
    using std::get;

    subsuite<
      const char *, std::string, std::istringstream
    >(_, "decoding from", type_only, [](auto &_) {
      using Fixture = fixture_type_t<decltype(_)>;

      _.test("integer", []() {
        Fixture pos("i666e");
        auto pos_value = bencode::basic_decode<DataType>(pos);
        expect(pos, at_eof());
        expect(get<typename DataType::integer>(pos_value), equal_to(666));

        Fixture neg("i-666e");
        auto neg_value = bencode::basic_decode<DataType>(neg);
        expect(neg, at_eof());
        expect(get<typename DataType::integer>(neg_value), equal_to(-666));
      });

      _.test("string", []() {
        Fixture data("4:spam");
        auto value = bencode::basic_decode<DataType>(data);
        expect(data, at_eof());
        expect(get<typename DataType::string>(value), equal_to("spam"));
      });

      _.test("list", []() {
        Fixture data("li666ee");
        auto value = bencode::basic_decode<DataType>(data);
        auto list = get<typename DataType::list>(value);
        expect(data, at_eof());
        expect(get<typename DataType::integer>(list[0]), equal_to(666));
      });

      _.test("dict", []() {
        Fixture data("d4:spami666ee");
        auto value = bencode::basic_decode<DataType>(data);
        auto dict = get<typename DataType::dict>(value);
        expect(data, at_eof());
        expect(get<typename DataType::integer>(dict["spam"]), equal_to(666));
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
      std::string data("i666e4:goat");
      auto begin = data.begin(), end = data.end();

      auto first = bencode::decode(begin, end);
      expect(std::get<bencode::integer>(first), equal_to(666));

      auto second = bencode::decode(begin, end);
      expect(std::get<bencode::string>(second), equal_to("goat"));
    });

    _.test("from stream", []() {
      std::stringstream data("i666e4:goat");

      auto first = bencode::decode(data);
      expect(std::get<bencode::integer>(first), equal_to(666));
      expect(data, is_not(at_eof()));

      auto second = bencode::decode(data);
      expect(std::get<bencode::string>(second), equal_to("goat"));
      expect(data, at_eof());
    });
  });

  subsuite<
    bencode::data_view, bdata_view
  >(_, "decoding (view)", type_only, [](auto &_) {
    using DataType = fixture_type_t<decltype(_)>;
    using boost::get;
    using std::get;

    _.test("integer", []() {
      std::string pos("i666e");
      auto pos_value = bencode::basic_decode<DataType>(pos);
      expect(get<typename DataType::integer>(pos_value), equal_to(666));

      std::string neg("i-666e");
      auto neg_value = bencode::basic_decode<DataType>(neg);
      expect(get<typename DataType::integer>(neg_value), equal_to(-666));
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
      std::string data("li666ee");
      auto value = bencode::basic_decode<DataType>(data);
      auto list = get<typename DataType::list>(value);
      expect(get<typename DataType::integer>(list[0]), equal_to(666));
    });

    _.test("dict", []() {
      std::string data("d4:spami666ee");
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

      expect(get<typename DataType::integer>(dict["spam"]), equal_to(666));
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

  subsuite<>(_, "error handling", [](auto &_) {
    _.test("unexpected type", []() {
      expect([]() { bencode::decode("x"); },
             thrown<std::invalid_argument>("unexpected type"));
    });

    _.test("unexpected end of string", []() {
      auto eos = thrown<std::invalid_argument>("unexpected end of string");

      expect([]() { bencode::decode(""); }, eos);
      expect([]() { bencode::decode("i123"); }, eos);
      expect([]() { bencode::decode("3"); }, eos);
      expect([]() { bencode::decode("3:as"); }, eos);
      expect([]() { bencode::decode("l"); }, eos);
      expect([]() { bencode::decode("li1e"); }, eos);
      expect([]() { bencode::decode("d"); }, eos);
      expect([]() { bencode::decode("d1:a"); }, eos);
      expect([]() { bencode::decode("d1:ai1e"); }, eos);
    });

    _.test("expected 'e'", []() {
      expect([]() { bencode::decode("i123i"); },
             thrown<std::invalid_argument>("expected 'e'"));
    });

    _.test("expected ':'", []() {
      expect([]() { bencode::decode("1abc"); },
             thrown<std::invalid_argument>("expected ':'"));
    });

    _.test("expected string token", []() {
      expect([]() { bencode::decode("di123ee"); },
             thrown<std::invalid_argument>("expected string token"));
    });

    _.test("duplicated key", []() {
      expect([]() { bencode::decode("d3:fooi1e3:fooi1ee"); },
             thrown<std::invalid_argument>("duplicated key in dict: foo"));
    });
  });

});
