target_sources(${CMAKE_PROJECT_NAME}
    PRIVATE
        AvixGimbalController.cc
        AvixGimbalController.h
        AvixGimbalLink.cc
        AvixGimbalLink.h
        AvixGimbalProtocol.cc
        AvixGimbalProtocol.h
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

qt_add_library(AvixGimbalModule STATIC)

qt_add_qml_module(AvixGimbalModule
    URI AvixGimbal
    VERSION 1.0
    RESOURCE_PREFIX /qml
    QML_FILES
        AvixGimbalControl.qml
    NO_PLUGIN
)
