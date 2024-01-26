#include <mettle.hpp>
using namespace mettle;

#include "bencode.hpp"

static const std::string nested_data("d"
    "3:one" "i1e"
    "5:three" "l" "d" "3:bar" "i0e" "3:foo" "i0e" "e" "e"
    "3:two" "l" "i3e" "3:foo" "i4e" "e"
  "e");

suite<
  bencode::data, bencode::boost_data
> test_data("test data", type_only, [](auto &_) {
  using DataType = fixture_type_t<decltype(_)>;
  using boost::get;
  using std::get;

  subsuite<>(_, "operator []", [](auto &_) {
    _.test("get (lvalue)", []() {
      auto value = bencode::basic_decode<DataType>(nested_data);
      expect(get<long long>(value["three"][0]["bar"]), equal_to(0));
    });

    _.test("get (rvalue)", []() {
      auto value = bencode::basic_decode<DataType>(nested_data);
      expect(get<long long>(std::move(value)["three"][0]["bar"]), equal_to(0));
    });

    _.test("set", []() {
      auto value = bencode::basic_decode<DataType>("de");
      value["foo"] = 1;
      value["bar"] = "two";
      expect(bencode::encode(value),
             equal_to("d" "3:bar" "3:two" "3:foo" "i1e" "e"));
    });
  });

  subsuite<>(_, "at", [](auto &_) {
    _.test("get (lvalue)", []() {
      auto value = bencode::basic_decode<DataType>(nested_data);
      expect(get<long long>(value.at("three").at(0).at("bar")),
             equal_to(0));
    });

    _.test("get (rvalue)", []() {
      auto value = bencode::basic_decode<DataType>(nested_data);
      expect(get<long long>(std::move(value).at("three").at(0).at("bar")),
             equal_to(0));
    });

    _.test("get (const lvalue)", []() {
      const auto value = bencode::basic_decode<DataType>(nested_data);
      expect(get<long long>(value.at("three").at(0).at("bar")),
             equal_to(0));
    });

    _.test("get (const rvalue)", []() {
      const auto value = bencode::basic_decode<DataType>(nested_data);
      expect(get<long long>(std::move(value).at("three").at(0).at("bar")),
             equal_to(0));
    });
  });

});
