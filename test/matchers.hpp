#ifndef INC_BENCODE_TEST_MATCHERS_HPP
#define INC_BENCODE_TEST_MATCHERS_HPP

#include <mettle.hpp>
using namespace mettle;

struct at_eof_base : matcher_tag {
  std::string desc() const {
    return "at eof";
  }
};

template<typename ...T>
struct at_eof;

template<typename Char>
struct at_eof<Char *> : at_eof_base {
  at_eof(const Char *) {}
  bool operator ()(const Char *s) const { return *s == Char('\0'); }
};

template<typename Char>
struct at_eof<Char *, std::size_t> : at_eof_base {
  at_eof(const Char *start, std::size_t len) : end(start + len) {}
  bool operator ()(const Char *s) const { return s == end; }
  Char * end;
};

template<typename Iter>
struct at_eof<Iter, Iter> : at_eof_base {
  at_eof(Iter, Iter end) : end(end) {}
  bool operator ()(Iter i) const { return i == end; }
  Iter end;
};

template<typename Char>
struct at_eof<std::basic_istringstream<Char>> : at_eof_base {
  using ios_type = std::basic_istringstream<Char>;
  at_eof(ios_type&) {}
  bool operator ()(const ios_type &ss) const { return ss.eof(); }
};

template<typename ...T>
at_eof(T &&...t) -> at_eof<std::remove_reference_t<std::remove_cv_t<T>>...>;

template<typename T>
auto within_memory(const T &t) {
  if constexpr(bencode::detail::is_iterable_v<T>) {
    return in_interval(&*std::begin(t), &*std::end(t), interval::closed);
  } else {
    return is_not(anything());
  }
}

template<typename T>
auto within_memory(const T *t) {
  return in_interval(t, t + bencode::detail::any_strlen(t), interval::closed);
}

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

#endif
