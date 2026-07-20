target_sources(${CMAKE_PROJECT_NAME}
    PRIVATE
        AvixGimbalProtocolTest.cc
        AvixGimbalProtocolTest.h
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
