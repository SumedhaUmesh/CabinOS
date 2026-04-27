add_library(cabinos_core
    src/can_bridge.cpp
    src/intent_router.cpp
    src/service_broker.cpp
    src/vehicle_api.cpp
    src/vehicle_services.cpp
)

target_include_directories(cabinos_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(cabinos_core PRIVATE -Wall -Wextra -Wpedantic)

add_executable(cabinos_cli src/main.cpp)
target_link_libraries(cabinos_cli PRIVATE cabinos_core)

add_executable(cabinos_can_ingest src/can_ingest_main.cpp)
target_link_libraries(cabinos_can_ingest PRIVATE cabinos_core)

option(CABINOS_ENABLE_GRPC "Build real gRPC transport" OFF)

if(CABINOS_ENABLE_GRPC)
    find_package(Protobuf REQUIRED)
    find_package(gRPC CONFIG REQUIRED)

    set(CABINOS_PROTO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/proto)
    set(CABINOS_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
    file(MAKE_DIRECTORY ${CABINOS_GEN_DIR})

    set(CABINOS_SERVICE_PROTO ${CABINOS_PROTO_DIR}/services.proto)

    set(CABINOS_SERVICE_PB_CC ${CABINOS_GEN_DIR}/services.pb.cc)
    set(CABINOS_SERVICE_PB_H ${CABINOS_GEN_DIR}/services.pb.h)
    set(CABINOS_SERVICE_GRPC_CC ${CABINOS_GEN_DIR}/services.grpc.pb.cc)
    set(CABINOS_SERVICE_GRPC_H ${CABINOS_GEN_DIR}/services.grpc.pb.h)

    add_custom_command(
        OUTPUT
            ${CABINOS_SERVICE_PB_CC}
            ${CABINOS_SERVICE_PB_H}
            ${CABINOS_SERVICE_GRPC_CC}
            ${CABINOS_SERVICE_GRPC_H}
        COMMAND ${Protobuf_PROTOC_EXECUTABLE}
        ARGS
            --proto_path=${CABINOS_PROTO_DIR}
            --cpp_out=${CABINOS_GEN_DIR}
            --grpc_out=${CABINOS_GEN_DIR}
            --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
            ${CABINOS_SERVICE_PROTO}
        DEPENDS
            ${CABINOS_SERVICE_PROTO}
        COMMENT "Generating gRPC and protobuf sources from services.proto"
        VERBATIM
    )

    add_library(cabinos_proto
        ${CABINOS_SERVICE_PB_CC}
        ${CABINOS_SERVICE_GRPC_CC}
    )
    target_include_directories(cabinos_proto PUBLIC ${CABINOS_GEN_DIR})
    target_link_libraries(cabinos_proto PUBLIC protobuf::libprotobuf gRPC::grpc++)

    add_library(cabinos_grpc_transport
        src/vehicle_api_grpc.cpp
    )
    target_include_directories(cabinos_grpc_transport
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CABINOS_GEN_DIR}
    )
    target_link_libraries(cabinos_grpc_transport
        PUBLIC
            cabinos_core
            cabinos_proto
            gRPC::grpc++
            protobuf::libprotobuf
    )

    add_executable(cabinos_grpc_server src/grpc_server_main.cpp)
    target_include_directories(cabinos_grpc_server PRIVATE ${CABINOS_GEN_DIR})
    target_link_libraries(cabinos_grpc_server PRIVATE cabinos_grpc_transport)

    add_executable(cabinos_grpc_client src/grpc_client_main.cpp)
    target_include_directories(cabinos_grpc_client PRIVATE ${CABINOS_GEN_DIR})
    target_link_libraries(cabinos_grpc_client PRIVATE cabinos_grpc_transport)
endif()

if(CABINOS_BUILD_TESTS)
    include(CTest)
    enable_testing()

    add_executable(cabinos_router_test tests/intent_router_test.cpp)
    target_link_libraries(cabinos_router_test PRIVATE cabinos_core)
    add_test(NAME cabinos_router_test COMMAND cabinos_router_test)
endif()
