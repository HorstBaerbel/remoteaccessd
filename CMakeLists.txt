cmake_minimum_required(VERSION 3.1)

project(remoteaccessd)
file(GLOB SRC_LIST 
    ${PROJECT_SOURCE_DIR}/*.cpp
)

file(GLOB WAV_LIST 
    ${PROJECT_SOURCE_DIR}/*.wav
)

# C++14
# add_compile_options(-std=c++14) # does not work on macOS
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")

# Make sure we build in release mode
if (NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
endif()

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
