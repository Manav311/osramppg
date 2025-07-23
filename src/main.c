#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(as705x, LOG_LEVEL_DBG);

// Replace this with the actual I2C bus your device is connected to
#define I2C_DEV DT_LABEL(DT_NODELABEL(i2c21))


const struct device *i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c21));


#define AS7056_I2C_ADDR 0x55

#define REG_APP_ID          0x00
#define REG_APP_VER         0x01
#define REG_SEQ_CFG         0x10
#define REG_LED_SEQ1_SUB12  0x11
#define REG_PD_SEQ1_SUB12   0x13
#define REG_SEQ1_LED1_CURR  0x15
#define REG_SEQ1_LED2_CURR  0x16
#define REG_MEAS_CTRL       0x20
#define REG_FIFO_LEVEL0     0x28
#define REG_FIFO_LEVEL1     0x29
#define REG_FIFO_DATA       0x2A
#define REG_LED_CTRL        0x03
#define REG_LED1_CURR       0x04
#define REG_SEQ_STOP        0x0C


int write_reg(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = { reg, val };
    return i2c_write(i2c_dev, buf, 2, AS7056_I2C_ADDR);
}

int read_reg(uint8_t reg, uint8_t *val) {
    return i2c_write_read(i2c_dev, AS7056_I2C_ADDR, &reg, 1, val, 1);
}

int read_fifo_samples(uint8_t *buf, size_t len) {
    uint8_t reg = REG_FIFO_DATA;
    return i2c_write_read(i2c_dev, AS7056_I2C_ADDR, &reg, 1, buf, len);
}

void main(void)
{
    if (!device_is_ready(i2c_dev)) {
        printk("I2C not ready\n");
        return;
    }

    printk("AS7056 configured, starting measurement...\n");

    // Stop sequencer if running
    write_reg(REG_SEQ_STOP, 0x01);
    k_msleep(50);

    // Sequencer configuration
    write_reg(REG_SEQ_CFG, 0x01);         // Enable sequencer
    write_reg(REG_LED_SEQ1_SUB12, 0x12);  // LED1 sub1, LED2 sub2
    write_reg(REG_PD_SEQ1_SUB12, 0x22);   // PD2 for both sub1 and sub2
    write_reg(REG_SEQ1_LED1_CURR, 0x3F);  // Max current
    write_reg(REG_SEQ1_LED2_CURR, 0x3F);  // Max current

    write_reg(REG_MEAS_CTRL, 0x01);       // Start sequencer
    k_msleep(100);

    uint32_t no_sample_time_ms = 0;
    bool test_mode_triggered = false;

    while (1) {
        uint8_t level_buf[2];
        if (i2c_write_read(i2c_dev, AS7056_I2C_ADDR,
                           (uint8_t[]){REG_FIFO_LEVEL0}, 1,
                           level_buf, 2) != 0) {
            printk("FIFO level read error\n");
            k_msleep(100);
            continue;
        }
        uint16_t fifo_level = level_buf[0] | ((uint16_t)level_buf[1] << 8);

        if (fifo_level < 3) {
            no_sample_time_ms += 10;
            if (!test_mode_triggered && no_sample_time_ms >= 5000) {
                printk("[!] No data received — falling back to LED test mode (LED1 ON constantly)\n");

                write_reg(REG_SEQ_STOP, 0x01);
                k_msleep(50);
                write_reg(REG_LED_CTRL, 0x01);   // Enable LED1 constant mode
                write_reg(REG_LED1_CURR, 0x3F);  // 100% current
                test_mode_triggered = true;
            }
            k_msleep(10);
            continue;
        }

        no_sample_time_ms = 0;

        while (fifo_level >= 3) {
            uint8_t sample_bytes[3];
            if (read_fifo_samples(sample_bytes, 3) != 0) {
                printk("FIFO read error\n");
                break;
            }

            uint32_t raw_sample = sample_bytes[0]
                                 | ((uint32_t)sample_bytes[1] << 8)
                                 | ((uint32_t)sample_bytes[2] << 16);

            uint8_t marker = (raw_sample >> 21) & 0x07;
            int32_t measurement = (int32_t)(raw_sample & 0x1FFFFF);
            if (measurement & 0x100000) {
                measurement |= ~0x1FFFFF;
            }

            static uint8_t sub_index = 1;
            if (marker == 0) sub_index = 1;
            else sub_index += 1;

            printk("LED%u: %d\n", sub_index, measurement);

            fifo_level -= 3;
        }

        k_msleep(10);
    }
}
