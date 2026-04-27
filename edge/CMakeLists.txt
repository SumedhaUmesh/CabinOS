add_library(cabinos_core
    src/intent_router.cpp
    src/service_broker.cpp
)

target_include_directories(cabinos_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(cabinos_core PRIVATE -Wall -Wextra -Wpedantic)

add_executable(cabinos_cli src/main.cpp)
target_link_libraries(cabinos_cli PRIVATE cabinos_core)

if(CABINOS_BUILD_TESTS)
    include(CTest)
    enable_testing()

    add_executable(cabinos_router_test tests/intent_router_test.cpp)
    target_link_libraries(cabinos_router_test PRIVATE cabinos_core)
    add_test(NAME cabinos_router_test COMMAND cabinos_router_test)
endif()
