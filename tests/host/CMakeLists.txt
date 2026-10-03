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
