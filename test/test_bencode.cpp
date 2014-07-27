#include <mettle.hpp>
using namespace mettle;

#include "bencode.hpp"

suite<> test_decode("test decoder", [](auto &_) {

  using BENCODE_ANY_NS::any_cast;

  subsuite<std::string, std::istringstream>(_, "decoding", [](auto &_) {
    using Fixture = fixture_type_t<decltype(_)>;

    _.test("integer", [](auto &) {
      Fixture pos("i666e");
      auto pos_value = bencode::decode(pos);
      expect(any_cast<bencode::integer>(pos_value), equal_to(666));

      Fixture neg("i-666e");
      auto neg_value = bencode::decode(neg);
      expect(any_cast<bencode::integer>(neg_value), equal_to(-666));
    });

    _.test("string", [](auto &) {
      Fixture data("4:spam");
      auto value = bencode::decode(data);
      expect(any_cast<bencode::string>(value), equal_to("spam"));
    });

    _.test("list", [](auto &) {
      Fixture data("li666ee");
      auto value = bencode::decode(data);
      auto list = any_cast<bencode::list>(value);
      expect(any_cast<bencode::integer>(list[0]), equal_to(666));
    });

    _.test("dict", [](auto &) {
      Fixture data("d4:spami666ee");
      auto value = bencode::decode(data);
      auto dict = any_cast<bencode::dict>(value);
      expect(any_cast<bencode::integer>(dict["spam"]), equal_to(666));
    });
  });

  subsuite<>(_, "decode successive objects", [](auto &_) {
    _.test("from string", []() {
      std::string data("i666e4:goat");
      auto begin = data.begin(), end = data.end();

      auto first = bencode::decode(begin, end);
      expect(any_cast<bencode::integer>(first), equal_to(666));

      auto second = bencode::decode(begin, end);
      expect(any_cast<bencode::string>(second), equal_to("goat"));
    });

    _.test("from stream", []() {
      std::stringstream data("i666e4:goat");

      auto first = bencode::decode(data);
      expect(any_cast<bencode::integer>(first), equal_to(666));

      auto second = bencode::decode(data);
      expect(any_cast<bencode::string>(second), equal_to("goat"));
    });
  });

  subsuite<>(_, "decode_view", [](auto &_) {
    _.test("integer", []() {
      std::string pos("i666e");
      auto pos_value = bencode::decode_view(pos);
      expect(any_cast<bencode::integer>(pos_value), equal_to(666));

      std::string neg("i-666e");
      auto neg_value = bencode::decode_view(neg);
      expect(any_cast<bencode::integer>(neg_value), equal_to(-666));
    });

    _.test("string", []() {
      std::string data("4:spam");
      auto in_range = all(
        greater_equal(data.data()),
        less_equal(data.data() + data.size())
      );

      auto value = bencode::decode_view(data);
      auto str = any_cast<bencode::string_view>(value);
      expect(str.begin(), in_range);
      expect(str.end(), in_range);
      expect(str, equal_to("spam"));
    });

    _.test("list", []() {
      std::string data("li666ee");
      auto value = bencode::decode_view(data);
      auto list = any_cast<bencode::list>(value);
      expect(any_cast<bencode::integer>(list[0]), equal_to(666));
    });

    _.test("dict", []() {
      std::string data("d4:spami666ee");
      auto in_range = all(
        greater_equal(data.data()),
        less_equal(data.data() + data.size())
      );

      auto value = bencode::decode_view(data);
      auto dict = any_cast<bencode::dict_view>(value);
      auto str = dict.find("spam")->first;
      expect(str.begin(), in_range);
      expect(str.end(), in_range);
      expect(str, equal_to("spam"));

      expect(any_cast<bencode::integer>(dict["spam"]), equal_to(666));
    });
  });

  subsuite<>(_, "error handling", [](auto &_) {
    _.test("unexpected type", []() {
      expect([]() { bencode::decode("x"); },
             thrown<std::invalid_argument>("unexpected type"));
    });

    _.test("unexpected end of string", []() {
      auto eos = thrown<std::invalid_argument>("unexpected end of string");

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

suite<> test_encode("test encoder", [](auto &_) {

  _.test("integer", []() {
    expect(bencode::encode(666), equal_to("i666e"));
  });

  _.test("string", []() {
    expect(bencode::encode("foo"), equal_to("3:foo"));
    expect(bencode::encode(std::string("foo")), equal_to("3:foo"));
  });

  _.test("list", []() {
    std::stringstream ss1;
    bencode::encode_list(ss1);
    expect(ss1.str(), equal_to("le"));

    std::stringstream ss2;
    bencode::encode_list(ss2, 1, "foo", 2);
    expect(ss2.str(), equal_to("li1e3:fooi2ee"));
  });

  _.test("dict", []() {
    std::stringstream ss1;
    bencode::encode_dict(ss1);
    expect(ss1.str(), equal_to("de"));

    std::stringstream ss2;
    bencode::encode_dict(ss2,
      "first", 1,
      "second", "foo",
      "third", 2
    );
    expect(ss2.str(), equal_to("d5:firsti1e6:second3:foo5:thirdi2ee"));
  });

  _.test("list_encoder", []() {
    std::stringstream ss;
    bencode::list_encoder(ss)
      .add(1).add("foo").add(2)
      .end();
    expect(ss.str(), equal_to("li1e3:fooi2ee"));
  });

  _.test("dict_encoder", []() {
    std::stringstream ss;
    bencode::dict_encoder(ss)
      .add("first", 1).add("second", "foo").add("third", 2)
      .end();
    expect(ss.str(), equal_to("d5:firsti1e6:second3:foo5:thirdi2ee"));
  });

  subsuite<>(_, "any", [](auto &_) {
    using BENCODE_ANY_NS::any;

    _.test("integer", []() {
      any value = bencode::integer(666);
      expect(bencode::encode(value), equal_to("i666e"));
    });

    _.test("string", []() {
      any value = bencode::string("foo");
      expect(bencode::encode(value), equal_to("3:foo"));
    });

    _.test("list", []() {
      any value = bencode::list{
        bencode::integer(1),
        bencode::string("foo"),
        bencode::integer(2)
      };
      expect(bencode::encode(value), equal_to("li1e3:fooi2ee"));
    });

    _.test("dict", []() {
      any value = bencode::dict{
        { bencode::string("a"), bencode::integer(1) },
        { bencode::string("b"), bencode::string("foo") },
        { bencode::string("c"), bencode::integer(2) }
      };
      expect(
        bencode::encode(value), equal_to("d1:ai1e1:b3:foo1:ci2ee")
      );
    });
  });

  subsuite<>(_, "vector", [](auto &_) {
    _.test("vector<int>", []() {
      std::vector<int> v = {1, 2, 3};
      expect(bencode::encode(v), equal_to("li1ei2ei3ee"));
    });

    _.test("vector<string>", []() {
      std::vector<std::string> v = {"cat", "dog", "goat"};
      expect(bencode::encode(v), equal_to("l3:cat3:dog4:goate"));
    });

    _.test("vector<vector<int>>", []() {
      std::vector<std::vector<int>> v = {{1}, {1, 2}, {1, 2, 3}};
      expect(bencode::encode(v), equal_to("lli1eeli1ei2eeli1ei2ei3eee"));
    });
  });

  subsuite<>(_, "map", [](auto &_) {
    _.test("map<string, int>", []() {
      std::map<std::string, int> m = {{"a", 1}, {"b", 2}, {"c", 3}};
      expect(bencode::encode(m), equal_to("d1:ai1e1:bi2e1:ci3ee"));
    });

    _.test("map<string, string>", []() {
      std::map<std::string, std::string> m = {
        {"a", "cat"}, {"b", "dog"}, {"c", "goat"}
      };
      expect(bencode::encode(m), equal_to("d1:a3:cat1:b3:dog1:c4:goate"));
    });

    _.test("map<string, map<string, int>>", []() {
      std::map<std::string, std::map<std::string, int>> m = {
        { "a", {{"a", 1}} },
        { "b", {{"a", 1}, {"b", 2}} },
        { "c", {{"a", 1}, {"b", 2}, {"c", 3}} }
      };
      expect(bencode::encode(m), equal_to(
        "d1:ad1:ai1ee1:bd1:ai1e1:bi2ee1:cd1:ai1e1:bi2e1:ci3eee"
      ));
    });
  });

});
