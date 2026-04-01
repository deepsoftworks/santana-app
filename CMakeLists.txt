cmake_minimum_required(VERSION 3.20)
project(santana VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
  ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI
  GIT_TAG        v5.0.0
)
FetchContent_MakeAvailable(ftxui)

FetchContent_Declare(
  cli11
  GIT_REPOSITORY https://github.com/CLIUtils/CLI11
  GIT_TAG        v2.4.1
)
FetchContent_MakeAvailable(cli11)

FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json
  GIT_TAG        v3.11.3
)
set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(nlohmann_json)

add_executable(santana
  src/main.cpp
  src/parser.cpp
  src/stdin_source.cpp
  src/renderer.cpp
  src/chart.cpp
)

target_include_directories(santana PRIVATE src)

target_link_libraries(santana PRIVATE
  ftxui::screen
  ftxui::dom
  ftxui::component
  CLI11::CLI11
  nlohmann_json::nlohmann_json
)

target_compile_options(santana PRIVATE -Wall -Wextra -Wpedantic)

install(TARGETS santana DESTINATION bin)
