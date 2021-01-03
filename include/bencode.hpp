#ifndef INC_BENCODE_HPP
#define INC_BENCODE_HPP

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstddef>
#include <iterator>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#ifdef __has_include
#  if __has_include(<boost/variant.hpp>)
#    include <boost/variant.hpp>
#    define BENCODE_HAS_BOOST
#  endif
#endif

namespace bencode {

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

    base_type & base() { return *this; }
    const base_type & base() const { return *this; }
  };

  template<template<typename ...> typename T>
  struct variant_traits {
    template<typename Visitor, typename ...Variants>
    inline static void call_visit(Visitor &&visitor, Variants &&...variants) {
      visit(std::forward<Visitor>(visitor),
            std::forward<Variants>(variants)...);
    }
  };

  using data = basic_data<std::variant, long long, std::string, std::vector,
                          map_proxy>;
  using data_view = basic_data<std::variant, long long, std::string_view,
                               std::vector, map_proxy>;

#ifdef BENCODE_HAS_BOOST
  using boost_data = bencode::basic_data<
    boost::variant, long long, std::string, std::vector, bencode::map_proxy
  >;
  using boost_data_view = bencode::basic_data<
    boost::variant, long long, std::string_view, std::vector, bencode::map_proxy
  >;

  template<>
  struct variant_traits<boost::variant> {
    template<typename Visitor, typename ...Variants>
    static void call_visit(Visitor &&visitor, Variants &&...variants) {
      boost::apply_visitor(std::forward<Visitor>(visitor),
                           std::forward<Variants>(variants)...);
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

    template<typename Data, typename Iter>
    typename Data::integer decode_int(Iter &begin, Iter end) {
      assert(*begin == 'i');
      ++begin;

      typename Data::integer positive = 1;
      if(*begin == '-') {
        positive = -1;
        ++begin;
      }

      // TODO: handle overflow
      typename Data::integer value = 0;
      while(begin != end && std::isdigit(*begin))
        value = value * 10 + *begin++ - '0';
      if(begin == end)
        throw std::invalid_argument("unexpected end of string");
      if(*begin != 'e')
        throw std::invalid_argument("expected 'e'");

      ++begin;
      return positive * value;
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

    template<typename Data, typename Iter>
    typename Data::string decode_str(Iter &begin, Iter end) {
      assert(std::isdigit(*begin));
      std::size_t len = 0;
      while(begin != end && std::isdigit(*begin))
        len = len * 10 + *begin++ - '0';

      if(begin == end)
        throw std::invalid_argument("unexpected end of string");
      if(*begin != ':')
        throw std::invalid_argument("expected ':'");
      ++begin;

      return str_reader<typename Data::string>{}(begin, end, len);
    }

    template<typename Data, typename Iter>
    typename Data::list decode_list(Iter &begin, Iter end) {
      assert(*begin == 'l');
      ++begin;

      typename Data::list value;
      while(begin != end && *begin != 'e') {
        value.push_back(basic_decode<Data>(begin, end));
      }

      if(begin == end)
        throw std::invalid_argument("unexpected end of string");

      ++begin;
      return value;
    }

    template<typename Data, typename Iter>
    typename Data::dict decode_dict(Iter &begin, Iter end) {
      assert(*begin == 'd');
      ++begin;

      typename Data::dict value;
      while(begin != end && *begin != 'e') {
        if(!std::isdigit(*begin))
          throw std::invalid_argument("expected string token");

        auto k = decode_str<Data>(begin, end);
        auto v = basic_decode<Data>(begin, end);
        auto result = value.emplace(std::move(k), std::move(v));
        if(!result.second) {
          throw std::invalid_argument(
            "duplicated key in dict: " + std::string(result.first->first)
          );
        }
      }

      if(begin == end)
        throw std::invalid_argument("unexpected end of string");

      ++begin;
      return value;
    }

  }

  template<typename Data, typename Iter>
  Data basic_decode(Iter &begin, Iter end) {
    if(begin == end)
      throw std::invalid_argument("unexpected end of string");

    if(*begin == 'i')
      return detail::decode_int<Data>(begin, end);
    else if(*begin == 'l')
      return detail::decode_list<Data>(begin, end);
    else if(*begin == 'd')
      return detail::decode_dict<Data>(begin, end);
    else if(std::isdigit(*begin))
      return detail::decode_str<Data>(begin, end);

    throw std::invalid_argument("unexpected type");
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
  }

  // TODO: make these use unformatted output?
  inline void encode(std::ostream &os, integer value) {
    os << "i" << value << "e";
  }

  inline void encode(std::ostream &os, const string_view &value) {
    os << value.size() << ":" << value;
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
    variant_traits<Variant>::call_visit(detail::encode_visitor(os),
                                        value.base());
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
