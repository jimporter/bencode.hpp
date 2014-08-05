#include <mettle.hpp>
using namespace mettle;

#include "bencode.hpp"

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
