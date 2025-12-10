/* SPDX-License-Identifier: Apache-2.0 */
#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/kernel.h>
#include <zephyr/toolchain.h>
#include <string.h>
#include <stdint.h>

#define DT_DRV_COMPAT nxp_s32k1xx_flash

/* FTFC controller base address for S32K148 */
#define S32K_FTFC_BASE      0x40020000u

/* Register offsets */
#define FTFC_FSTAT_OFF      0x00
#define FTFC_FERSTAT_OFF    0x2E

/* FSTAT bits */
#define FSTAT_CCIF          BIT(7)
#define FSTAT_RDCOLERR      BIT(6)
#define FSTAT_ACCERR        BIT(5)
#define FSTAT_FPVIOL        BIT(4)

/* Command opcodes */
#define CMD_ERASE_SECTOR    0x09
#define CMD_PROGRAM_PHRASE  0x07

#define REG8(off) (*(volatile uint8_t *)(S32K_FTFC_BASE + (off)))

/* Immutable config derived from Devicetree */
struct s32k_flash_config {
    uintptr_t flash_base;
    size_t flash_size;
    size_t erase_block;
    size_t write_block;
    const struct flash_parameters params;
};

/* Runtime data */
struct s32k_flash_data {
    struct k_mutex lock;
};

/* --- RAM-resident helpers --- */
__ramfunc static int ftfc_clear_errors_and_ready(void)
{
    uint8_t fstat = REG8(FTFC_FSTAT_OFF);
    if (fstat & (FSTAT_RDCOLERR | FSTAT_ACCERR | FSTAT_FPVIOL)) {
        REG8(FTFC_FSTAT_OFF) = (FSTAT_RDCOLERR | FSTAT_ACCERR | FSTAT_FPVIOL);
    }
    while (!(REG8(FTFC_FSTAT_OFF) & FSTAT_CCIF)) { }
    fstat = REG8(FTFC_FSTAT_OFF);
    if (fstat & (FSTAT_RDCOLERR | FSTAT_ACCERR | FSTAT_FPVIOL)) {
        REG8(FTFC_FSTAT_OFF) = (FSTAT_RDCOLERR | FSTAT_ACCERR | FSTAT_FPVIOL);
        return -EIO;
    }
    return 0;
}

__ramfunc static int ftfc_launch_and_wait(void)
{
    REG8(FTFC_FSTAT_OFF) = FSTAT_CCIF;
    while (!(REG8(FTFC_FSTAT_OFF) & FSTAT_CCIF)) { }
    uint8_t fstat = REG8(FTFC_FSTAT_OFF);
    if (fstat & (FSTAT_RDCOLERR | FSTAT_ACCERR | FSTAT_FPVIOL)) {
        REG8(FTFC_FSTAT_OFF) = (FSTAT_RDCOLERR | FSTAT_ACCERR | FSTAT_FPVIOL);
        return -EIO;
    }
    uint8_t ferstat = REG8(FTFC_FERSTAT_OFF);
    if (ferstat) {
        REG8(FTFC_FERSTAT_OFF) = ferstat;
        return -EIO;
    }
    return 0;
}

__ramfunc static int ftfc_cmd_program_phrase(uint32_t addr, const uint8_t *data, size_t phrase_size)
{
    int rc = ftfc_clear_errors_and_ready();
    if (rc) return rc;

    REG8(0x07) = CMD_PROGRAM_PHRASE;
    REG8(0x06) = (addr >> 16) & 0xFF;
    REG8(0x05) = (addr >> 8) & 0xFF;
    REG8(0x04) = addr & 0xFF;

    for (size_t i = 0; i < phrase_size && i < 8; i++) {
        REG8(0x0B + i) = data[i]; /* FCCOB4..FCCOBB */
    }

    return ftfc_launch_and_wait();
}

__ramfunc static int ftfc_cmd_erase_sector(uint32_t addr)
{
    int rc = ftfc_clear_errors_and_ready();
    if (rc) return rc;

    REG8(0x07) = CMD_ERASE_SECTOR;
    REG8(0x06) = (addr >> 16) & 0xFF;
    REG8(0x05) = (addr >> 8) & 0xFF;
    REG8(0x04) = addr & 0xFF;

    return ftfc_launch_and_wait();
}

/* --- Zephyr flash API --- */
static int s32k_flash_read(const struct device *dev, off_t offset, void *buf, size_t len)
{
    const struct s32k_flash_config *cfg = dev->config;
    if (offset < 0 || (size_t)offset + len > cfg->flash_size) {
        return -EINVAL;
    }
    memcpy(buf, (const void *)(cfg->flash_base + offset), len);
    return 0;
}

static int s32k_flash_write(const struct device *dev, off_t offset, const void *buf, size_t len)
{
    const struct s32k_flash_config *cfg = dev->config;
    if (offset < 0 || (size_t)offset + len > cfg->flash_size) {
        return -EINVAL;
    }
    if ((offset % cfg->write_block) != 0 || (len % cfg->write_block) != 0) {
        return -EINVAL;
    }

    const uint8_t *p = buf;
    uint32_t addr = cfg->flash_base + offset;
    for (size_t done = 0; done < len; done += cfg->write_block) {
        int rc = ftfc_cmd_program_phrase(addr, p, cfg->write_block);
        if (rc) return rc;
        addr += cfg->write_block;
        p += cfg->write_block;
    }
    return 0;
}

static int s32k_flash_erase(const struct device *dev, off_t offset, size_t size)
{
    const struct s32k_flash_config *cfg = dev->config;
    if (offset < 0 || (size_t)offset + size > cfg->flash_size) {
        return -EINVAL;
    }
    if ((offset % cfg->erase_block) != 0 || (size % cfg->erase_block) != 0) {
        return -EINVAL;
    }

    uint32_t addr = cfg->flash_base + offset;
    for (size_t done = 0; done < size; done += cfg->erase_block) {
        int rc = ftfc_cmd_erase_sector(addr);
        if (rc) return rc;
        addr += cfg->erase_block;
    }
    return 0;
}

static const struct flash_parameters *s32k_get_parameters(const struct device *dev)
{
    const struct s32k_flash_config *cfg = dev->config;
    return &cfg->params;
}

/* Provide size via driver API */
static int s32k_get_size(const struct device *dev, uint64_t *out)
{
    const struct s32k_flash_config *cfg = dev->config;
    *out = cfg->flash_size;
    return 0;
}

static int s32k_flash_init(const struct device *dev)
{
    struct s32k_flash_data *data = dev->data;
    k_mutex_init(&data->lock);
    REG8(FTFC_FSTAT_OFF) = (FSTAT_RDCOLERR | FSTAT_ACCERR | FSTAT_FPVIOL);
    REG8(FTFC_FERSTAT_OFF) = REG8(FTFC_FERSTAT_OFF);
    return 0;
}
#define S32K148_FLASH_PAGE_SIZE   0x1000   /* 4 KB sectors */
#define S32K148_FLASH_SIZE        (1536 * 1024)

static const struct flash_pages_layout s32k_layout = {
    .pages_count = S32K148_FLASH_SIZE / S32K148_FLASH_PAGE_SIZE,
    .pages_size  = S32K148_FLASH_PAGE_SIZE,
};

static void s32k_page_layout(const struct device *dev,
                             const struct flash_pages_layout **layout,
                             size_t *layout_size)
{
    ARG_UNUSED(dev);
    *layout = &s32k_layout;
    *layout_size = 1;   /* only one uniform layout */
}

static const struct flash_driver_api s32k_flash_api = {
    .read = s32k_flash_read,
    .write = s32k_flash_write,
    .erase = s32k_flash_erase,
    .get_parameters = s32k_get_parameters,
    .get_size = s32k_get_size,
    .page_layout = s32k_page_layout,
};

/* --- DT glue: bind to flash0 --- */

#define FLASH_INIT(inst) \
    static const struct s32k_flash_config s32k_flash_cfg_##inst = { \
        .flash_base  = DT_INST_REG_ADDR(inst), \
        .flash_size  = DT_INST_REG_SIZE(inst), \
        .erase_block = DT_INST_PROP(inst, erase_block_size), \
        .write_block = DT_INST_PROP(inst, write_block_size), \
        .params = { \
            .write_block_size = DT_INST_PROP(inst, write_block_size), \
            .erase_value = 0xFF, \
        }, \
    }; \
    static struct s32k_flash_data s32k_flash_data_##inst; \
    DEVICE_DT_INST_DEFINE(inst, \
        s32k_flash_init, NULL, \
        &s32k_flash_data_##inst, &s32k_flash_cfg_##inst, \
        POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE, \
        &s32k_flash_api);

DT_INST_FOREACH_STATUS_OKAY(FLASH_INIT)

/* Compile-time sanity: ensure an OKAY node exists for this compatible */
BUILD_ASSERT(DT_HAS_COMPAT_STATUS_OKAY(nxp_s32k1xx_flash),
             "No nxp,s32k1xx-flash nodes OKAY");

#if DT_HAS_CHOSEN(zephyr_flash_controller)
BUILD_ASSERT(DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_flash_controller),
                                nxp_s32k1xx_flash),
             "zephyr,flash-controller is not nxp,s32k1xx-flash");
#endif
