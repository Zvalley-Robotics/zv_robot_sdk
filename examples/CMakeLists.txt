cmake_minimum_required(VERSION 3.5)
project(zv_robot_sdk_examples)

# C++ 17
set(CMAKE_CXX_STANDARD           17 )
set(CMAKE_CXX_STANDARD_REQUIRED  ON )
set(CMAKE_CXX_EXTENSIONS         OFF)


add_executable(helloworldPublisher ${CMAKE_CURRENT_LIST_DIR}/helloworld/publisher.cpp)
add_executable(helloworldSubscriber ${CMAKE_CURRENT_LIST_DIR}/helloworld/subscriber.cpp)
add_executable(lowcmdPublisher ${CMAKE_CURRENT_LIST_DIR}/lowcmd/publisher.cpp)
add_executable(lowcmdSubscriber ${CMAKE_CURRENT_LIST_DIR}/lowcmd/subscriber.cpp)

set(TARGETS helloworldPublisher helloworldSubscriber lowcmdPublisher lowcmdSubscriber)
foreach(TARGET_NAME IN LISTS TARGETS)
    target_include_directories(${TARGET_NAME} PRIVATE
                               ${CMAKE_CURRENT_LIST_DIR}/../include
                               ${CMAKE_CURRENT_LIST_DIR}/../include/zv/idl/ros2
                               ${CMAKE_CURRENT_LIST_DIR}/../include/zv/idl/t4
                               ${CMAKE_CURRENT_LIST_DIR}/../include/zv/robot/internal/idl
                               ${CMAKE_CURRENT_LIST_DIR}/../thirdparty/cyclonedds/include
                               ${CMAKE_CURRENT_LIST_DIR}/../thirdparty/cyclonedds/include/ddscxx)
    target_link_libraries(${TARGET_NAME} PRIVATE
                          ${CMAKE_CURRENT_LIST_DIR}/../libs/linux-x86_64-py312/static/libzv_robot_sdk.a
                          ${CMAKE_CURRENT_LIST_DIR}/../thirdparty/cyclonedds/lib/libddscxx.so
                          ${CMAKE_CURRENT_LIST_DIR}/../thirdparty/cyclonedds/lib/libddsc.so)
endforeach()
