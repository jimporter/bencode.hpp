#ifndef INC_BENCODE_TEST_DECODE_DECODE_HPP
#define INC_BENCODE_TEST_DECODE_DECODE_HPP

using bstring = std::basic_string<std::byte>;
using bistringstream = std::basic_istringstream<std::byte>;

template<typename T>
struct char_type;

template<typename Char>
struct char_type<Char *> { using type = std::remove_cv_t<Char>; };
template<typename Char>
struct char_type<std::basic_string<Char>> { using type = Char; };
template<typename Char>
struct char_type<std::basic_string_view<Char>> { using type = Char; };
template<typename Char>
struct char_type<std::vector<Char>> { using type = Char; };
template<typename Char>
struct char_type<std::basic_istringstream<Char>> { using type = Char; };

template<typename T>
T make_data(const char *data) {
  using Char = typename char_type<T>::type;
  if constexpr(std::is_constructible_v<T, const Char *>)
    return T((Char*)(data));
  else
    return T((Char*)data, (Char*)(data + std::strlen(data)));
}

template<typename DecodeArgs, typename DoDecode>
auto mix_decoder(DecodeArgs &&decode_args, DoDecode &&do_decode) {
  return [decode_args, do_decode](auto &&data) {
    return std::apply(
      do_decode, decode_args(std::forward<decltype(data)>(data))
    );
  };
}

auto decode_args = [](auto &&data) {
  return std::tuple<decltype(data)&>(data);
};
auto decode_iter_args = [](auto &&data) {
  return std::make_tuple(data.begin(), data.end());
};
auto decode_ptr_len_args = [](auto &&data) {
  return std::tuple<decltype(data)&, std::size_t>(
    data, bencode::detail::any_strlen(data)
  );
};

template<typename InType, typename Builder, typename Callable>
auto decode_tests(Builder &_, Callable &&decode) {
  using boost::get;
  using std::get;

  using OutType = decltype(decode(std::declval<InType>()));
  using Integer = typename OutType::integer;
  using String = typename OutType::string;
  using List = typename OutType::list;
  using Dict = typename OutType::dict;
  auto S = &make_data<String>;

  _.test("integer", [decode]() {
    auto pos = make_data<InType>("i42e");
    auto pos_value = decode(pos);
    expect(get<Integer>(pos_value), equal_to(42));

    auto neg = make_data<InType>("i-42e");
    auto neg_value = decode(neg);
    expect(get<Integer>(neg_value), equal_to(-42));
  });

  _.test("string", [decode, S]() {
    auto data = make_data<InType>("4:spam");
    // Silence GCC < 10.
    [[maybe_unused]] auto within_data_memory = within_memory(data);

    auto value = decode(data);
    auto str = get<String>(value);
    expect(str, equal_to(S("spam")));
    if constexpr(bencode::detail::is_view_v<String>) {
      expect(&*str.begin(), within_data_memory);
      expect(&*str.end(), within_data_memory);
    }
  });

  _.test("list", [decode]() {
    auto data = make_data<InType>("li42ee");
    auto value = decode(data);
    auto list = get<List>(value);
    expect(get<Integer>(list[0]), equal_to(42));
  });

  _.test("dict", [decode, S]() {
    auto data = make_data<InType>("d4:spami42ee");
    // Silence GCC < 10.
    [[maybe_unused]] auto within_data_memory = within_memory(data);

    auto value = decode(data);
    auto dict = get<Dict>(value);
    expect(get<Integer>(dict[S("spam")]), equal_to(42));

    auto str = dict.find(S("spam"))->first;
    expect(str, equal_to(S("spam")));
    if constexpr (bencode::detail::is_view_v<String>) {
      expect(&*str.begin(), within_data_memory);
      expect(&*str.end(), within_data_memory);
    }
  });

  _.test("nested", [decode, S]() {
    auto data = make_data<InType>(
      "d"
      "3:one" "i1e"
      "5:three" "l" "d" "3:bar" "i0e" "3:foo" "i0e" "e" "e"
      "3:two" "l" "i3e" "3:foo" "i4e" "e"
      "e"
    );
    auto value = decode(data);
    auto dict = get<Dict>(value);
    expect(get<Integer>(dict[S("one")]), equal_to(1));
    expect(get<String>(get<List>(dict[S("two")])[1]), equal_to(S("foo")));
    expect(get<Integer>(get<Dict>(get<List>(dict[S("three")])[0])[S("foo")]),
           equal_to(0));
  });
}

#endif
