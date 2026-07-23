module;

#include <concepts>
#include <tuple>

export module khct:helpers;

namespace khct {

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

} // namespace khct
