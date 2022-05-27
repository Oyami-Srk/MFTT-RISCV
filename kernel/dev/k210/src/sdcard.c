#include <dev/buffered_io.h>
#include <driver/console.h>
#include <lib/sys/spinlock.h>
#include <riscv.h>
#include <types.h>

#include "../dmac.h"
#include "../gpiohs.h"
#include "../memmaps.h"
#include "../spi.h"

#if 0
void SD_CS_HIGH(void) { gpiohs_set_pin(7, GPIO_PV_HIGH); }

void SD_CS_LOW(void) { gpiohs_set_pin(7, GPIO_PV_LOW); }

void SD_HIGH_SPEED_ENABLE(void) {
    // spi_set_clk_rate(SPI_DEVICE_0, 10000000);
}

static void sd_lowlevel_init(uint8_t spi_index) {
    gpiohs_set_drive_mode(7, GPIO_DM_OUTPUT);
    // spi_set_clk_rate(SPI_DEVICE_0, 200000);     /*set clk rate*/
}

static void sd_write_data(uint8_t const *data_buff, uint32_t length) {
    spi_init(SPI_DEVICE_0, SPI_WORK_MODE_0, SPI_FF_STANDARD, 8, 0);
    spi_send_data_standard(SPI_DEVICE_0, SPI_CHIP_SELECT_3, NULL, 0, data_buff,
                           length);
}

static void sd_read_data(uint8_t *data_buff, uint32_t length) {
    spi_init(SPI_DEVICE_0, SPI_WORK_MODE_0, SPI_FF_STANDARD, 8, 0);
    spi_receive_data_standard(SPI_DEVICE_0, SPI_CHIP_SELECT_3, NULL, 0,
                              data_buff, length);
}

static void sd_write_data_dma(uint8_t const *data_buff, uint32_t length) {
    spi_init(SPI_DEVICE_0, SPI_WORK_MODE_0, SPI_FF_STANDARD, 8, 0);
    spi_send_data_standard_dma(DMAC_CHANNEL0, SPI_DEVICE_0, SPI_CHIP_SELECT_3,
                               NULL, 0, data_buff, length);
}

static void sd_read_data_dma(uint8_t *data_buff, uint32_t length) {
    spi_init(SPI_DEVICE_0, SPI_WORK_MODE_0, SPI_FF_STANDARD, 8, 0);
    spi_receive_data_standard_dma(-1, DMAC_CHANNEL0, SPI_DEVICE_0,
                                  SPI_CHIP_SELECT_3, NULL, 0, data_buff,
                                  length);
}

/*
 * @brief  Send 5 bytes command to the SD card.
 * @param  Cmd: The user expected command to send to SD card.
 * @param  Arg: The command argument.
 * @param  Crc: The CRC.
 * @retval None
 */
static void sd_send_cmd(uint8_t cmd, uint32_t arg, uint8_t crc) {
    uint8_t frame[6];
    frame[0] = (cmd | 0x40);
    frame[1] = (uint8_t)(arg >> 24);
    frame[2] = (uint8_t)(arg >> 16);
    frame[3] = (uint8_t)(arg >> 8);
    frame[4] = (uint8_t)(arg);
    frame[5] = (crc);
    SD_CS_LOW();
    sd_write_data(frame, 6);
}

static void sd_end_cmd(void) {
    uint8_t frame[1] = {0xFF};
    /*!< SD chip select high */
    SD_CS_HIGH();
    /*!< Send the Cmd bytes */
    sd_write_data(frame, 1);
}

/*
 * Be noticed: all commands & responses below
 * 		are in SPI mode format. May differ from
 * 		what they are in SD mode.
 */

#define SD_CMD0   0
#define SD_CMD8   8
#define SD_CMD58  58 // READ_OCR
#define SD_CMD55  55 // APP_CMD
#define SD_ACMD41 41 // SD_SEND_OP_COND
#define SD_CMD16  16 // SET_BLOCK_SIZE
#define SD_CMD17  17 // READ_SINGLE_BLOCK
#define SD_CMD24  24 // WRITE_SINGLE_BLOCK
#define SD_CMD13  13 // SEND_STATUS

/*
 * Read sdcard response in R1 type.
 */
static uint8_t sd_get_response_R1(void) {
    uint8_t  result;
    uint16_t timeout = 0xff;

    while (timeout--) {
        sd_read_data(&result, 1);
        if (result != 0xff)
            return result;
    }

    // timeout!
    return 0xff;
}

/*
 * Read the rest of R3 response
 * Be noticed: frame should be at least 4-byte long
 */
static void sd_get_response_R3_rest(uint8_t *frame) { sd_read_data(frame, 4); }

/*
 * Read the rest of R7 response
 * Be noticed: frame should be at least 4-byte long
 */
static void sd_get_response_R7_rest(uint8_t *frame) { sd_read_data(frame, 4); }

static int switch_to_SPI_mode(void) {
    int timeout = 0xff;

    while (--timeout) {
        sd_send_cmd(SD_CMD0, 0, 0x95);
        uint64_t result = sd_get_response_R1();
        sd_end_cmd();

        if (0x01 == result)
            break;
    }
    if (0 == timeout) {
        kprintf("SD_CMD0 failed\n");
        return 0xff;
    }

    return 0;
}

// verify supply voltage range
static int verify_operation_condition(void) {
    uint64_t result;

    // Stores the response reversely.
    // That means
    // frame[2] - VCA
    // frame[3] - Check Pattern
    uint8_t frame[4];

    sd_send_cmd(SD_CMD8, 0x01aa, 0x87);
    result = sd_get_response_R1();
    sd_get_response_R7_rest(frame);
    sd_end_cmd();

    if (0x09 == result) {
        kprintf("invalid CRC for CMD8\n");
        return 0xff;
    } else if (0x01 == result && 0x01 == (frame[2] & 0x0f) &&
               0xaa == frame[3]) {
        return 0x00;
    }

    kprintf("verify_operation_condition() fail!\n");
    return 0xff;
}

// read OCR register to check if the voltage range is valid
// this step is not mandotary, but I advise to use it
static int read_OCR(void) {
    uint64_t result;
    uint8_t  ocr[4];

    int timeout;

    timeout = 0xff;
    while (--timeout) {
        sd_send_cmd(SD_CMD58, 0, 0);
        result = sd_get_response_R1();
        sd_get_response_R3_rest(ocr);
        sd_end_cmd();

        if (0x01 == result &&                  // R1 response in idle status
            (ocr[1] & 0x1f) && (ocr[2] & 0x80) // voltage range valid
        ) {
            return 0;
        }
    }

    // timeout!
    kprintf("read_OCR() timeout!\n");
    kprintf("result = %d\n", result);
    return 0xff;
}

// send ACMD41 to tell sdcard to finish initializing
static int set_SDXC_capacity(void) {
    uint8_t result = 0xff;

    int timeout = 0xfff;
    while (--timeout) {
        sd_send_cmd(SD_CMD55, 0, 0);
        result = sd_get_response_R1();
        sd_end_cmd();
        if (0x01 != result) {
            kprintf("SD_CMD55 fail! result = %d\n", result);
            return 0xff;
        }

        sd_send_cmd(SD_ACMD41, 0x40000000, 0);
        result = sd_get_response_R1();
        sd_end_cmd();
        if (0 == result) {
            return 0;
        }
    }

    // timeout!
    kprintf("set_SDXC_capacity() timeout!\n");
    kprintf("result = %d\n", result);
    return 0xff;
}

// Used to differ whether sdcard is SDSC type.
static int is_standard_sd = 0;

// check OCR register to see the type of sdcard,
// thus determine whether block size is suitable to buffer size
static int check_block_size(void) {
    uint8_t result = 0xff;
    uint8_t ocr[4];

    int timeout = 0xff;
    while (timeout--) {
        sd_send_cmd(SD_CMD58, 0, 0);
        result = sd_get_response_R1();
        sd_get_response_R3_rest(ocr);
        sd_end_cmd();

        if (0 == result) {
            if (ocr[0] & 0x40) {
                kprintf("[SDCARD] SDHC/SDXC detected!\n");
                if (512 != BUFFER_SIZE) {
                    kprintf("BUFFER_SIZE != 512\n");
                    return 0xff;
                }

                is_standard_sd = 0;
            } else {
                kprintf("[SDCARD] SDSC detected, setting block size.\n");

                // setting SD card block size to BUFFER_SIZE
                int timeout = 0xff;
                int result  = 0xff;
                while (--timeout) {
                    sd_send_cmd(SD_CMD16, BUFFER_SIZE, 0);
                    result = sd_get_response_R1();
                    sd_end_cmd();

                    if (0 == result)
                        break;
                }
                if (0 == timeout) {
                    kprintf("check_OCR(): fail to set block size");
                    return 0xff;
                }

                is_standard_sd = 1;
            }

            return 0;
        }
    }

    // timeout!
    kprintf("check_OCR() timeout!\n");
    kprintf("result = %d\n", result);
    return 0xff;
}

/*
 * @brief  Initializes the SD/SD communication.
 * @param  None
 * @retval The SD Response:
 *         - 0xFF: Sequence failed
 *         - 0: Sequence succeed
 */
static int sd_init(void) {
    uint8_t frame[10];

    sd_lowlevel_init(0);
    // SD_CS_HIGH();
    SD_CS_LOW();

    // send dummy bytes for 80 clock cycles
    for (int i = 0; i < 10; i++)
        frame[i] = 0xff;
    sd_write_data(frame, 10);

    if (0 != switch_to_SPI_mode())
        return 0xff;
    if (0 != verify_operation_condition())
        return 0xff;
    if (0 != read_OCR())
        return 0xff;
    if (0 != set_SDXC_capacity())
        return 0xff;
    if (0 != check_block_size())
        return 0xff;

    return 0;
}

static sleeplock_t sdcard_lock;

void sdcard_init(void) {
    int result = sd_init();
    sleeplock_init(&sdcard_lock);

    if (0 != result) {
        kpanic("sdcard_init failed");
    }
}

void sdcard_read_sector(uint8_t *buf, int sectorno) {
    uint8_t  result;
    uint32_t address;
    uint8_t  dummy_crc[2];

    if (is_standard_sd) {
        address = sectorno << 9;
    } else {
        address = sectorno;
    }

    // enter critical section!
    sleeplock_acquire(&sdcard_lock);

    sd_send_cmd(SD_CMD17, address, 0);
    result = sd_get_response_R1();

    if (0 != result) {
        sleeplock_release(&sdcard_lock);
        kpanic("sdcard: fail to read");
    }

    int timeout = 0xffffff;
    while (--timeout) {
        sd_read_data(&result, 1);
        if (0xfe == result)
            break;
    }
    if (0 == timeout) {
        kpanic("sdcard: timeout waiting for reading");
    }
    sd_read_data_dma(buf, BUFFER_SIZE);
    sd_read_data(dummy_crc, 2);

    sd_end_cmd();

    sleeplock_release(&sdcard_lock);
    // leave critical section!
}

void sdcard_write_sector(uint8_t *buf, int sectorno) {
    uint32_t             address;
    static uint8_t const START_BLOCK_TOKEN = 0xfe;
    uint8_t              dummy_crc[2]      = {0xff, 0xff};

    if (is_standard_sd) {
        address = sectorno << 9;
    } else {
        address = sectorno;
    }

    // enter critical section!
    sleeplock_acquire(&sdcard_lock);

    sd_send_cmd(SD_CMD24, address, 0);
    if (0 != sd_get_response_R1()) {
        sleeplock_release(&sdcard_lock);
        kpanic("sdcard: fail to write");
    }

    // sending data to be written
    sd_write_data(&START_BLOCK_TOKEN, 1);
    sd_write_data_dma(buf, BUFFER_SIZE);
    sd_write_data(dummy_crc, 2);

    // waiting for sdcard to finish programming
    uint8_t result;
    int     timeout = 0xfff;
    while (--timeout) {
        sd_read_data(&result, 1);
        if (0x05 == (result & 0x1f)) {
            break;
        }
    }
    if (0 == timeout) {
        sleeplock_release(&sdcard_lock);
        kpanic("sdcard: invalid response token");
    }

    timeout = 0xffffff;
    while (--timeout) {
        sd_read_data(&result, 1);
        if (0 != result)
            break;
    }
    if (0 == timeout) {
        sleeplock_release(&sdcard_lock);
        kpanic("sdcard: timeout waiting for response");
    }
    sd_end_cmd();

    // send SD_CMD13 to check if writing is correctly done
    uint8_t error_code = 0xff;
    sd_send_cmd(SD_CMD13, 0, 0);
    result = sd_get_response_R1();
    sd_read_data(&error_code, 1);
    sd_end_cmd();
    if (0 != result || 0 != error_code) {
        sleeplock_release(&sdcard_lock);
        kprintf("result: %x\n", result);
        kprintf("error_code: %x\n", error_code);
        kpanic("sdcard: an error occurs when writing");
    }

    sleeplock_release(&sdcard_lock);
    // leave critical section!
}

// A simple test for sdcard read/write test
void test_sdcard(void) {
    uint8_t buf[BUFFER_SIZE];

    for (int sec = 0; sec < 5; sec++) {
        for (int i = 0; i < BUFFER_SIZE; i++) {
            buf[i] = 0xaa; // data to be written
        }

        sdcard_write_sector(buf, sec);

        for (int i = 0; i < BUFFER_SIZE; i++) {
            buf[i] = 0xff; // fill in junk
        }

        sdcard_read_sector(buf, sec);
        for (int i = 0; i < BUFFER_SIZE; i++) {
            if (0 == i % 16) {
                kprintf("\n");
            }

            kprintf("%x ", buf[i]);
        }
        kprintf("\n");
    }

    while (1)
        ;
}
#else
void SD_CS_HIGH(void) { gpiohs_set_pin(7, GPIO_PV_HIGH); }

void SD_CS_LOW(void) { gpiohs_set_pin(7, GPIO_PV_LOW); }

void SD_HIGH_SPEED_ENABLE(void) {
    // spi_set_clk_rate(SPI_DEVICE_0, 10000000);
}

static void sd_lowlevel_init(uint8_t spi_index) {
    gpiohs_set_drive_mode(7, GPIO_DM_OUTPUT);
    // spi_set_clk_rate(SPI_DEVICE_0, 200000);     /*set clk rate*/
}

static void sd_write_data(uint8_t const *data_buff, uint32_t length) {
    spi_init(SPI_DEVICE_0, SPI_WORK_MODE_0, SPI_FF_STANDARD, 8, 0);
    spi_send_data_standard(SPI_DEVICE_0, SPI_CHIP_SELECT_3, NULL, 0, data_buff,
                           length);
}

static void sd_read_data(uint8_t *data_buff, uint32_t length) {
    spi_init(SPI_DEVICE_0, SPI_WORK_MODE_0, SPI_FF_STANDARD, 8, 0);
    spi_receive_data_standard(SPI_DEVICE_0, SPI_CHIP_SELECT_3, NULL, 0,
                              data_buff, length);
}

/*
 * @brief  Send 5 bytes command to the SD card.
 * @param  Cmd: The user expected command to send to SD card.
 * @param  Arg: The command argument.
 * @param  Crc: The CRC.
 * @retval None
 */
static void sd_send_cmd(uint8_t cmd, uint32_t arg, uint8_t crc) {
    uint8_t frame[6];
    frame[0] = (cmd | 0x40);
    frame[1] = (uint8_t)(arg >> 24);
    frame[2] = (uint8_t)(arg >> 16);
    frame[3] = (uint8_t)(arg >> 8);
    frame[4] = (uint8_t)(arg);
    frame[5] = (crc);
    SD_CS_LOW();
    sd_write_data(frame, 6);
}

static void sd_end_cmd(void) {
    uint8_t frame[1] = {0xFF};
    /*!< SD chip select high */
    SD_CS_HIGH();
    /*!< Send the Cmd bytes */
    sd_write_data(frame, 1);
}

/*
 * Be noticed: all commands & responses below
 * 		are in SPI mode format. May differ from
 * 		what they are in SD mode.
 */

#define SD_CMD0   0
#define SD_CMD8   8
#define SD_CMD58  58 // READ_OCR
#define SD_CMD55  55 // APP_CMD
#define SD_ACMD41 41 // SD_SEND_OP_COND
#define SD_CMD16  16 // SET_BLOCK_SIZE
#define SD_CMD17  17 // READ_SINGLE_BLOCK
#define SD_CMD24  24 // WRITE_SINGLE_BLOCK
#define SD_CMD13  13 // SEND_STATUS

/*
 * Read sdcard response in R1 type.
 */
static uint8_t sd_get_response_R1(void) {
    uint8_t  result;
    uint16_t timeout = 0xff;

    while (timeout--) {
        sd_read_data(&result, 1);
        if (result != 0xff)
            return result;
    }

    // timeout!
    return 0xff;
}

/*
 * Read the rest of R3 response
 * Be noticed: frame should be at least 4-byte long
 */
static void sd_get_response_R3_rest(uint8_t *frame) { sd_read_data(frame, 4); }

/*
 * Read the rest of R7 response
 * Be noticed: frame should be at least 4-byte long
 */
static void sd_get_response_R7_rest(uint8_t *frame) { sd_read_data(frame, 4); }

static int switch_to_SPI_mode(void) {
    int timeout = 0xff;

    while (--timeout) {
        sd_send_cmd(SD_CMD0, 0, 0x95);
        uint64_t result = sd_get_response_R1();
        sd_end_cmd();

        if (0x01 == result)
            break;
    }
    if (0 == timeout) {
        kprintf("SD_CMD0 failed\n");
        return 0xff;
    }

    return 0;
}

// verify supply voltage range
static int verify_operation_condition(void) {
    uint64_t result;

    // Stores the response reversely.
    // That means
    // frame[2] - VCA
    // frame[3] - Check Pattern
    uint8_t frame[4];

    sd_send_cmd(SD_CMD8, 0x01aa, 0x87);
    result = sd_get_response_R1();
    sd_get_response_R7_rest(frame);
    sd_end_cmd();

    if (0x09 == result) {
        kprintf("invalid CRC for CMD8\n");
        return 0xff;
    } else if (0x01 == result && 0x01 == (frame[2] & 0x0f) &&
               0xaa == frame[3]) {
        return 0x00;
    }

    kprintf("verify_operation_condition() fail!\n");
    return 0xff;
}

// read OCR register to check if the voltage range is valid
// this step is not mandotary, but I advise to use it
static int read_OCR(void) {
    uint64_t result;
    uint8_t  ocr[4];

    int timeout;

    timeout = 0xff;
    while (--timeout) {
        sd_send_cmd(SD_CMD58, 0, 0);
        result = sd_get_response_R1();
        sd_get_response_R3_rest(ocr);
        sd_end_cmd();

        if (0x01 == result &&                  // R1 response in idle status
            (ocr[1] & 0x1f) && (ocr[2] & 0x80) // voltage range valid
        ) {
            return 0;
        }
    }

    // timeout!
    kprintf("read_OCR() timeout!\n");
    kprintf("result = %d\n", result);
    return 0xff;
}

// send ACMD41 to tell sdcard to finish initializing
static int set_SDXC_capacity(void) {
    uint8_t result = 0xff;

    int timeout = 0xfff;
    while (--timeout) {
        sd_send_cmd(SD_CMD55, 0, 0);
        result = sd_get_response_R1();
        sd_end_cmd();
        if (0x01 != result) {
            kprintf("SD_CMD55 fail! result = %d\n", result);
            return 0xff;
        }

        sd_send_cmd(SD_ACMD41, 0x40000000, 0);
        result = sd_get_response_R1();
        sd_end_cmd();
        if (0 == result) {
            return 0;
        }
    }

    // timeout!
    kprintf("set_SDXC_capacity() timeout!\n");
    kprintf("result = %d\n", result);
    return 0xff;
}

// Used to differ whether sdcard is SDSC type.
static int is_standard_sd = 0;

// check OCR register to see the type of sdcard,
// thus determine whether block size is suitable to buffer size
static int check_block_size(void) {
    uint8_t result = 0xff;
    uint8_t ocr[4];

    int timeout = 0xff;
    while (timeout--) {
        sd_send_cmd(SD_CMD58, 0, 0);
        result = sd_get_response_R1();
        sd_get_response_R3_rest(ocr);
        sd_end_cmd();

        if (0 == result) {
            if (ocr[0] & 0x40) {
                kprintf("SDHC/SDXC detected\n");
                if (512 != BUFFER_SIZE) {
                    kprintf("BUFFER_SIZE != 512\n");
                    return 0xff;
                }

                is_standard_sd = 0;
            } else {
                kprintf("SDSC detected, setting block size\n");

                // setting SD card block size to BUFFER_SIZE
                int timeout = 0xff;
                int result  = 0xff;
                while (--timeout) {
                    sd_send_cmd(SD_CMD16, BUFFER_SIZE, 0);
                    result = sd_get_response_R1();
                    sd_end_cmd();

                    if (0 == result)
                        break;
                }
                if (0 == timeout) {
                    kprintf("check_OCR(): fail to set block size");
                    return 0xff;
                }

                is_standard_sd = 1;
            }

            return 0;
        }
    }

    // timeout!
    kprintf("check_OCR() timeout!\n");
    kprintf("result = %d\n", result);
    return 0xff;
}

/*
 * @brief  Initializes the SD/SD communication.
 * @param  None
 * @retval The SD Response:
 *         - 0xFF: Sequence failed
 *         - 0: Sequence succeed
 */
static int sd_init(void) {
    uint8_t frame[10];

    sd_lowlevel_init(0);
    // SD_CS_HIGH();
    SD_CS_LOW();

    // send dummy bytes for 80 clock cycles
    for (int i = 0; i < 10; i++)
        frame[i] = 0xff;
    sd_write_data(frame, 10);

    if (0 != switch_to_SPI_mode())
        return 0xff;
    if (0 != verify_operation_condition())
        return 0xff;
    if (0 != read_OCR())
        return 0xff;
    if (0 != set_SDXC_capacity())
        return 0xff;
    if (0 != check_block_size())
        return 0xff;

    return 0;
}

static spinlock_t sdcard_lock;

void sdcard_init(void) {
    int result = sd_init();
    spinlock_init(&sdcard_lock);

    if (0 != result) {
        kpanic("sdcard_init failed");
    }
}

void sdcard_read_sector(uint8_t *buf, int sectorno) {
    uint8_t  result;
    uint32_t address;
    uint8_t  dummy_crc[2];

    if (is_standard_sd) {
        address = sectorno << 9;
    } else {
        address = sectorno;
    }

    // enter critical section!
    spinlock_acquire(&sdcard_lock);

    sd_send_cmd(SD_CMD17, address, 0);
    result = sd_get_response_R1();

    if (0 != result) {
        spinlock_release(&sdcard_lock);
        kpanic("sdcard: fail to read");
    }

    int timeout = 0xfff;
    while (--timeout) {
        sd_read_data(&result, 1);
        if (0xfe == result)
            break;
    }
    if (0 == timeout) {
        kpanic("sdcard: timeout waiting for reading");
    }
    sd_read_data(buf, BUFFER_SIZE);
    sd_read_data(dummy_crc, 2);

    sd_end_cmd();

    spinlock_release(&sdcard_lock);
    // leave critical section!
}

void sdcard_write_sector(uint8_t *buf, int sectorno) {
    uint32_t             address;
    static uint8_t const START_BLOCK_TOKEN = 0xfe;
    uint8_t              dummy_crc[2]      = {0xff, 0xff};

    if (is_standard_sd) {
        address = sectorno << 9;
    } else {
        address = sectorno;
    }

    // enter critical section!
    spinlock_acquire(&sdcard_lock);

    sd_send_cmd(SD_CMD24, address, 0);
    if (0 != sd_get_response_R1()) {
        spinlock_release(&sdcard_lock);
        kpanic("sdcard: fail to write");
    }

    // sending data to be written
    sd_write_data(&START_BLOCK_TOKEN, 1);
    sd_write_data(buf, BUFFER_SIZE);
    sd_write_data(dummy_crc, 2);

    // waiting for sdcard to finish programming
    uint8_t result;
    int     timeout = 0xfff;
    while (--timeout) {
        sd_read_data(&result, 1);
        if (0x05 == (result & 0x1f)) {
            break;
        }
    }
    if (0 == timeout) {
        spinlock_release(&sdcard_lock);
        kpanic("sdcard: invalid response token");
    }

    timeout = 0xffffff;
    while (--timeout) {
        sd_read_data(&result, 1);
        if (0 != result)
            break;
    }
    if (0 == timeout) {
        spinlock_release(&sdcard_lock);
        kpanic("sdcard: timeout waiting for response");
    }
    sd_end_cmd();

    // send SD_CMD13 to check if writing is correctly done
    uint8_t error_code = 0xff;
    sd_send_cmd(SD_CMD13, 0, 0);
    result = sd_get_response_R1();
    sd_read_data(&error_code, 1);
    sd_end_cmd();
    if (0 != result || 0 != error_code) {
        spinlock_release(&sdcard_lock);
        kprintf("result: %x\n", result);
        kprintf("error_code: %x\n", error_code);
        kpanic("sdcard: an error occurs when writing");
    }

    spinlock_release(&sdcard_lock);
    // leave critical section!
}

// A simple test for sdcard read/write test
void test_sdcard(void) {
    uint8_t buf[BUFFER_SIZE];

    for (int sec = 0; sec < 5; sec++) {
        for (int i = 0; i < BUFFER_SIZE; i++) {
            buf[i] = 0xaa; // data to be written
        }

        sdcard_write_sector(buf, sec);

        for (int i = 0; i < BUFFER_SIZE; i++) {
            buf[i] = 0xff; // fill in junk
        }

        sdcard_read_sector(buf, sec);
        for (int i = 0; i < BUFFER_SIZE; i++) {
            if (0 == i % 16) {
                kprintf("\n");
            }

            kprintf("%x ", buf[i]);
        }
        kprintf("\n");
    }

    while (1)
        ;
}
#endif

// VFS-Implements
#include <lib/string.h>
#include <vfs.h>

#define SECTOR_SIZE 512

// func 0 -> read
static int sdcard_disk_rw_lba(size_t offset, char *buf, size_t len, int func) {
    char *kbuf = kmalloc(SECTOR_SIZE);

    size_t remain = len;
    for (; remain > 0;) {
        size_t sector    = offset / SECTOR_SIZE;
        size_t s         = offset % SECTOR_SIZE;
        size_t copy_size = remain > SECTOR_SIZE ? SECTOR_SIZE - s : remain;
        if (func) {
            // write
            if (copy_size != SECTOR_SIZE) {
                // read then write
                memset(kbuf, 0, SECTOR_SIZE);
                sdcard_read_sector((uint8_t *)kbuf, (int)sector);
                memcpy(buf, kbuf + s, copy_size);
                // then write
                sdcard_write_sector((uint8_t *)kbuf, (int)sector);
            } else {
                memcpy(kbuf, buf, copy_size);
                sdcard_write_sector((uint8_t *)kbuf, (int)sector);
            }
        } else {
            // read
            memset(kbuf, 0, SECTOR_SIZE);
            sdcard_read_sector((uint8_t *)kbuf, (int)sector);
            memcpy(buf, kbuf + s, copy_size);
        }
        buf += copy_size;
        offset += copy_size;
        remain -= copy_size;
    }

    kfree(kbuf);
    return len;
}

static int sdcard_disk_write(file_t *file, const char *buffer, size_t offset,
                             size_t len) {
    return sdcard_disk_rw_lba(offset, (char *)buffer, len, 1);
}

static int sdcard_disk_read(file_t *file, char *buffer, size_t offset,
                            size_t len) {
    return sdcard_disk_rw_lba(offset, buffer, len, 0);
}

static inode_ops_t inode_ops = {
    .link = NULL, .lookup = NULL, .mkdir = NULL, .rmdir = NULL, .unlink = NULL};

static file_ops_t file_ops = {
    .flush  = NULL,
    .mmap   = NULL,
    .munmap = NULL,
    .write  = sdcard_disk_write,
    .read   = sdcard_disk_read,
    .open   = NULL,
    .close  = NULL,
    .seek   = NULL,
};

// MFTT-Driver Framework

#include <dev/dev.h>
#include <driver/console.h>
#include <memory.h>
#include <riscv.h>
#include <trap.h>

int init_sdcard(dev_driver_t *drv) {
    kprintf("[SDCARD] SDCARD Start initialize.\n");
    sdcard_init();
    // Setup vfs
    inode_t *inode   = vfs_alloc_inode(NULL);
    inode->i_f_op    = &file_ops;
    inode->i_op      = &inode_ops;
    inode->i_dev[0]  = DEV_VIRTIO_DISK;
    inode->i_dev[1]  = 1;
    inode->i_fs_data = (void *)sdcard_disk_rw_lba;

    vfs_link_inode(inode, vfs_get_dentry("/dev", NULL), "raw_sda");

    kprintf("[SDCARD] SDCARD Finish initialize.\n");
    return 0;
}

dev_driver_t sdcard_driver = {
    .name = "sdcard",
    .init = init_sdcard,
#ifdef PLATFORM_QEMU
    .loading_sequence = 0xFF,
#else
    .loading_sequence = 6,
#endif
    .dev_id       = DEV_SDCARD_DISK,
    .major_ver    = 0,
    .minor_ver    = 1,
    .private_data = NULL,
    .list         = LIST_HEAD_INIT(sdcard_driver.list),
};

ADD_DEV_DRIVER(sdcard_driver);
