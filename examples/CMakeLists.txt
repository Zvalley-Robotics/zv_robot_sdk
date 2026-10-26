cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(zv_robot_sdk_examples LANGUAGES CXX)

    set(SDK_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
    set(SDK_INCLUDE_DIR "${SDK_ROOT}/libs/cpp/include")
    set(SDK_LIB_DIR "${SDK_ROOT}/libs/cpp/lib")
    # ZV_ROBOT_SDK_MULTIARCH_LAYOUT: 双架构合并包按目标架构选择运行库；单架构中间包保持平铺结构。
    if(NOT EXISTS "${SDK_LIB_DIR}/libzv_robot_sdk.so" AND
       (IS_DIRECTORY "${SDK_LIB_DIR}/x86_64" OR IS_DIRECTORY "${SDK_LIB_DIR}/aarch64"))
        string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _zv_processor)
        if(_zv_processor MATCHES "^(x86_64|amd64)$")
            set(_zv_arch x86_64)
        elseif(_zv_processor MATCHES "^(aarch64|arm64)$")
            set(_zv_arch aarch64)
        else()
            message(FATAL_ERROR "Unsupported SDK architecture: ${CMAKE_SYSTEM_PROCESSOR}")
        endif()
        set(SDK_LIB_DIR "${SDK_LIB_DIR}/${_zv_arch}")
        if(NOT EXISTS "${SDK_LIB_DIR}/libzv_robot_sdk.so")
            message(FATAL_ERROR "Missing SDK library for ${_zv_arch}: ${SDK_LIB_DIR}")
        endif()
    endif()
    set(ZV_ROBOT_SDK_PREFIX "" CACHE PATH
        "Use only this installed SDK prefix, overriding bundled libs/cpp")

    if(ZV_ROBOT_SDK_PREFIX)
        # An explicit installation must win over bundled artifacts and cached packages.
        unset(zv_robot_sdk_DIR CACHE)
        unset(zv_robot_sdk_DIR)
        find_package(zv_robot_sdk CONFIG REQUIRED
            PATHS "${ZV_ROBOT_SDK_PREFIX}" NO_DEFAULT_PATH)
        message(STATUS "Examples SDK package: ${zv_robot_sdk_DIR}")
    elseif(EXISTS "${SDK_LIB_DIR}/libzv_robot_sdk.so")
        message(STATUS "Examples SDK bundled libraries: ${SDK_LIB_DIR}")
        find_package(CURL REQUIRED)
        find_package(Threads REQUIRED)

        foreach(required_library
                "${SDK_LIB_DIR}/libzv_robot_sdk.so"
                "${SDK_LIB_DIR}/libddsc.so"
                "${SDK_LIB_DIR}/libddscxx.so")
            if(NOT EXISTS "${required_library}")
                message(FATAL_ERROR "Missing SDK library: ${required_library}")
            endif()
        endforeach()

        add_library(zv_robot_sdk SHARED IMPORTED GLOBAL)
        set_target_properties(zv_robot_sdk PROPERTIES
            IMPORTED_LOCATION "${SDK_LIB_DIR}/libzv_robot_sdk.so"
            INTERFACE_INCLUDE_DIRECTORIES
                "${SDK_INCLUDE_DIR};${SDK_INCLUDE_DIR}/zv/idl/ros2;${SDK_INCLUDE_DIR}/zv/idl/t3;${SDK_INCLUDE_DIR}/zv/idl/t4;${SDK_INCLUDE_DIR}/zv/robot/internal/idl;${SDK_INCLUDE_DIR}/ddscxx"
        )
        target_link_libraries(zv_robot_sdk INTERFACE
            "${SDK_LIB_DIR}/libddscxx.so"
            "${SDK_LIB_DIR}/libddsc.so"
            CURL::libcurl
            Threads::Threads
            ${CMAKE_DL_LIBS}
        )

        set(ZV_ROBOT_SDK_PACKAGED_LAYOUT ON)
        set(CMAKE_BUILD_RPATH_USE_ORIGIN ON)
        # 相对运行时输出目录计算 RPATH，双架构包的 lib/<arch> 也能正确解析。
        set(_zv_runtime_directory "${CMAKE_CURRENT_BINARY_DIR}")
        if(CMAKE_RUNTIME_OUTPUT_DIRECTORY)
            get_filename_component(_zv_runtime_directory "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}" ABSOLUTE)
        endif()
        file(RELATIVE_PATH _zv_library_relative "${_zv_runtime_directory}" "${SDK_LIB_DIR}")
        set(ZV_ROBOT_SDK_PACKAGED_RPATH "\$ORIGIN/${_zv_library_relative}")
    else()
        find_package(zv_robot_sdk REQUIRED)
        find_package(CURL REQUIRED)
        message(STATUS "Examples SDK package: ${zv_robot_sdk_DIR}")
    endif()
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

function(add_sdk_example target source)
    add_executable(${target} "${CMAKE_CURRENT_LIST_DIR}/${source}")
    target_link_libraries(${target} PRIVATE zv_robot_sdk)
    # Packaged SDKs use a manually imported target without exported link options.
    target_link_options(${target} PRIVATE "LINKER:--disable-new-dtags")
    if(ZV_ROBOT_SDK_PACKAGED_LAYOUT)
        set_target_properties(${target} PROPERTIES
            BUILD_WITH_INSTALL_RPATH TRUE
            INSTALL_RPATH "${ZV_ROBOT_SDK_PACKAGED_RPATH}"
            INSTALL_RPATH_USE_LINK_PATH FALSE
        )
    endif()
endfunction()

add_sdk_example(helloworldPublisher helloworld/publisher.cpp)
add_sdk_example(helloworldSubscriber helloworld/subscriber.cpp)
add_sdk_example(lowcmdPublisher lowcmd/publisher.cpp)
add_sdk_example(lowcmdSubscriber lowcmd/subscriber.cpp)
add_sdk_example(loco highcmd/loco.cpp)
add_sdk_example(robotIntentPublisher highcmd/intent_publisher.cpp)
add_sdk_example(robotStateSubscriber highcmd/state_subscriber.cpp)
add_sdk_example(slam slam/slam.cpp)
add_sdk_example(dataCollection data_collection/data_collection.cpp)
add_sdk_example(streamer streamer/streamer.cpp)
add_sdk_example(cameraPreview camera/camera_preview.cpp)
add_sdk_example(cameraSubscriber camera/subscriber.cpp)
add_sdk_example(auth auth/auth.cpp)
add_sdk_example(audio audio/audio.cpp)
add_sdk_example(teleop teleop/teleop.cpp)
add_sdk_example(imuNavSubscriber imu_nav/subscriber.cpp)
add_sdk_example(t3StateMonitor t3/state_monitor.cpp)
add_sdk_example(t3ActionDemo t3/action_demo.cpp)

set(ZV_ROBOT_SDK_EXAMPLE_TARGETS
    helloworldPublisher
    helloworldSubscriber
    lowcmdPublisher
    lowcmdSubscriber
    loco
    robotIntentPublisher
    robotStateSubscriber
    slam
    dataCollection
    streamer
    cameraPreview
    cameraSubscriber
    auth
    audio
    teleop
    imuNavSubscriber
    t3StateMonitor
    t3ActionDemo
)

if(NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    foreach(example_target IN LISTS ZV_ROBOT_SDK_EXAMPLE_TARGETS)
        set_target_properties(${example_target} PROPERTIES
            INSTALL_RPATH "\$ORIGIN/../../../${CMAKE_INSTALL_LIBDIR}")
    endforeach()
    install(TARGETS ${ZV_ROBOT_SDK_EXAMPLE_TARGETS}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/zv_robot_sdk/examples)
endif()
