cmake_minimum_required(VERSION 3.1)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

project(remoteaccessd)

# Make sure we generate a compile command database for clang-tidy
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB SRC_LIST 
    ${PROJECT_SOURCE_DIR}/*.cpp
)

file(GLOB WAV_LIST 
    ${PROJECT_SOURCE_DIR}/*.wav
)

# C++14
# add_compile_options(-std=c++14) # does not work on macOS
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
#set(CMAKE_CXX_CLANG_TIDY clang-tidy)

# Make sure we build in release mode
if (NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
endif()

# Install target
add_executable(${PROJECT_NAME} ${SRC_LIST})
target_link_libraries(${PROJECT_NAME} stdc++fs)
install(CODE "MESSAGE(\"Installing daemon...\")")
install(TARGETS ${PROJECT_NAME} DESTINATION /usr/local/bin)
install(CODE "MESSAGE(\"Installing sounds...\")")
install(FILES ${WAV_LIST} DESTINATION /usr/local/share/${PROJECT_NAME})
install(CODE "MESSAGE(\"Stopping systemd service...\")")
install(CODE "execute_process(COMMAND 'systemctl stop remoteaccess')")
install(CODE "MESSAGE(\"(Re-)installing systemd service...\")")
install(FILES ${PROJECT_SOURCE_DIR}/remoteaccess.service DESTINATION /lib/systemd/system)
install(CODE "execute_process(COMMAND 'chmod 644 /lib/systemd/system/remoteaccess.service')")
install(CODE "execute_process(COMMAND 'systemctl daemon-reload')")
install(CODE "execute_process(COMMAND 'systemctl enable remoteaccess')")
install(CODE "MESSAGE(\"Starting systemd service...\")")
install(CODE "execute_process(COMMAND 'systemctl start remoteaccess')")
install(CODE "MESSAGE(\"Done.\")")

# Uninstall target
if(NOT TARGET uninstall)
    configure_file(
        "${CMAKE_MODULE_PATH}/uninstall.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)
    add_custom_target(uninstall
        COMMAND echo \"Uninstalling deamon...\"
        COMMAND systemctl stop remoteaccess
        COMMAND systemctl disable remoteaccess
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
        COMMAND systemctl daemon-reload
        COMMAND echo \"Done.\")
endif()
