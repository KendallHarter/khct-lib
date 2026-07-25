#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_all.hpp>

#include <charconv>
#include <flat_map>
#include <meta>
#include <string>

import khct;

namespace {

template<std::integral T>
constexpr auto to_str(const T value) noexcept -> std::string
{
   char buffer[32]{};
   std::to_chars(std::begin(buffer), std::end(buffer), value);
   return std::string{buffer};
}

} // namespace

TEST_CASE("NamedVariant is compile time compatible", "[named_variant]")
{
   enum struct TestA {
      value0,
      value2 = 2
   };

   static constexpr auto visitor
      = khct::OverloadSet{[](khct::Tag<TestA::value0>) { return 1; }, [](khct::Tag<TestA::value2>) { return 2; }};
   static constexpr khct::NamedVariant<TestA> strong1{khct::Tag<TestA::value0>{}};
   STATIC_REQUIRE(strong1 == strong1);
   STATIC_REQUIRE((strong1 <=> strong1) == 0);

   STATIC_REQUIRE(strong1.holds_tag<TestA::value0>());
   STATIC_REQUIRE(strong1.visit(visitor) == 1);

   static constexpr auto strong2 = strong1;
   STATIC_REQUIRE(strong2.holds_tag<TestA::value0>());
   STATIC_REQUIRE(strong2.visit(visitor) == 1);
}

TEST_CASE("Destructuring values works", "[named_variant]")
{
   enum struct TestB {
      value0[[= khct::type<int>]],
      value1[[= khct::type<int, int>]],
      value3[[= khct::type<>]] = 3,
   };

   static constexpr auto visitor = khct::OverloadSet{
      [](const khct::Tag<TestB::value0>& vals) {
         const auto& [x] = vals;
         return to_str(x);
      },
      [](const khct::Tag<TestB::value1>& vals) {
         const auto& [x1, x2] = vals;
         return to_str(x1 + x2);
      },
      [](khct::Tag<TestB::value3>) { return std::string{":O"}; }};

   static constexpr auto strong0 = khct::NamedVariant<TestB>{khct::Tag<TestB::value0>{0}};
   static constexpr auto strong1 = khct::NamedVariant<TestB>{khct::Tag<TestB::value1>{1, 2}};
   static constexpr auto strong3 = khct::NamedVariant<TestB>{khct::Tag<TestB::value3>{}};
   STATIC_REQUIRE(strong0.visit(visitor) == "0");
   STATIC_REQUIRE(strong1.visit(visitor) == "3");
   STATIC_REQUIRE(strong3.visit(visitor) == ":O");
}

TEST_CASE("Moving", "[named_variant]")
{
   struct MoveOnly {
      MoveOnly() = default;
      MoveOnly(const MoveOnly&) = delete;
      auto operator=(const MoveOnly&) -> MoveOnly& = delete;
      MoveOnly(MoveOnly&&) = default;
      auto operator=(MoveOnly&&) -> MoveOnly& = default;

      int some_val = 0;
   };

   enum struct MoveOnlyTest {
      val[[= khct::type<MoveOnly>]]
   };
   auto test = khct::NamedVariant<MoveOnlyTest>{khct::Tag<MoveOnlyTest::val>{}};
   auto test2 = std::move(test);
   test = khct::Tag<MoveOnlyTest::val>{};
   test2.visit([](khct::Tag<MoveOnlyTest::val>& v) {
      auto& [val] = v;
      val.some_val += 1;
   });
   test2.visit([](khct::Tag<MoveOnlyTest::val>& v) {
      auto& [val] = v;
      REQUIRE(val.some_val == 1);
   });
}

TEST_CASE("Various functions", "[named_variant]")
{
   enum struct Tester {
      val0,
      val1,
      val2,
   };

   auto test = khct::NamedVariant<Tester>{khct::Tag<Tester::val0>{}};
   REQUIRE(test.holds_tag<Tester::val0>());
   REQUIRE(test.held_tag_id() == Tester::val0);
   REQUIRE(test.get_if<Tester::val0>());
   REQUIRE(!test.get_if<Tester::val1>());
}

TEST_CASE("Runtime value construction and comparison", "[named_variant]")
{
   static constexpr auto num_iters = 100;

   enum struct Something : std::size_t {
      val0,
      val1[[= khct::type<int>]],
      val2[[= khct::type<double>]]
   };

   static constexpr auto num_enumerators = std::meta::enumerators_of(^^Something).size();
   static constexpr auto enum_to_value_array = [] {
      std::flat_map<Something, std::underlying_type_t<Something>> values;
      for (const auto& enumer : std::meta::enumerators_of(^^Something)) {
         const auto value = std::meta::extract<Something>(std::meta::constant_of(enumer));
         values.emplace(value, std::to_underlying(value));
      }

      return std::define_static_array(values);
   }();

   static constexpr auto enum_to_value = [](const Something val) consteval {
      return std::ranges::find(enum_to_value_array, val, [](const auto& x) { return x.first; })->second;
   };

   const auto gen_index = GENERATE(take(num_iters, random(0zu, num_enumerators - 1)));
   const auto potential_value = GENERATE(take(1, random(0.0, 100.0)));

   const auto value = [&] -> khct::NamedVariant<Something> {
      switch (gen_index) {
      case enum_to_value(Something::val0): return khct::Tag<Something::val0>{}; break;

      case enum_to_value(Something::val1): return khct::Tag<Something::val1>{static_cast<int>(potential_value)}; break;

      case enum_to_value(Something::val2): return khct::Tag<Something::val2>{potential_value * 2}; break;

      default:
         REQUIRE(false);
         return khct::Tag<Something::val0>{};
         break;
      }
   }();

   const auto visitor = khct::OverloadSet{
      [](khct::Tag<Something::val0>) { return true; },
      [&](khct::Tag<Something::val1> x) { return x.get<0>() == static_cast<int>(potential_value); },
      [&](khct::Tag<Something::val2> x) { return x.get<0>() == potential_value * 2; }};
   REQUIRE(value.held_tag_id() == Something{gen_index});
   REQUIRE(value.visit(visitor));
}
