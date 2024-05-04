#include <mettle.hpp>
using namespace mettle;

#include "bencode.hpp"
#include "../matchers.hpp"

suite<> test_decode_integers("decode integers", [](auto &_) {
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
