cmake_minimum_required(VERSION 3.13)
project(ambilight_cpp LANGUAGES CXX)

# Options
option(BUILD_TESTS "Build tests" ON)

add_compile_options(-Wall -Wextra -Wpedantic)
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  add_compile_options(-O3)
else()
  add_compile_options(-O0 -g)
endif()

# include dirs
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# sources
set(SRC
  src/main.cpp
  src/led_driver.cpp
  src/spi_helper.cpp
)

add_library(ledcore STATIC ${SRC})
target_include_directories(ledcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# create executable for daemon
add_executable(led_daemon src/main.cpp src/led_driver.cpp src/spi_helper.cpp)
target_link_libraries(led_daemon PRIVATE pthread)  # threads
# no special libs needed for spidev (we use open/ioctl)

# install target
install(TARGETS led_daemon DESTINATION bin)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/ DESTINATION include/ambilight)

# tests (optional)
if(BUILD_TESTS)
  enable_testing()
  add_executable(test_led_driver tests/test_led_driver.cpp src/led_driver.cpp src/spi_helper.cpp)
  add_test(NAME LedDriverTest COMMAND test_led_driver)
endif()
