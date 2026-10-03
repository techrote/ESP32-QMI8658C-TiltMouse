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
    usb_hid_mouse_report
    test_usb_hid_mouse.c
    ${CMAKE_CURRENT_LIST_DIR}/../../components/usb_hid_mouse/usb_hid_mouse_report.c
)
target_include_directories(
    usb_hid_mouse_report
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../../components/usb_hid_mouse/include
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
