FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
)

FetchContent_GetProperties(googletest)

if(NOT googletest_POPULATED)
  FetchContent_Populate(googletest)
  add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

target_link_libraries(clio_tests PUBLIC clio gmock_main)
target_include_directories(clio_tests PRIVATE unittests)

enable_testing()

include(GoogleTest)

#increase timeout for tests discovery to 10 seconds, by default it is 5s. As more unittests added, we start to hit this issue
#https://github.com/google/googletest/issues/3475
gtest_discover_tests(clio_tests DISCOVERY_TIMEOUT 10)
