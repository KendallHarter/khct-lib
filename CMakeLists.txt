cmake_minimum_required(VERSION 4.0)

project(khct_lib)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS OFF)

option(KHCT_BUILD_TESTS "If tests should be built or not" FALSE)

add_library(khct)
add_library(khct::khct ALIAS khct)

# Add -freflection if GCC
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
   # -fabi-version is needed to fix some linking errors
   # hopefully remove it someday
   target_compile_options(khct PUBLIC -freflection -fabi-version=20)
endif()

target_sources(
   khct
      PUBLIC
         FILE_SET CXX_MODULES
         FILES
            src/main_module.cppm
            src/submodules/strong_enum.cppm
            src/submodules/helpers.cppm
)

if("${KHCT_BUILD_TESTS}")
   include(CTest)
   include(FetchContent)

   FetchContent_Declare(
      Catch2
      GIT_REPOSITORY https://github.com/catchorg/Catch2.git
      GIT_TAG        v3.10.0
   )

   FetchContent_MakeAvailable(Catch2)

   add_executable(strong_enum_tests "tests/strong_enum.cpp")
   target_link_libraries(strong_enum_tests PRIVATE khct Catch2::Catch2WithMain)
   add_test(NAME strong_enum_tests COMMAND strong_enum_tests)
endif()
