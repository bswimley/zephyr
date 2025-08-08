# Override the default linker script with our full .ld
zephyr_library_sources(mmu_patch.c)

zephyr_code_relocate(FILES zephyr/arch/arm/core/cortex_a_r/vector_table.S
                     LOCATION VECTORS)

zephyr_code_relocate(FILES zephyr/arch/arm/core/cortex_a_r/reset.S
                     LOCATION RESET)