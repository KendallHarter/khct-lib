#include <catch2/catch_test_macros.hpp>

#include <charconv>
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
   test2.visit([](khct::Tag<MoveOnlyTest::val>& v) {
      auto& [val] = v;
      val.some_val += 1;
   });
   test2.visit([](khct::Tag<MoveOnlyTest::val>& v) {
      auto& [val] = v;
      REQUIRE(val.some_val == 1);
   });
}
