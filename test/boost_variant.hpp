#ifndef INC_TEST_BOOST_VARIANT_HPP
#define INC_TEST_BOOST_VARIANT_HPP

#include "bencode.hpp"
#include <boost/variant.hpp>

using bdata = bencode::basic_data<
  boost::variant, long long, std::string, std::vector, bencode::map_proxy
>;
using bdata_view = bencode::basic_data<
  boost::variant, long long, std::string_view, std::vector, bencode::map_proxy
>;

template<>
struct bencode::variant_traits<boost::variant> {
  template<typename Visitor, typename ...Variants>
  static void call_visit(Visitor &&visitor, Variants &&...variants) {
    boost::apply_visitor(std::forward<Visitor>(visitor),
                         std::forward<Variants>(variants)...);
  }
};

#endif
