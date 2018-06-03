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

suite<> test_decode("test decoder", [](auto &_) {

  subsuite<
    const char *, std::string, std::istringstream
  >(_, "decoding", type_only, [](auto &_) {
    using Fixture = fixture_type_t<decltype(_)>;

    _.test("integer", []() {
      Fixture pos("i666e");
      auto pos_value = bencode::decode(pos);
      expect(pos, at_eof());
      expect(boost::get<bencode::integer>(pos_value), equal_to(666));

      Fixture neg("i-666e");
      auto neg_value = bencode::decode(neg);
      expect(neg, at_eof());
      expect(boost::get<bencode::integer>(neg_value), equal_to(-666));
    });

    _.test("string", []() {
      Fixture data("4:spam");
      auto value = bencode::decode(data);
      expect(data, at_eof());
      expect(boost::get<bencode::string>(value), equal_to("spam"));
    });

    _.test("list", []() {
      Fixture data("li666ee");
      auto value = bencode::decode(data);
      auto list = boost::get<bencode::list>(value);
      expect(data, at_eof());
      expect(boost::get<bencode::integer>(list[0]), equal_to(666));
    });

    _.test("dict", []() {
      Fixture data("d4:spami666ee");
      auto value = bencode::decode(data);
      auto dict = boost::get<bencode::dict>(value);
      expect(data, at_eof());
      expect(boost::get<bencode::integer>(dict["spam"]), equal_to(666));
    });
  });

  subsuite<>(_, "decode successive objects", [](auto &_) {
    _.test("from string", []() {
      std::string data("i666e4:goat");
      auto begin = data.begin(), end = data.end();

      auto first = bencode::decode(begin, end);
      expect(boost::get<bencode::integer>(first), equal_to(666));

      auto second = bencode::decode(begin, end);
      expect(boost::get<bencode::string>(second), equal_to("goat"));
    });

    _.test("from stream", []() {
      std::stringstream data("i666e4:goat");

      auto first = bencode::decode(data);
      expect(boost::get<bencode::integer>(first), equal_to(666));
      expect(data, is_not(at_eof()));

      auto second = bencode::decode(data);
      expect(boost::get<bencode::string>(second), equal_to("goat"));
      expect(data, at_eof());
    });
  });

  subsuite<>(_, "decode_view", [](auto &_) {
    _.test("integer", []() {
      std::string pos("i666e");
      auto pos_value = bencode::decode_view(pos);
      expect(boost::get<bencode::integer_view>(pos_value), equal_to(666));

      std::string neg("i-666e");
      auto neg_value = bencode::decode_view(neg);
      expect(boost::get<bencode::integer_view>(neg_value), equal_to(-666));
    });

    _.test("string", []() {
      std::string data("4:spam");
      auto in_range = all(
        greater_equal(data.data()),
        less_equal(data.data() + data.size())
      );

      auto value = bencode::decode_view(data);
      auto str = boost::get<bencode::string_view>(value);
      expect(&*str.begin(), in_range);
      expect(&*str.end(), in_range);
      expect(str, equal_to("spam"));
    });

    _.test("list", []() {
      std::string data("li666ee");
      auto value = bencode::decode_view(data);
      auto list = boost::get<bencode::list_view>(value);
      expect(boost::get<bencode::integer_view>(list[0]), equal_to(666));
    });

    _.test("dict", []() {
      std::string data("d4:spami666ee");
      auto in_range = all(
        greater_equal(data.data()),
        less_equal(data.data() + data.size())
      );

      auto value = bencode::decode_view(data);
      auto dict = boost::get<bencode::dict_view>(value);
      auto str = dict.find("spam")->first;
      expect(&*str.begin(), in_range);
      expect(&*str.end(), in_range);
      expect(str, equal_to("spam"));

      expect(boost::get<bencode::integer_view>(dict["spam"]), equal_to(666));
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
