cmake_minimum_required(VERSION 2.8)

project(mlnx-fw-dump-me)

set_property(GLOBAL PROPERTY CXX_STANDARD 14)

add_executable(
    fwdumpd
    ${CMAKE_CURRENT_SOURCE_DIR}/src/fwdumpdmain.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/usock.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/usock.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sdk_fd_wrap.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sdk_fd_wrap.h
)

target_link_libraries(
    fwdumpd
    sxcore
    sxapi
    sxhostifc
    pthread
    swsscommon
)

add_executable(
    fwdumpcli
    ${CMAKE_CURRENT_SOURCE_DIR}/src/fwdumpcli.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/usock.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/usock.h
)

target_link_libraries(
    fwdumpcli
    swsscommon
)

install(TARGETS fwdumpd fwdumpcli DESTINATION /usr/bin/)
