#include <cassert>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// Try to use N4082's any class, or fall back to Boost's.
#ifdef __has_include
#  if __has_include(<experimental/any>)
#    include <experimental/any>
#    define BENCODE_ANY_NAMESPACE std::experimental
#  else
#    include <boost/any.hpp>
#    define BENCODE_ANY_NAMESPACE boost
#  endif
#else
#  include <boost/any.hpp>
#  define BENCODE_ANY_NAMESPACE boost
#endif

namespace bencode {

  using any = BENCODE_ANY_NAMESPACE::any;

  using string = std::string;
  using integer = long long;
  using list = std::vector<boost::any>;
  using dict = std::map<std::string, boost::any>;

  template<typename T>
  any decode(T &begin, T end);

  namespace detail {

    template<typename T>
    string decode_str(T &begin, T end) {
      size_t len = 0;
      for(; begin != end && std::isdigit(*begin); ++begin)
        len = len * 10 + *begin - '0';

      if(begin == end)
        throw std::invalid_argument("unexpected end of string");
      if(*begin != ':')
        throw std::invalid_argument("expected ':'");
      ++begin;
      if(std::distance(begin, end) < static_cast<ssize_t>(len))
        throw std::invalid_argument("length exceeds total");

      std::string value(len, 0);
      T str_start = begin;
      std::advance(begin, len);
      std::copy(str_start, begin, value.begin());
      return value;
    }

    template<typename T>
    integer decode_int(T &begin, T end) {
      assert(*begin == 'i');
      ++begin;

      bool positive = true;
      if(*begin == '-') {
        positive = false;
        ++begin;
      }

      // TODO: handle overflow
      integer value = 0;
      for(; begin != end && std::isdigit(*begin); ++begin)
        value = value * 10 + *begin - '0';
      if(begin == end)
        throw std::invalid_argument("unexpected end of string");
      if(*begin != 'e')
        throw std::invalid_argument("expected 'e'");

      ++begin;
      return positive ? value : -value;
    }

    template<typename T>
    list decode_list(T &begin, T end) {
      assert(*begin == 'l');
      ++begin;

      list value;
      while(begin != end && *begin != 'e') {
        value.push_back(decode(begin, end));
      }

      if(begin == end)
        throw std::invalid_argument("unexpected end of string");

      ++begin;
      return value;
    }

    template<typename T>
    dict decode_dict(T &begin, T end) {
      assert(*begin == 'd');
      ++begin;

      dict value;
      while(begin != end && *begin != 'e') {
        auto key = decode_str(begin, end);
        value[key] = decode(begin, end);
      }

      if(begin == end)
        throw std::invalid_argument("unexpected end of string");

      ++begin;
      return value;
    }

  }

  template<typename T>
  any decode(T &begin, T end) {
    if(begin == end)
      return any();

    if(*begin == 'i')
      return detail::decode_int(begin, end);
    else if(*begin == 'l')
      return detail::decode_list(begin, end);
    else if(*begin == 'd')
      return detail::decode_dict(begin, end);
    else if(std::isdigit(*begin))
      return detail::decode_str(begin, end);

    throw std::runtime_error("unexpected type");
  }

  template<typename T>
  inline any decode(const T &begin, T end) {
    T b(begin);
    return decode(b, end);
  }

  inline any decode(const std::string &s) {
    return decode(s.begin(), s.end());
  }


  // TODO: make these use unformatted output?
  inline std::ostream & encode(std::ostream &os, integer value) {
    return os << "i" << value << "e";
  }

  inline std::ostream & encode(std::ostream &os, const string &value) {
    return os << value.size() << ":" << value;
  }

  template<typename T>
  std::ostream & encode(std::ostream &os, const std::vector<T> &value);

  template<typename T>
  std::ostream & encode(std::ostream &os,
                        const std::map<std::string, T> &value);

  class encode_list {
  public:
    encode_list(std::ostream &os) : os(os) {
      os.put('l');
    }

    template<typename T>
    encode_list & add(T &&value) {
      encode(os, std::forward<T>(value));
      return *this;
    }

    std::ostream & end() {
      return os.put('e');
    }
  private:
    std::ostream &os;
  };

  class encode_dict {
  public:
    encode_dict(std::ostream &os) : os(os) {
      os.put('d');
    }

    template<typename T>
    encode_dict & add(const string &key, T &&value) {
      encode(os, key);
      encode(os, std::forward<T>(value));
      return *this;
    }

    std::ostream & end() {
      return os.put('e');
    }
  private:
    std::ostream &os;
  };

  template<typename T>
  std::ostream & encode(std::ostream &os, const std::vector<T> &value) {
    encode_list e(os);
    for(auto &&i : value)
      e.add(i);
    return e.end();
  }

  template<typename T>
  std::ostream & encode(std::ostream &os,
                        const std::map<std::string, T> &value) {
    encode_dict e(os);
    for(auto &&i : value)
      e.add(i.first, i.second);
    return e.end();
  }

  template<typename T>
  std::string encode(T &&t) {
    std::stringstream ss;
    encode(ss, t);
    return ss.str();
  }

}
