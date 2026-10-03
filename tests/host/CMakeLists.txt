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
