cmake_minimum_required(VERSION 3.5)
project(zv_robot_sdk_examples)

# C++ 17
set(CMAKE_CXX_STANDARD           17 )
set(CMAKE_CXX_STANDARD_REQUIRED  ON )
set(CMAKE_CXX_EXTENSIONS         OFF)

# SDK 预构建产物目录（由 build_libs.sh 生成在父目录的 libs/cpp/）
set(SDK_LIBS_DIR    ${CMAKE_CURRENT_LIST_DIR}/../libs/cpp/lib)
set(SDK_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/../libs/cpp/include)

# 导入 zv_robot_sdk 静态库
add_library(zv_robot_sdk STATIC IMPORTED)
set_target_properties(zv_robot_sdk PROPERTIES
    IMPORTED_LOCATION ${SDK_LIBS_DIR}/libzv_robot_sdk.a
)
target_include_directories(zv_robot_sdk INTERFACE
    "${SDK_INCLUDE_DIR}"
    "${SDK_INCLUDE_DIR}/zv/idl/ros2"
    "${SDK_INCLUDE_DIR}/zv/idl/t4"
    "${SDK_INCLUDE_DIR}/zv/robot/internal/idl"
    "${SDK_INCLUDE_DIR}/ddscxx"
)

# DDS 运行时库（zv_robot_sdk.a 依赖，需动态链接）
set(DDS_LIBS
    ${SDK_LIBS_DIR}/libddscxx.so
    ${SDK_LIBS_DIR}/libddsc.so
)

# 运行时 RPATH：构建时指向 libs/cpp/lib
set(CMAKE_BUILD_RPATH ${SDK_LIBS_DIR})

add_executable(helloworldPublisher ${CMAKE_CURRENT_LIST_DIR}/helloworld/publisher.cpp)
target_link_libraries(helloworldPublisher PRIVATE zv_robot_sdk ${DDS_LIBS})

add_executable(helloworldSubscriber ${CMAKE_CURRENT_LIST_DIR}/helloworld/subscriber.cpp)
target_link_libraries(helloworldSubscriber PRIVATE zv_robot_sdk ${DDS_LIBS})

add_executable(lowcmdPublisher ${CMAKE_CURRENT_LIST_DIR}/lowcmd/publisher.cpp)
target_link_libraries(lowcmdPublisher PRIVATE zv_robot_sdk ${DDS_LIBS})

add_executable(lowcmdSubscriber ${CMAKE_CURRENT_LIST_DIR}/lowcmd/subscriber.cpp)
target_link_libraries(lowcmdSubscriber PRIVATE zv_robot_sdk ${DDS_LIBS})

add_executable(imuNavSubscriber ${CMAKE_CURRENT_LIST_DIR}/imu_nav/subscriber.cpp)
target_link_libraries(imuNavSubscriber PRIVATE zv_robot_sdk ${DDS_LIBS})
