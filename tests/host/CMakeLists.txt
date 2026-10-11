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
