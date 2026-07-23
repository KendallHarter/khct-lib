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
         [](khct::Tag<TestA::value1>) { std::puts("value1"); }, [](khct::Tag<TestA::value2>) { std::puts("value2"); }});
}
