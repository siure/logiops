add_executable(logid_config_test
        interval_gesture_config_test.cpp
        ../Configuration.cpp
        ../config/config.cpp
        ../util/log.cpp)

target_include_directories(logid_config_test PRIVATE
        ..
        ../../ipcgull/src/include
        ${EVDEV_INCLUDE_DIR}
        ${LIBUDEV_INCLUDE_DIRECTORIES})

target_link_libraries(logid_config_test PRIVATE
        config++
        ipcgull
        ${CMAKE_THREAD_LIBS_INIT}
        ${LIBUDEV_LIBRARIES})

add_test(NAME logid_config_test COMMAND logid_config_test)
