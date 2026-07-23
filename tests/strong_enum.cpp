#include <catch2/catch_test_macros.hpp>

#include <cassert>
#include <cstdio>

import khct;

namespace {

enum struct TestA {
   value1,
   value2
};

} // namespace

auto main() -> int
{
   khct::StrongEnum<TestA> strong1{khct::StrongEnum<TestA>::tags.value1};
   strong1.visit(
      khct::OverloadSet{
         [](khct::StrongEnumTag<TestA, TestA::value1>) { std::puts("value1"); },
         [](khct::StrongEnumTag<TestA, TestA::value2>) { std::puts("value2"); }});
}
