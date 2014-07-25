#include <mettle.hpp>
using namespace mettle;

#include "bencode.hpp"

suite<> test_decode("test decoding", [](auto &_) {

  // Try to use N4082's any_casat, or fall back to Boost's.
  #ifdef __has_include
  #  if __has_include(<experimental/any>)
  using std::experimental::any_cast;
  #  else
  using boost::any_cast;
  #  endif
  #else
  using boost::any_cast;
  #endif

  _.test("string", []() {
    auto value = bencode::decode("4:spam");
    expect(any_cast<bencode::string>(value), equal_to("spam"));
  });

  _.test("integer", []() {
    auto value = bencode::decode("i666e");
    expect(any_cast<bencode::integer>(value), equal_to(666));

    auto neg_value = bencode::decode("i-666e");
    expect(any_cast<bencode::integer>(neg_value), equal_to(-666));
  });

  _.test("dict", []() {
    auto value = bencode::decode("d4:spami666ee");
    auto d = any_cast<bencode::dict>(value);
    expect(any_cast<bencode::integer>(d["spam"]), equal_to(666));
  });

  _.test("list", []() {
    auto value = bencode::decode("li666ee");
    auto list = any_cast<bencode::list>(value);
    expect(any_cast<bencode::integer>(list[0]), equal_to(666));
  });
});
