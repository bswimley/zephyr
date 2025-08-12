include(${ZEPHYR_BASE}/boards/common/jlink.board.cmake)

# Override the default linker script with our full .ld
set(BOARD_LINKER_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/microzed.ld)
