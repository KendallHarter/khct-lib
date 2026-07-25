module;

#include <algorithm>
#include <flat_map>
#include <flat_set>
#include <meta>
#include <ranges>
#include <utility>

export module khct:named_variant;

import :helpers;

namespace khct {

template<std::meta::info Enumerator>
[[nodiscard]] consteval auto is_valid_for_named_enum() -> bool
{
   const auto enums = std::meta::enumerators_of(Enumerator);

   if (!std::meta::is_complete_type(Enumerator)) {
      throw std::meta::exception{"NamedVariant must be passed a complete type", Enumerator};
   }

   if (!std::meta::is_enum_type(Enumerator)) {
      throw std::meta::exception{"NamedVariant must be passed an enumerator", Enumerator};
   }

   std::flat_set<std::underlying_type_t<typename[:Enumerator:]>> seen_values;
   for (const auto& enum_val : enums) {
      const auto [_, inserted] = seen_values.insert(extract_enum_value<typename[:Enumerator:]>(enum_val));
      if (!inserted) {
         throw std::meta::exception{"NamedVariants cannot have repeated values", Enumerator};
      }
   }

   for (const auto& enum_val : enums) {
      bool type_anno_has_been_seen = false;
      for (const auto& annotation : std::meta::annotations_of(enum_val)) {
         const auto anno_type = std::meta::type_of(annotation);
         if (std::meta::has_template_arguments(anno_type) && std::meta::template_of(anno_type) == ^^TypeStruct) {
            if (type_anno_has_been_seen) {
               throw std::meta::exception{
                  "NamedVariant enumerators can only have one or zero type annotations", Enumerator};
            }
            type_anno_has_been_seen = true;
         }
      }
   }

   return true;
}

export template<typename Info>
concept IsValidForNamedVariant = is_valid_for_named_enum<^^Info>();

export template<typename Enum>
   requires IsValidForNamedVariant<Enum>
struct NamedVariant {
private:
   using Self = NamedVariant;

   static constexpr auto enumerators = std::define_static_array(std::meta::enumerators_of(^^Enum));

   // Dummy struct so that UnionStorage is always default initializable
   struct Dummy {};
   union UnionStorage;

   static constexpr auto union_field_args = [] {
      std::vector<std::pair<std::vector<std::meta::info>, std::string_view>> union_fields;
      for (const auto& entry : enumerators) {
         std::vector<std::meta::info> union_fields_to_add{};
         for (const auto& annotation : std::meta::annotations_of(entry)) {
            const auto anno_type = std::meta::type_of(annotation);
            if (std::meta::has_template_arguments(anno_type) && std::meta::template_of(anno_type) == ^^TypeStruct) {
               union_fields_to_add = std::meta::template_arguments_of(anno_type);
               break;
            }
         }
         union_fields.push_back(std::pair{union_fields_to_add, std::meta::identifier_of(entry)});
      }
      return std::define_static_array(union_fields | std::views::transform([](const auto& x) {
                                         const auto storage = std::define_static_array(x.first);
                                         return Tuple{
                                            storage.data(), storage.size(), std::define_static_string(x.second)};
                                      }));
   }();

   static consteval auto calc_tag_base(const Enum Value) -> std::meta::info
   {
      for (const auto& [enum_entry, union_info] : std::views::zip(enumerators, union_field_args)) {
         if (Value == std::meta::extract<Enum>(std::meta::constant_of(enum_entry))) {
            const auto [ptr, len, _] = union_info;
            return std::meta::substitute(^^Tuple, std::span{ptr, len});
         }
      }
      std::unreachable();
   }

public:
   // This is internal to the enum so that each NamedVariant has its own tag type
   // This prevents passing tag types from other NamedVariant instances
   // clang-format off
   template<Enum Value>
   struct Tag : [:calc_tag_base(Value):] {};
   // clang-format on

   consteval
   {
      std::vector storage_args{std::meta::data_member_spec(^^Dummy, {.name = "_"})};
      for (const auto& enum_info : std::meta::enumerators_of(^^Enum)) {
         storage_args.push_back(
            std::meta::data_member_spec(
               std::meta::substitute(^^Tag, {std::meta::constant_of(enum_info)}),
               {.name = std::meta::identifier_of(enum_info)}));
      }
      std::meta::define_aggregate(^^UnionStorage, storage_args);
   }

private:
   struct TypeInfo {
      std::meta::info union_member;
      std::meta::info union_type;
   };

   // Have to kinda iterate twice to solve this, unfortunately
   static constexpr auto enum_value_to_info_array = [] {
      std::flat_map<std::underlying_type_t<Enum>, TypeInfo> value_to_tuple_type;
      // drop the first element as it is always the Dummy field
      const auto union_fields
         = std::meta::nonstatic_data_members_of(^^UnionStorage, std::meta::access_context::current())
         | std::views::drop(1) | std::ranges::to<std::vector>();
      for (const auto& [enum_entry, union_field] : std::views::zip(enumerators, union_fields)) {
         const auto union_type = std::meta::type_of(union_field);
         value_to_tuple_type.insert(
            {extract_enum_value<Enum>(enum_entry),
             {
                .union_member = union_field,
                .union_type = union_type,
             }});
      }
      return std::define_static_array(value_to_tuple_type);
   }();

   static constexpr auto union_members = std::define_static_array(
      std::meta::nonstatic_data_members_of(^^UnionStorage, std::meta::access_context::current()));

   static constexpr auto union_types
      = std::define_static_array(union_members | std::views::transform(std::meta::type_of));

   [[nodiscard]] static consteval auto enum_value_is_named(const Enum enum_value) -> bool
   {
      return std::ranges::contains(
         enum_value_to_info_array, std::to_underlying(enum_value), [&](const auto& x) { return x.first; });
   }

   [[nodiscard]] static consteval auto enum_val_to_info(const Enum enum_value) -> TypeInfo
   {
      return std::ranges::find(
                enum_value_to_info_array, std::to_underlying(enum_value), [&](const auto& x) { return x.first; })
         ->second;
   }

   template<typename SelfT>
   constexpr void assign_union_to(this SelfT&& self, Self& other)
   {
      template for (constexpr auto enumer : enumerators)
      {
         static constexpr auto compile_value = extract_enum_value<Enum>(enumer);
         if (self.value_ == compile_value) {
            static constexpr auto enum_val = Enum{compile_value};
            // Use construct_at instead of assigning to the member because stateless members
            // in GCC don't get their lifetimes properly started when doing that,
            // but construct_at seems to do the job
            std::construct_at(
               &other.storage_.[:enum_val_to_info(enum_val)
                                    .union_member:],
                                                   std::forward_like<SelfT>(
                                                      self.storage_.[:enum_val_to_info(enum_val).union_member:]));
            return;
         }
      }
      std::unreachable();
   }

   [[no_unique_address]] UnionStorage storage_ = UnionStorage{Dummy{}};
   std::underlying_type_t<Enum> value_;

public:
   template<Enum Value>
   constexpr explicit(false) NamedVariant(const Tag<Value>& val) noexcept
      : storage_{Dummy{}}, value_{std::to_underlying(Value)}
   { std::construct_at(&storage_.[:enum_val_to_info(Value).union_member:], val); }

   template<Enum Value>
   constexpr explicit(false) NamedVariant(Tag<Value>&& val) noexcept
      : storage_{Dummy{}}, value_{std::to_underlying(Value)}
   { std::construct_at(&storage_.[:enum_val_to_info(Value).union_member:], std::move(val)); }

   template<Enum Value>
      requires(enum_value_is_named(Value))
   [[nodiscard]] constexpr auto holds_tag(this const Self& self) noexcept -> bool
   { return self.value_ == std::to_underlying(Value); }

   NamedVariant() = delete;

   constexpr NamedVariant(const NamedVariant& rhs) noexcept
      requires(all_satisfy_concept(union_types, ^^std::is_nothrow_copy_constructible_v))
      : value_{rhs.value_}
   { rhs.assign_union_to(*this); }

   constexpr NamedVariant(NamedVariant&& rhs) noexcept
      requires(all_satisfy_concept(union_types, ^^std::is_nothrow_move_constructible_v))
      : value_{rhs.value_}
   { std::move(rhs).assign_union_to(*this); }

   auto operator=(const NamedVariant&) -> NamedVariant& = default;
   auto operator=(NamedVariant&&) -> NamedVariant& = default;

   ~NamedVariant()
      requires(all_satisfy_concept(union_types, ^^std::is_trivially_destructible_v))
   = default;

   constexpr ~NamedVariant()
      requires(!all_satisfy_concept(union_types, ^^std::is_trivially_destructible_v))
   {
      template for (constexpr auto enumer : enumerators)
      {
         static constexpr auto compile_value = extract_enum_value<Enum>(enumer);
         if (value_ == compile_value) {
            static constexpr auto enum_value = Enum{compile_value};
            std::destroy_at(&storage_.[:enum_val_to_info(enum_value).union_member:]);
            return;
         }
      }
      std::unreachable();
   }

   // TODO: Conditionally support == and <=>

   // TODO: Constrain this
   //       Add noexcept specification
   template<typename SelfT, typename Visitor>
   constexpr auto visit(this SelfT&& self, Visitor&& visitor) -> decltype(auto)
   {
      template for (constexpr auto enumer : enumerators)
      {
         static constexpr auto compile_value = extract_enum_value<Enum>(enumer);
         if (self.value_ == compile_value) {
            static constexpr auto enum_value = Enum{compile_value};
            return std::forward<Visitor>(visitor)(self.storage_.[:enum_val_to_info(enum_value).union_member:]);
         }
      }
      std::unreachable();
   }

   // Disallow certain things because of compiler limitations
   static_assert(
      all_satisfy_concept(union_types, ^^std::is_trivially_destructible_v),
      "All types must be trivially destructible until trivial unions are implemented");
};

template<auto>
struct TagAliasImpl;

template<typename Enum, Enum Value>
   requires(std::is_enum_v<Enum>)
struct TagAliasImpl<Value> {
   using type = NamedVariant<Enum>::template Tag<Value>;
};

export template<auto Value>
using Tag = TagAliasImpl<Value>::type;

} // namespace khct
