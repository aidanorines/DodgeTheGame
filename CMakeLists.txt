cmake_minimum_required(VERSION 3.24)
project(Dodge LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# enet and doctest declare cmake_minimum_required below 3.5, which CMake 4 rejects without this.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

FetchContent_Declare(enet GIT_REPOSITORY https://github.com/lsalzman/enet.git GIT_TAG v1.3.18 GIT_SHALLOW TRUE)
FetchContent_Declare(doctest GIT_REPOSITORY https://github.com/doctest/doctest.git GIT_TAG v2.4.11 GIT_SHALLOW TRUE)
FetchContent_Declare(raylib GIT_REPOSITORY https://github.com/raysan5/raylib.git GIT_TAG 5.5 GIT_SHALLOW TRUE)
# raygui is header-only with no root CMakeLists; SOURCE_SUBDIR to a missing dir makes MakeAvailable only download it.
FetchContent_Declare(raygui GIT_REPOSITORY https://github.com/raysan5/raygui.git GIT_TAG 4.0 GIT_SHALLOW TRUE SOURCE_SUBDIR none)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(enet doctest raylib raygui)

target_include_directories(enet PUBLIC ${enet_SOURCE_DIR}/include)
if(WIN32)
  target_link_libraries(enet PUBLIC ws2_32 winmm)
endif()

add_library(dodge_shared STATIC shared/names.cpp shared/sim.cpp shared/protocol.cpp)
target_include_directories(dodge_shared PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(dodge_shared PUBLIC enet)

# shortcut: header-only until Task 6 adds server sources, then switch to STATIC.
add_library(dodge_server_lib INTERFACE)
target_link_libraries(dodge_server_lib INTERFACE dodge_shared)

add_executable(dodge_server server/main.cpp)
target_link_libraries(dodge_server PRIVATE dodge_server_lib)

add_executable(dodge_client client/main.cpp)
target_include_directories(dodge_client PRIVATE ${raygui_SOURCE_DIR}/src)
target_link_libraries(dodge_client PRIVATE dodge_shared raylib)

enable_testing()
add_executable(dodge_tests tests/test_main.cpp tests/test_smoke.cpp tests/test_names.cpp
  tests/test_sim_players.cpp tests/test_sim_hazards.cpp tests/test_protocol.cpp)
target_link_libraries(dodge_tests PRIVATE dodge_server_lib doctest::doctest)
add_test(NAME dodge_tests COMMAND dodge_tests)
