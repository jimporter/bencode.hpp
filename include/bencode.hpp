#ifndef INC_BENCODE_HPP
#define INC_BENCODE_HPP

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstddef>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#if __has_include(<charconv>)
#  include <charconv>
#  define BENCODE_HAS_CHARCONV
#endif

#if __has_include(<boost/variant.hpp>)
#  include <boost/variant.hpp>
#  define BENCODE_HAS_BOOST
#endif

namespace bencode {

  template<template<typename ...> typename T>
  struct variant_traits;

#define BENCODE_MAP_PROXY_FN_1(name, specs)                                   \
  template<typename T>                                                        \
  auto name(T &&t) specs { return proxy_->name(std::forward<T>(t)); }

#define BENCODE_MAP_PROXY_FN_N(name, specs)                                   \
  template<typename ...T>                                                     \
  auto name(T &&...t) specs { return proxy_->name(std::forward<T>(t)...); }

  // A proxy of std::map, since the standard doesn't require that map support
  // incomplete types.
  template<typename Key, typename Value>
  class map_proxy {
  public:
    using map_type = std::map<Key, Value>;
    using key_type = Key;
    using mapped_type = Value;
    using value_type = std::pair<const Key, Value>;

    // Construction/assignment
    map_proxy() : proxy_(new map_type()) {}
    map_proxy(const map_proxy &rhs) : proxy_(new map_type(*rhs.proxy_)) {}
    map_proxy(map_proxy &&rhs) noexcept : proxy_(std::move(rhs.proxy_)) {}
    map_proxy(std::initializer_list<value_type> i) : proxy_(new map_type(i)) {}

    map_proxy operator =(const map_proxy &rhs) {
      *proxy_ = *rhs.proxy_;
      return *this;
    }

    map_proxy operator =(const map_proxy &&rhs) {
      *proxy_ = std::move(*rhs.proxy_);
      return *this;
    }

    void swap(const map_proxy &rhs) { proxy_->swap(*rhs.proxy); }

    operator map_type &() { return *proxy_; };
    operator const map_type &() const { return *proxy_; };

    // Pointer access
    map_type & operator *() { return *proxy_; }
    const map_type & operator *() const { return *proxy_; }
    map_type * operator ->() { return proxy_.get(); }
    const map_type * operator ->() const { return proxy_.get(); }

    // Element access
    template<typename K>
    mapped_type & at(K &&k) { return proxy_->at(std::forward<K>(k)); }
    template<typename K>
    const mapped_type &
    at(K &&k) const { return proxy_->at(std::forward<K>(k)); }
    template<typename K>
    mapped_type & operator [](K &&k) { return (*proxy_)[std::forward<K>(k)]; }

    // Iterators
    auto begin() noexcept { return proxy_->begin(); }
    auto begin() const noexcept { return proxy_->begin(); }
    auto cbegin() const noexcept { return proxy_->cbegin(); }
    auto end() noexcept { return proxy_->end(); }
    auto end() const noexcept { return proxy_->end(); }
    auto cend() const noexcept { return proxy_->cend(); }
    auto rbegin() noexcept { return proxy_->rbegin(); }
    auto rbegin() const noexcept { return proxy_->rbegin(); }
    auto crbegin() const noexcept { return proxy_->crbegin(); }
    auto rend() noexcept { return proxy_->rend(); }
    auto rend() const noexcept { return proxy_->rend(); }
    auto crend() const noexcept { return proxy_->crend(); }

    // Capacity
    bool empty() const noexcept { return proxy_->empty(); }
    auto size() const noexcept { return proxy_->size(); }
    auto max_size() const noexcept { return proxy_->max_size(); }

    // Modifiers
    void clear() noexcept { proxy_->clear(); }
    BENCODE_MAP_PROXY_FN_N(insert,)
    BENCODE_MAP_PROXY_FN_N(insert_or_assign,)
    BENCODE_MAP_PROXY_FN_N(emplace,)
    BENCODE_MAP_PROXY_FN_N(emplace_hint,)
    BENCODE_MAP_PROXY_FN_N(try_emplace,)
    BENCODE_MAP_PROXY_FN_N(erase,)

    // Lookup
    BENCODE_MAP_PROXY_FN_1(count, const)
    BENCODE_MAP_PROXY_FN_1(find,)
    BENCODE_MAP_PROXY_FN_1(find, const)
    BENCODE_MAP_PROXY_FN_1(equal_range,)
    BENCODE_MAP_PROXY_FN_1(equal_range, const)
    BENCODE_MAP_PROXY_FN_1(lower_bound,)
    BENCODE_MAP_PROXY_FN_1(lower_bound, const)
    BENCODE_MAP_PROXY_FN_1(upper_bound,)
    BENCODE_MAP_PROXY_FN_1(upper_bound, const)

    auto key_comp() const { return proxy_->key_comp(); }
    auto value_comp() const { return proxy_->value_comp(); }

  private:
    std::unique_ptr<map_type> proxy_;
  };

#define BENCODE_MAP_PROXY_RELOP(op)                                           \
  template<typename Key, typename Value>                                      \
  bool operator op(const map_proxy<Key, Value> &lhs,                          \
                   const map_proxy<Key, Value> &rhs) {                        \
    return *lhs == *rhs;                                                      \
  }

  BENCODE_MAP_PROXY_RELOP(==)
  BENCODE_MAP_PROXY_RELOP(!=)
  BENCODE_MAP_PROXY_RELOP(>=)
  BENCODE_MAP_PROXY_RELOP(<=)
  BENCODE_MAP_PROXY_RELOP(>)
  BENCODE_MAP_PROXY_RELOP(<)

  template<template<typename ...> typename Variant, typename I, typename S,
           template<typename ...> typename L, template<typename ...> typename D>
  struct basic_data : Variant<I, S, L<basic_data<Variant, I, S, L, D>>,
                              D<S, basic_data<Variant, I, S, L, D>>> {
    using integer = I;
    using string = S;
    using list = L<basic_data>;
    using dict = D<S, basic_data>;

    using base_type = Variant<integer, string, list, dict>;
    using base_type::base_type;

    base_type & base() & { return *this; }
    base_type && base() && { return *this; }
    const base_type & base() const & { return *this; }
  };

  template<typename T>
  struct variant_traits_for;

  template<template<typename ...> typename Variant,
           typename I, typename S, template<typename ...> typename L,
           template<typename ...> typename D>
  struct variant_traits_for<basic_data<Variant, I, S, L, D>>
    : variant_traits<Variant> {};

  template<>
  struct variant_traits<std::variant> {
    template<typename Visitor, typename ...Variants>
    inline static decltype(auto)
    visit(Visitor &&visitor, Variants &&...variants) {
      return std::visit(std::forward<Visitor>(visitor),
                        std::forward<Variants>(variants).base()...);
    }

    template<typename Type, typename Variant>
    inline static decltype(auto) get_if(Variant *variant) {
      return std::get_if<Type>(&variant->base());
    }

    template<typename Type, typename Variant>
    inline static decltype(auto) get_if(const Variant *variant) {
      return std::get_if<Type>(&variant->base());
    }

    template<typename Variant>
    inline static auto index(const Variant &variant) {
      return variant.index();
    }
  };

  using data = basic_data<std::variant, long long, std::string, std::vector,
                          map_proxy>;
  using data_view = basic_data<std::variant, long long, std::string_view,
                               std::vector, map_proxy>;

#ifdef BENCODE_HAS_BOOST
  using boost_data = basic_data<boost::variant, long long, std::string,
                                std::vector, map_proxy>;
  using boost_data_view = basic_data<boost::variant, long long,
                                     std::string_view, std::vector, map_proxy>;

  template<>
  struct variant_traits<boost::variant> {
    template<typename Visitor, typename ...Variants>
    static decltype(auto)
    visit(Visitor &&visitor, Variants &&...variants) {
      return boost::apply_visitor(std::forward<Visitor>(visitor),
                                  std::forward<Variants>(variants).base()...);
    }

    template<typename Type, typename Variant>
    inline static decltype(auto) get_if(Variant *variant) {
      return boost::get<Type>(variant);
    }

    template<typename Type, typename Variant>
    inline static decltype(auto) get_if(const Variant *variant) {
      return boost::get<Type>(variant);
    }

    template<typename Variant>
    inline static auto index(const Variant &variant) {
      return variant.which();
    }
  };
#endif

  using integer = data::integer;
  using string = data::string;
  using list = data::list;
  using dict = data::dict;

  using integer_view = data_view::integer;
  using string_view = data_view::string;
  using list_view = data_view::list;
  using dict_view = data_view::dict;

  enum eof_behavior {
    check_eof,
    no_check_eof
  };

  template<typename Data, typename Iter>
  Data basic_decode(Iter &begin, Iter end);

  namespace detail {

    template<typename Integer>
    inline void check_overflow(Integer value, Integer digit) {
      using limits = std::numeric_limits<Integer>;
      // Wrap `max` in parentheses to work around <windows.h> #defining `max`.
      if((value > (limits::max)() / 10) ||
         (value == (limits::max)() / 10 && digit > (limits::max)() % 10))
        throw std::invalid_argument("integer overflow");
    }

    template<typename Integer>
    inline void check_underflow(Integer value, Integer digit) {
      using limits = std::numeric_limits<Integer>;
      // As above, work around <windows.h> #defining `min`.
      if((value < (limits::min)() / 10) ||
         (value == (limits::min)() / 10 && digit < (limits::min)() % 10))
        throw std::invalid_argument("integer underflow");
    }

    template<typename Integer>
    inline void
    check_over_underflow(Integer value, Integer digit, Integer sgn) {
      if(sgn == 1)
        check_overflow(value, digit);
      else
        check_underflow(value, digit);
    }

    template<typename Integer, typename Iter>
    inline Integer
    decode_digits(Iter &begin, Iter end, [[maybe_unused]] Integer sgn = 1) {
      assert(sgn == 1 || (std::is_signed_v<Integer> &&
                          std::make_signed_t<Integer>(sgn) == -1));

      Integer value = 0;

      // For performance, decode as many digits as we know will fit within an
      // `Integer` value, and then if there are any more beyond that, do
      // proper overflow detection.
      for(int i = 0; i != std::numeric_limits<Integer>::digits10; i++) {
        if(begin == end)
          throw std::invalid_argument("unexpected end of string");
        if(!std::isdigit(*begin))
          return value;

        if constexpr(std::is_signed_v<Integer>)
          value = value * 10 + (*begin++ - '0') * sgn;
        else
          value = value * 10 + (*begin++ - '0');
      }
      if(begin == end)
        throw std::invalid_argument("unexpected end of string");

      // We're approaching the limits of what `Integer` can hold. Check for
      // overflow.
      if(std::isdigit(*begin)) {
        Integer digit;
        if constexpr(std::is_signed_v<Integer>) {
          digit = (*begin++ - '0') * sgn;
          check_over_underflow(value, digit, sgn);
        } else {
          digit = (*begin++ - '0');
          check_overflow(value, digit);
        }
        value = value * 10 + digit;
      }

      // Still more digits? That's too many!
      if(std::isdigit(*begin)) {
        if(sgn == 1)
          throw std::invalid_argument("integer overflow");
        else
          throw std::invalid_argument("integer underflow");
      }

      return value;
    }

    template<typename Integer, typename Iter>
    Integer decode_int(Iter &begin, Iter end) {
      assert(*begin == 'i');
      ++begin;
      Integer sgn = 1;
      if(*begin == '-') {
        if constexpr(std::is_unsigned_v<Integer>) {
          throw std::invalid_argument("expected unsigned integer");
        } else {
          sgn = -1;
          ++begin;
        }
      }

      Integer value = decode_digits<Integer>(begin, end, sgn);
      if(*begin != 'e')
        throw std::invalid_argument("expected 'e'");

      ++begin;
      return value;
    }

    template<typename String>
    class str_reader {
    public:
      template<typename Iter, typename Size>
      inline String operator ()(Iter &begin, Iter end, Size len) {
        return call(
          begin, end, len,
          typename std::iterator_traits<Iter>::iterator_category()
        );
      }
    private:
      template<typename Iter, typename Size>
      String call(Iter &begin, Iter end, Size len, std::forward_iterator_tag) {
        if(std::distance(begin, end) < static_cast<std::ptrdiff_t>(len))
          throw std::invalid_argument("unexpected end of string");

        auto orig = begin;
        std::advance(begin, len);
        return String(orig, begin);
      }

      template<typename Iter, typename Size>
      String call(Iter &begin, Iter end, Size len, std::input_iterator_tag) {
        String value(len, 0);
        for(Size i = 0; i < len; i++) {
          if(begin == end)
            throw std::invalid_argument("unexpected end of string");
          value[i] = *begin++;
        }
        return value;
      }
    };

    template<>
    class str_reader<std::string_view> {
    public:
      template<typename Iter, typename Size>
      std::string_view operator ()(Iter &begin, Iter end, Size len) {
        if(std::distance(begin, end) < static_cast<std::ptrdiff_t>(len))
          throw std::invalid_argument("unexpected end of string");

        std::string_view value(&*begin, len);
        std::advance(begin, len);
        return value;
      }
    };

    template<typename String, typename Iter>
    String decode_str(Iter &begin, Iter end) {
      assert(std::isdigit(*begin));
      std::size_t len = decode_digits<std::size_t>(begin, end);
      if(begin == end)
        throw std::invalid_argument("unexpected end of string");
      if(*begin != ':')
        throw std::invalid_argument("expected ':'");
      ++begin;

      return str_reader<String>{}(begin, end, len);
    }

  }

  template<typename Data, typename Iter>
  Data basic_decode(Iter &begin, Iter end) {
    using Traits = variant_traits_for<Data>;
    using integer = typename Data::integer;
    using string  = typename Data::string;
    using list    = typename Data::list;
    using dict    = typename Data::dict;

    typename Data::string dict_key;
    Data result;
    std::stack<Data*> state;

    // There are three ways we can store an element we've just parsed:
    //   1) to the root node
    //   2) appended to the end of a list
    //   3) inserted into a dict
    // We then return a pointer to the thing we've just inserted, which lets
    // us add that pointer to our node stack. Since we only ever manipulate the
    // top element of the stack, this pointer should be valid for as long as we
    // hold onto it.
    auto store = [&result, &state, &dict_key](auto &&thing) -> Data * {
      if(state.empty()) {
        result = std::move(thing);
        return &result;
      } else if(auto p = Traits::template get_if<list>(state.top())) {
        p->push_back(std::move(thing));
        return &p->back();
      } else if(auto p = Traits::template get_if<dict>(state.top())) {
        auto i = p->emplace(std::move(dict_key), std::move(thing));
        if(!i.second) {
          throw std::invalid_argument(
            "duplicated key in dict: " + std::string(i.first->first)
          );
        }
        return &i.first->second;
      }
      assert(false && "expected list or dict");
      return nullptr;
    };

    do {
      if(begin == end)
        throw std::invalid_argument("unexpected end of string");

      if(*begin == 'e') {
        if(!state.empty()) {
          ++begin;
          state.pop();
        } else {
          throw std::invalid_argument("unexpected e");
        }
      } else {
        if(!state.empty() && Traits::index(*state.top()) == 3 /* dict */) {
          if(!std::isdigit(*begin))
            throw std::invalid_argument("expected string token");
          dict_key = detail::decode_str<string>(begin, end);
          if(begin == end)
            throw std::invalid_argument("unexpected end of string");
        }

        if(*begin == 'i') {
          store(detail::decode_int<integer>(begin, end));
        } else if(*begin == 'l') {
          ++begin;
          state.push(store( list{} ));
        } else if(*begin == 'd') {
          ++begin;
          state.push(store( dict{} ));
        } else if(std::isdigit(*begin)) {
          store(detail::decode_str<string>(begin, end));
        } else {
          throw std::invalid_argument("unexpected type");
        }
      }
    } while(!state.empty());

    return result;
  }

  template<typename Data, typename Iter>
  inline Data basic_decode(const Iter &begin, Iter end) {
    Iter b(begin);
    return basic_decode<Data>(b, end);
  }

  template<typename Data>
  inline Data basic_decode(const string_view &s) {
    return basic_decode<Data>(s.begin(), s.end());
  }

  template<typename Data>
  Data basic_decode(std::istream &s, eof_behavior e = check_eof) {
    static_assert(!std::is_same_v<typename Data::string, std::string_view>,
                  "reading from stream not supported for data views");

    std::istreambuf_iterator<char> begin(s), end;
    auto result = basic_decode<Data>(begin, end);
    // If we hit EOF, update the parent stream.
    if(e == check_eof && begin == end)
      s.setstate(std::ios_base::eofbit);
    return result;
  }

  template<typename T>
  inline data decode(T &begin, T end) {
    return basic_decode<data>(begin, end);
  }

  template<typename T>
  inline data decode(const T &begin, T end) {
    return basic_decode<data>(begin, end);
  }

  inline data decode(const string_view &s) {
    return basic_decode<data>(s.begin(), s.end());
  }

  inline data decode(std::istream &s, eof_behavior e = check_eof) {
    return basic_decode<data>(s, e);
  }

  template<typename T>
  inline data_view decode_view(T &begin, T end) {
    return basic_decode<data_view>(begin, end);
  }

  template<typename T>
  inline data_view decode_view(const T &begin, T end) {
    return basic_decode<data_view>(begin, end);
  }

  inline data_view decode_view(const string_view &s) {
    return basic_decode<data_view>(s.begin(), s.end());
  }

#ifdef BENCODE_HAS_BOOST
  template<typename T>
  inline boost_data boost_decode(T &begin, T end) {
    return basic_decode<boost_data>(begin, end);
  }

  template<typename T>
  inline boost_data boost_decode(const T &begin, T end) {
    return basic_decode<boost_data>(begin, end);
  }

  inline boost_data boost_decode(const string_view &s) {
    return basic_decode<boost_data>(s.begin(), s.end());
  }

  inline boost_data boost_decode(std::istream &s, eof_behavior e = check_eof) {
    return basic_decode<boost_data>(s, e);
  }

  template<typename T>
  inline boost_data_view boost_decode_view(T &begin, T end) {
    return basic_decode<boost_data_view>(begin, end);
  }

  template<typename T>
  inline boost_data_view boost_decode_view(const T &begin, T end) {
    return basic_decode<boost_data_view>(begin, end);
  }

  inline boost_data_view boost_decode_view(const string_view &s) {
    return basic_decode<boost_data_view>(s.begin(), s.end());
  }
#endif

  namespace detail {
    class list_encoder {
    public:
      inline list_encoder(std::ostream &os) : os(os) {
        os.put('l');
      }

      inline ~list_encoder() {
        os.put('e');
      }

      template<typename T>
      inline list_encoder & add(T &&value);
    private:
      std::ostream &os;
    };

    class dict_encoder {
    public:
      inline dict_encoder(std::ostream &os) : os(os) {
        os.put('d');
      }

      inline ~dict_encoder() {
        os.put('e');
      }

      template<typename T>
      inline dict_encoder & add(const string_view &key, T &&value);
    private:
      std::ostream &os;
    };

    template<typename T>
    void write_integer(std::ostream &os, T value) {
#ifdef BENCODE_HAS_CHARCONV
      // digits10 tells how many base-10 digits can fully fit in T, so we add 1
      // for the digit that can only partially fit, plus one more for the
      // negative sign.
      char buf[std::numeric_limits<T>::digits10 + 2];
      auto r = std::to_chars(buf, buf + sizeof(buf), value);
      if(r.ec != std::errc())
        throw std::invalid_argument("failed to write integer value");
      os.write(buf, r.ptr - buf);
#else
      auto s = std::to_string(value);
      os.write(s.c_str(), s.size());
#endif
    }
  }

  inline void encode(std::ostream &os, integer value) {
    os.put('i');
    detail::write_integer(os, value);
    os.put('e');
  }

  inline void encode(std::ostream &os, const string_view &value) {
    detail::write_integer(os, value.size());
    os.put(':');
    os.write(value.data(), value.size());
  }

  template<typename T>
  void encode(std::ostream &os, const std::vector<T> &value) {
    detail::list_encoder e(os);
    for(auto &&i : value)
      e.add(i);
  }

  template<typename T>
  void encode(std::ostream &os, const std::map<string, T> &value) {
    detail::dict_encoder e(os);
    for(auto &&i : value)
      e.add(i.first, i.second);
  }

  template<typename T>
  void encode(std::ostream &os, const std::map<string_view, T> &value) {
    detail::dict_encoder e(os);
    for(auto &&i : value)
      e.add(i.first, i.second);
  }

  template<typename K, typename V>
  void encode(std::ostream &os, const map_proxy<K, V> &value) {
    encode(os, *value);
  }

  namespace detail {
    class encode_visitor {
    public:
      inline encode_visitor(std::ostream &os) : os(os) {}

      template<typename T>
      void operator ()(T &&operand) const {
        encode(os, std::forward<T>(operand));
      }
    private:
      std::ostream &os;
    };
  }

  template<template<typename ...> typename Variant, typename I, typename S,
           template<typename ...> typename L, template<typename ...> typename D>
  void encode(std::ostream &os, const basic_data<Variant, I, S, L, D> &value) {
    variant_traits<Variant>::visit(detail::encode_visitor(os), value);
  }

  namespace detail {
    template<typename T>
    inline list_encoder & list_encoder::add(T &&value) {
      encode(os, std::forward<T>(value));
      return *this;
    }

    template<typename T>
    inline dict_encoder &
    dict_encoder::add(const string_view &key, T &&value) {
      encode(os, key);
      encode(os, std::forward<T>(value));
      return *this;
    }
  }

  template<typename T>
  std::string encode(T &&t) {
    std::stringstream ss;
    encode(ss, std::forward<T>(t));
    return ss.str();
  }

}

#endif
