module;

#include <concepts>
#include <flat_set>
#include <meta>
#include <random>
#include <tuple>
#include <utility>

export module khct:helpers;

namespace khct {

template<typename Enumerator>
[[nodiscard]] consteval auto extract_enum_value(const std::meta::info enum_val) -> std::underlying_type_t<Enumerator>
{ return std::to_underlying(std::meta::extract<Enumerator>(std::meta::constant_of(enum_val))); }

export template<std::equality_comparable... T>
struct AnyOf {
   [[nodiscard]] constexpr explicit AnyOf(T... vals) noexcept : vals_{vals...} {}

   [[nodiscard]] friend auto operator==(const AnyOf& lhs, const auto& val) -> bool
      requires(std::equality_comparable_with<T, decltype(val)> && ...)
   {
      template for (const auto& v : lhs.vals_)
      {
         if (v == val) {
            return true;
         }
      }
      return false;
   }

   [[nodiscard]] friend auto operator==(const auto& val, const AnyOf& rhs) -> bool
      requires(std::equality_comparable_with<T, decltype(val)> && ...)
   { return rhs == val; }

private:
   std::tuple<T...> vals_;
};

export template<typename... Ts>
struct OverloadSet : Ts... {
   using Ts::operator()...;
};

export template<typename T, typename Prng>
   requires std::is_enum_v<T> && std::uniform_random_bit_generator<std::remove_cvref_t<Prng>>
[[nodiscard]] constexpr auto generate_equally_weighted_enum_value(Prng&& prng) noexcept(noexcept(prng())) -> T
{
   // Gather up all the values and then pick one among them
   static constexpr auto enum_values = [] {
      std::flat_set<std::underlying_type_t<T>> values;
      for (const auto& enum_val : std::meta::enumerators_of(^^T)) {
         values.insert(extract_enum_value<T>(enum_val));
      }
      return std::define_static_array(values);
   }();

   return static_cast<T>(enum_values[std::uniform_int_distribution<std::size_t>{0, enum_values.size() - 1}(prng)]);
}

template<std::ranges::input_range R>
   requires std::same_as<std::remove_cvref_t<std::ranges::range_value_t<R>>, std::meta::info>
[[nodiscard]] consteval auto all_satisfy_concept(R&& range, const std::meta::info to_fulfill) -> bool
{
   for (const auto& info : range) {
      const auto sub = std::meta::substitute(to_fulfill, {info});
      if (!std::meta::extract<bool>(sub)) {
         return false;
      }
   }

   return true;
}

template<std::ranges::input_range R, typename T>
   requires std::same_as<std::remove_cvref_t<std::ranges::range_value_t<R>>, std::meta::info>
[[nodiscard]] consteval auto all_satisfy_partial_concept(R&& range, T) -> bool
{
   for (const auto& info : range) {
      const auto sub = std::meta::substitute(^^T::operator(), {info});
      const auto func = std::meta::extract<bool (*)()>(sub);
      if (!func()) {
         return false;
      }
   }

   return true;
}

// GCC doesn't support concept parameters yet so pass a reflection of one instead
template<std::meta::info Concept, typename... Ts>
   requires(std::meta::is_concept(Concept))
constexpr auto partial_concept
   = []<typename... Us> static { return std::meta::extract<bool>(std::meta::substitute(Concept, {^^Ts..., ^^Us...})); };

// Use an external base so structured bindings work
template<typename... Ts>
struct MakeTupleBase {
   struct Base;
   consteval
   {
      []<std::size_t... Is>(std::index_sequence<Is...>) {
         std::meta::define_aggregate(
            ^^Base, {std::meta::data_member_spec(^^Ts, {.name = "_", .no_unique_address = true})...});
      }(std::index_sequence_for<Ts...>{});
   }
};

export template<typename... Ts>
struct Tuple : MakeTupleBase<Ts...>::Base {
private:
   using Base = MakeTupleBase<Ts...>::Base;
   static constexpr auto fields
      = std::define_static_array(std::meta::members_of(^^Base, std::meta::access_context::current()));

public:
   template<std::size_t I, typename SelfT>
      requires(I < sizeof...(Ts))
   constexpr auto get(this SelfT&& self) -> decltype(auto)
   { return std::forward_like<SelfT>(self.[:fields[I]:]); }
};

export template<typename... Ts>
Tuple(Ts...) -> Tuple<std::remove_cvref_t<Ts>...>;

export template<typename... Ts, typename... Us>
   requires(sizeof...(Ts) == sizeof...(Us)) && (std::equality_comparable_with<Ts, Us> && ...)
constexpr auto operator==(const Tuple<Ts...>& lhs, const Tuple<Us...>& rhs) noexcept(
   noexcept(((std::declval<Ts>() == std::declval<Us>()) && ...))) -> bool
{
   return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
      return ((lhs.template get<Is>() == rhs.template get<Is>()) && ...);
   }(std::index_sequence_for<Ts...>{});
}

export template<typename... Ts, typename... Us>
   requires(sizeof...(Ts) == sizeof...(Us)) && (std::three_way_comparable_with<Ts, Us> && ...)
constexpr auto operator<=>(const Tuple<Ts...>& lhs, const Tuple<Us...>& rhs) noexcept(
   noexcept(((std::declval<Ts>() <=> std::declval<Us>()) && ...)))
   -> std::common_comparison_category_t<decltype(std::declval<Ts>() <=> std::declval<Us>())...>
{
   if constexpr (sizeof...(Ts) == 0) {
      return std::strong_ordering::equal;
   }
   else {
      static constexpr auto [... Is] = std::index_sequence_for<Ts...>{};
      for (constexpr auto I : std::array{Is...}) {
         const auto val = lhs.template get<I>() <=> rhs.template get<I>();
         if (val != 0) {
            return val;
         }
      }
      return std::strong_ordering::equal;
   }
}

} // namespace khct
