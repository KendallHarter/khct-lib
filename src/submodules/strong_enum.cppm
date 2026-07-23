module;

#include <algorithm>
#include <flat_set>
#include <meta>
#include <ranges>
#include <tuple>
#include <utility>

export module khct:strong_enum;

import :helpers;

namespace khct {

template<typename Enumerator>
[[nodiscard]] consteval auto extract_enum_value(const std::meta::info enum_val) -> std::underlying_type_t<Enumerator>
{ return std::to_underlying(std::meta::extract<Enumerator>(std::meta::constant_of(enum_val))); }

template<std::meta::info Enumerator>
[[nodiscard]] consteval auto is_valid_for_strong_enum() -> bool
{
   if (!std::meta::is_complete_type(Enumerator)) {
      throw std::meta::exception{"StrongEnum must be passed a complete type", Enumerator};
   }

   if (!std::meta::is_enum_type(Enumerator)) {
      throw std::meta::exception{"StrongEnum must be passed an enumerator", Enumerator};
   }

   std::flat_set<std::underlying_type_t<typename[:Enumerator:]>> seen_values;
   for (const auto& enum_val : std::meta::enumerators_of(Enumerator)) {
      const auto [_, inserted] = seen_values.insert(extract_enum_value<typename[:Enumerator:]>(enum_val));
      if (!inserted) {
         throw std::meta::exception{"StrongEnums cannot have repeated values", Enumerator};
      }
   }

   return true;
}

export template<typename Info>
concept IsValidForStrongEnum = is_valid_for_strong_enum<^^Info>();

export template<typename Enum>
   requires IsValidForStrongEnum<Enum>
struct StrongEnum {
   // This is internal to the enum so that each StrongEnum has its own tag type
   // This prevents passing tag types from other StrongEnum instances
   template<std::underlying_type_t<Enum>>
   struct Tag {};

private:
   using Self = StrongEnum;

   static constexpr auto enumerators = std::define_static_array(std::meta::enumerators_of(^^Enum));

   struct TagsStruct;
   consteval
   {
      std::vector<std::meta::info> tags_struct_fields;
      std::vector<std::meta::info> tuple_fields;
      for (const auto& entry : enumerators) {
         tags_struct_fields.push_back(
            std::meta::data_member_spec(
               std::meta::substitute(^^Tag, {std::meta::reflect_constant(extract_enum_value<Enum>(entry))}),
               {.name = std::meta::identifier_of(entry)}));
      }
      std::meta::define_aggregate(^^TagsStruct, tags_struct_fields);
   }

   std::underlying_type_t<Enum> value_;

public:
   static constexpr TagsStruct tags{};

   template<std::underlying_type_t<Enum> Value>
   constexpr explicit StrongEnum(Tag<Value>) noexcept : value_{Value}
   {}

   StrongEnum() = delete;
   StrongEnum(const StrongEnum&) = default;
   StrongEnum(StrongEnum&&) = default;
   auto operator=(const StrongEnum&) -> StrongEnum& = default;
   auto operator=(StrongEnum&&) -> StrongEnum& = default;

   friend auto operator==(const StrongEnum&, const StrongEnum&) -> bool = default;

   template<std::underlying_type_t<Enum> Value>
   friend constexpr auto operator==(const StrongEnum& lhs, Tag<Value>) noexcept -> bool
   { return lhs.value_ == Value; }

   template<std::underlying_type_t<Enum> Value>
   friend constexpr auto operator==(Tag<Value>, const StrongEnum& rhs) noexcept -> bool
   { return rhs.value_ == Value; }

   // TODO: Do we really want to expose this?
   //       Maybe make it configurable?
   // friend auto operator<=>(const StrongEnum&, const StrongEnum&) -> auto = default;
   //
   // template<std::underlying_type_t<Enum> Value>
   // friend constexpr auto operator<=>(const StrongEnum& lhs, Tag<Value>) noexcept -> auto
   // {
   //    return lhs.value_ <=> Value;
   // }
   //
   // template<std::underlying_type_t<Enum> Value>
   // friend constexpr auto operator<=>(Tag<Value>, const StrongEnum& rhs) noexcept -> auto
   // {
   //    return rhs.value_ <=> Value;
   // }

   // TODO: Constrain this
   //       Add noexcept specification
   template<typename Visitor>
   constexpr auto visit(this const Self& self, Visitor&& visitor) -> decltype(auto)
   {
      template for (constexpr auto enumer : enumerators)
      {
         static constexpr auto compile_value = extract_enum_value<Enum>(enumer);
         if (self.value_ == compile_value) {
            std::forward<Visitor>(visitor)(Tag<compile_value>{});
            return;
         }
      }
      std::unreachable();
   }
};

template<auto>
struct TagImpl;

template<typename Enum, Enum Value>
   requires(std::is_enum_v<Enum>)
struct TagImpl<Value> {
   using type = StrongEnum<Enum>::template Tag<std::to_underlying(Value)>;
};

export template<auto Value>
using Tag = TagImpl<Value>::type;

} // namespace khct
