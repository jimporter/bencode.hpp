#include <mettle.hpp>
using namespace mettle;

#include "bencode.hpp"
#include "../matchers.hpp"

suite<> test_decode_errors("decode error handling", [](auto &_) {
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
    expect([]() { bencode::decode("d3:fooi1e3:fooi1ee"); },
           decode_error<bencode::syntax_error>(
             "duplicated key in dict: \"foo\"", 17
           ));
  });
});
