#include <mettle.hpp>
using namespace mettle;

#include "bencode.hpp"

suite<> test_encode("test encoder", [](auto &_) {

  _.test("integer", []() {
    expect(bencode::encode(42), equal_to("i42e"));
    expect(bencode::encode(bencode::integer(42)), equal_to("i42e"));
  });

  _.test("string", []() {
    expect(bencode::encode("foo"), equal_to("3:foo"));
    expect(bencode::encode(std::string("foo")), equal_to("3:foo"));
    expect(bencode::encode(bencode::string("foo")), equal_to("3:foo"));
  });

  _.test("list", []() {
    expect(bencode::encode(bencode::list{}), equal_to("le"));
    expect(bencode::encode(bencode::list{1, "foo", 2}),
           equal_to("l" "i1e" "3:foo" "i2e" "e"));
  });

  _.test("dict", []() {
    expect(bencode::encode(bencode::dict{}), equal_to("de"));
    expect(bencode::encode(bencode::dict{
      {"one", 1},
      {"two", "foo"},
      {"three", 2}
    }), equal_to("d" "3:one" "i1e" "5:three" "i2e" "3:two" "3:foo" "e"));
  });

  _.test("nested", []() {
    expect(bencode::encode(bencode::dict{
      {"one", 1},
      {"two", bencode::list{
        3, "foo", 4
      }},
      {"three", bencode::list{
        bencode::dict{
          {"foo", 0},
          {"bar", 0}
        }
      }}
    }), equal_to("d"
      "3:one" "i1e"
      "5:three" "l" "d" "3:bar" "i0e" "3:foo" "i0e" "e" "e"
      "3:two" "l" "i3e" "3:foo" "i4e" "e"
    "e"));
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

  subsuite<
    bencode::data, bencode::boost_data
  >(_, "data", type_only, [](auto &_) {
    using DataType = fixture_type_t<decltype(_)>;

    _.test("integer", []() {
      DataType d = 42;
      expect(bencode::encode(d), equal_to("i42e"));
    });

    _.test("string", []() {
      DataType d = "foo";
      expect(bencode::encode(d), equal_to("3:foo"));
    });

    _.test("list", []() {
      DataType d1 = typename DataType::list{};
      expect(bencode::encode(d1), equal_to("le"));

      DataType d2 = typename DataType::list{1, "foo", 2};
      expect(bencode::encode(d2),
             equal_to("l" "i1e" "3:foo" "i2e" "e"));
    });

    _.test("dict", []() {
      DataType d1 = typename DataType::dict{};
      expect(bencode::encode(d1), equal_to("de"));
      DataType d2 = typename DataType::dict{
        {"one", 1},
        {"two", "foo"},
        {"three", 2}
      };
      expect(bencode::encode(d2),
             equal_to("d" "3:one" "i1e" "5:three" "i2e" "3:two" "3:foo" "e"));
    });
  });

});
