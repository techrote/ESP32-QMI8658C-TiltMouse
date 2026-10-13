cmake_minimum_required(VERSION 3.16)
project(tiltmouse_host_tests LANGUAGES C)

enable_testing()

add_executable(board_contract test_board_contract.c)
target_include_directories(
    board_contract
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../../components/board/include
)
target_compile_features(board_contract PRIVATE c_std_11)
target_compile_options(board_contract PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_test(NAME board_contract COMMAND board_contract)

add_executable(
    touch_buttons
    test_touch_buttons.c
    ${CMAKE_CURRENT_LIST_DIR}/../../components/touch_buttons/touch_buttons.c
)
target_include_directories(
    touch_buttons
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../../components/touch_buttons/include
        ${CMAKE_CURRENT_LIST_DIR}/../../components/touch_input/include
)
target_compile_definitions(
    touch_buttons
    PRIVATE
        TOUCH_FIXTURE_PATH="${CMAKE_CURRENT_LIST_DIR}/../fixtures/touch_replay.csv"
)
target_compile_features(touch_buttons PRIVATE c_std_11)
target_compile_options(touch_buttons PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_test(NAME touch_buttons COMMAND touch_buttons)


add_executable(
    usb_hid_mouse_report
    test_usb_hid_mouse.c
    ${CMAKE_CURRENT_LIST_DIR}/../../components/usb_hid_mouse/usb_hid_mouse_report.c
    ${CMAKE_CURRENT_LIST_DIR}/../../components/mouse_report/mouse_report.c
)
target_include_directories(
    usb_hid_mouse_report
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../../components/usb_hid_mouse/include
        ${CMAKE_CURRENT_LIST_DIR}/../../components/mouse_report/include
)
target_compile_features(usb_hid_mouse_report PRIVATE c_std_11)
target_compile_options(
    usb_hid_mouse_report
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
)
add_test(NAME usb_hid_mouse_report COMMAND usb_hid_mouse_report)

add_executable(
    qmi8658_driver
    test_qmi8658.c
    ${CMAKE_CURRENT_LIST_DIR}/../../components/qmi8658/qmi8658.c
)
target_include_directories(
    qmi8658_driver
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../../components/qmi8658/include
)
target_compile_features(qmi8658_driver PRIVATE c_std_11)
target_compile_options(qmi8658_driver PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_test(NAME qmi8658_driver COMMAND qmi8658_driver)

add_executable(
    orientation_motion
    test_orientation_motion.c
    ${CMAKE_CURRENT_LIST_DIR}/../../components/orientation/orientation.c
    ${CMAKE_CURRENT_LIST_DIR}/../../components/mouse_motion/mouse_motion.c
)
target_include_directories(
    orientation_motion
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../../components/board/include
        ${CMAKE_CURRENT_LIST_DIR}/../../components/orientation/include
        ${CMAKE_CURRENT_LIST_DIR}/../../components/mouse_motion/include
)
target_compile_definitions(
    orientation_motion
    PRIVATE
        IMU_FIXTURE_PATH="${CMAKE_CURRENT_LIST_DIR}/../fixtures/imu_motion_replay.csv"
)
target_compile_features(orientation_motion PRIVATE c_std_11)
target_compile_options(orientation_motion PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_link_libraries(orientation_motion PRIVATE m)

add_test(NAME orientation_motion COMMAND orientation_motion)

add_executable(
    mouse_transport
    test_mouse_transport.c
    ${CMAKE_CURRENT_LIST_DIR}/../../components/mouse_report/mouse_report.c
    ${CMAKE_CURRENT_LIST_DIR}/../../components/mouse_report/mouse_transport.c
)
target_include_directories(
    mouse_transport
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../../components/mouse_report/include
)
target_compile_features(mouse_transport PRIVATE c_std_11)
target_compile_options(mouse_transport PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME mouse_transport COMMAND mouse_transport)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(
    NAME usb_hid_descriptor
    COMMAND
        ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_LIST_DIR}/test_usb_hid_descriptor.py
        ${CMAKE_CURRENT_LIST_DIR}/../../components/usb_hid_mouse/usb_hid_mouse.c
)

find_package(OpenSSL REQUIRED COMPONENTS Crypto)
add_executable(
    espnow_sender
    test_espnow_sender.c
    ${CMAKE_CURRENT_LIST_DIR}/../../components/espnow_mouse/espnow_codec.c
    ${CMAKE_CURRENT_LIST_DIR}/../../components/mouse_report/mouse_report.c
)
target_include_directories(
    espnow_sender PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../../components/espnow_mouse/include
        ${CMAKE_CURRENT_LIST_DIR}/../../components/mouse_report/include
)
target_compile_features(espnow_sender PRIVATE c_std_11)
target_compile_options(espnow_sender PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_link_libraries(espnow_sender PRIVATE OpenSSL::Crypto)
add_test(NAME espnow_sender COMMAND espnow_sender)

add_executable(
    app_pipeline
    test_app_pipeline.c
    ${CMAKE_CURRENT_LIST_DIR}/../../components/app_pipeline/app_pipeline.c
    ${CMAKE_CURRENT_LIST_DIR}/../../components/orientation/orientation.c
    ${CMAKE_CURRENT_LIST_DIR}/../../components/mouse_motion/mouse_motion.c
    ${CMAKE_CURRENT_LIST_DIR}/../../components/touch_buttons/touch_buttons.c
    ${CMAKE_CURRENT_LIST_DIR}/../../components/mouse_report/mouse_report.c
)
target_include_directories(
    app_pipeline
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../../components/app_pipeline/include
        ${CMAKE_CURRENT_LIST_DIR}/../../components/orientation/include
        ${CMAKE_CURRENT_LIST_DIR}/../../components/mouse_motion/include
        ${CMAKE_CURRENT_LIST_DIR}/../../components/touch_buttons/include
        ${CMAKE_CURRENT_LIST_DIR}/../../components/touch_input/include
        ${CMAKE_CURRENT_LIST_DIR}/../../components/mouse_report/include
)
target_compile_features(app_pipeline PRIVATE c_std_11)
target_compile_options(app_pipeline PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_link_libraries(app_pipeline PRIVATE m)
add_test(NAME app_pipeline COMMAND app_pipeline)

add_executable(
    espnow_health
    test_espnow_health.c
    ${CMAKE_CURRENT_LIST_DIR}/../../components/espnow_mouse/espnow_health.c
)
target_include_directories(
    espnow_health PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../../components/espnow_mouse/include
)
target_compile_features(espnow_health PRIVATE c_std_11)
target_compile_options(espnow_health PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME espnow_health COMMAND espnow_health)

add_executable(
    usb_lifecycle
    test_usb_lifecycle.c
    ${CMAKE_CURRENT_LIST_DIR}/../../components/usb_hid_mouse/usb_hid_mouse.c
    ${CMAKE_CURRENT_LIST_DIR}/../../components/usb_hid_mouse/usb_hid_mouse_report.c
    ${CMAKE_CURRENT_LIST_DIR}/../../components/mouse_report/mouse_report.c
    ${CMAKE_CURRENT_LIST_DIR}/../../components/mouse_report/mouse_transport.c
)
target_include_directories(
    usb_lifecycle PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/fakes
        ${CMAKE_CURRENT_LIST_DIR}/../../components/usb_hid_mouse/include
        ${CMAKE_CURRENT_LIST_DIR}/../../components/mouse_report/include
)
target_compile_features(usb_lifecycle PRIVATE c_std_11)
target_compile_options(usb_lifecycle PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME usb_lifecycle COMMAND usb_lifecycle)

add_executable(app_main_timing
    test_app_main_timing.c
    ${CMAKE_CURRENT_LIST_DIR}/../../components/app_pipeline/app_pipeline.c
    ${CMAKE_CURRENT_LIST_DIR}/../../components/orientation/orientation.c
    ${CMAKE_CURRENT_LIST_DIR}/../../components/mouse_motion/mouse_motion.c
    ${CMAKE_CURRENT_LIST_DIR}/../../components/touch_buttons/touch_buttons.c
    ${CMAKE_CURRENT_LIST_DIR}/../../components/mouse_report/mouse_report.c)
target_include_directories(app_main_timing PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/main_fakes
    ${CMAKE_CURRENT_LIST_DIR}/fakes
    ${CMAKE_CURRENT_LIST_DIR}/../../components/app_pipeline/include
    ${CMAKE_CURRENT_LIST_DIR}/../../components/board/include
    ${CMAKE_CURRENT_LIST_DIR}/../../components/qmi8658/include
    ${CMAKE_CURRENT_LIST_DIR}/../../components/touch_input/include
    ${CMAKE_CURRENT_LIST_DIR}/../../components/touch_buttons/include
    ${CMAKE_CURRENT_LIST_DIR}/../../components/orientation/include
    ${CMAKE_CURRENT_LIST_DIR}/../../components/mouse_motion/include
    ${CMAKE_CURRENT_LIST_DIR}/../../components/mouse_report/include
    ${CMAKE_CURRENT_LIST_DIR}/../../components/usb_hid_mouse/include
    ${CMAKE_CURRENT_LIST_DIR}/../../components/espnow_mouse/include)
target_compile_features(app_main_timing PRIVATE c_std_11)
target_compile_options(app_main_timing PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_link_libraries(app_main_timing PRIVATE m)
add_test(NAME app_main_timing COMMAND app_main_timing)

# Same actual target main and fakes, with the captured stationary gyro offset.
get_target_property(TM_MAIN_TEST_SOURCES app_main_timing SOURCES)
get_target_property(TM_MAIN_TEST_INCLUDES app_main_timing INCLUDE_DIRECTORIES)
add_executable(app_main_bias ${TM_MAIN_TEST_SOURCES})
target_include_directories(app_main_bias PRIVATE ${TM_MAIN_TEST_INCLUDES})
target_compile_definitions(app_main_bias PRIVATE TM_TEST_GYRO_BIAS=1)
target_compile_features(app_main_bias PRIVATE c_std_11)
target_compile_options(app_main_bias PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_link_libraries(app_main_bias PRIVATE m)
add_test(NAME app_main_bias COMMAND app_main_bias)
