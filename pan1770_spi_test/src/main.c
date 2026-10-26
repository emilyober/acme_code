#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/i2c.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>

#include <zephyr/sys/printk.h>

#include <stdio.h>
#include <string.h>

/*settings*/
#define DEVICE_NAME "PAN1770_ACME"
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

/*control sampling speed */
#define SAMPLE_INTERVAL_MS 100

/*SPI*/
#define SPI_NODE DT_NODELABEL(spi3)

/*I2C*/
#define I2C_NODE DT_NODELABEL(i2c0)

static const struct device *i2c_dev =DEVICE_DT_GET(I2C_NODE);

#define TMP117_ADDR        0x48

#define TMP117_TEMP_REG    0x00
#define TMP117_CONFIG_REG  0x01
#define TMP117_HIGH_REG    0x02
#define TMP117_LOW_REG     0x03

static const struct device *spi_dev =
    DEVICE_DT_GET(SPI_NODE);

static const struct spi_config spi_cfg = {
    .frequency = 125000,
    .operation = SPI_WORD_SET(8) | SPI_TRANSFER_MSB,
    .slave = 0,
    .cs = NULL,
};

/*SPI test pattern */
static uint8_t spi_data[] = {
    0xAA,
    0x55,
    0xAA,
    0x55
};

static struct spi_buf spi_tx_buf = {
    .buf = spi_data,
    .len = sizeof(spi_data),
};

static struct spi_buf_set spi_tx = {
    .buffers = &spi_tx_buf,
    .count = 1,
};

/*BLE UUIDs*/

#define BT_UUID_TEMP_SERVICE_VAL \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, \
                       0x1234, 0x56789abcdef0)

#define BT_UUID_TEMP_CHAR_VAL \
    BT_UUID_128_ENCODE(0x12345679, 0x1234, 0x5678, \
                       0x1234, 0x56789abcdef0)

static struct bt_uuid_128 temp_service_uuid =
    BT_UUID_INIT_128(BT_UUID_TEMP_SERVICE_VAL);

static struct bt_uuid_128 temp_char_uuid =
    BT_UUID_INIT_128(BT_UUID_TEMP_CHAR_VAL);

static bool notifications_enabled = false;

/*BLE notification subscription*/

static void temp_ccc_changed(
    const struct bt_gatt_attr *attr,
    uint16_t value)
{
    notifications_enabled =
        (value == BT_GATT_CCC_NOTIFY);

    if (notifications_enabled) {
        printk("BLE notifications enabled\n");
    } else {
        printk("BLE notifications disabled\n");
    }
}

/*BLE service*/

BT_GATT_SERVICE_DEFINE(
    temp_service,

    BT_GATT_PRIMARY_SERVICE(
        &temp_service_uuid
    ),

    BT_GATT_CHARACTERISTIC(
        &temp_char_uuid.uuid,

        BT_GATT_CHRC_NOTIFY,

        BT_GATT_PERM_NONE,

        NULL,
        NULL,
        NULL
    ),

    BT_GATT_CCC(
        temp_ccc_changed,
        BT_GATT_PERM_READ |
        BT_GATT_PERM_WRITE
    )
);

/*advertising data*/

static const struct bt_data ad[] = {

    BT_DATA_BYTES(
        BT_DATA_FLAGS,
        BT_LE_AD_GENERAL |
        BT_LE_AD_NO_BREDR
    ),

    BT_DATA(
        BT_DATA_NAME_COMPLETE,
        DEVICE_NAME,
        DEVICE_NAME_LEN
    ),
};

/*fake TMP117 function. later this will actually read I2C.*/

static float read_temperature(void)
{
    static float fake_temp = 23.50f;

    fake_temp += 0.10f;

    if (fake_temp > 30.0f) {
        fake_temp = 23.50f;
    }

    return fake_temp;
}

/*real TMP117 data to temp conversion*/
static float tmp117_read_temperature(void)
{
    int16_t raw;

    int err = tmp117_read_raw(&raw);

    if (err) {
        printk("TMP117 read failed: %d\n", err);

        /*fake value while sensor is absent */
        return 23.50f;
    }

    return raw * 0.0078125f;
}

/*real direct TMP117 read*/
static int tmp117_read_raw(int16_t *raw)
{
    uint8_t reg = TMP117_TEMP_REG;
    uint8_t data[2];

    int err = i2c_write_read(
        i2c_dev,
        TMP117_ADDR,
        &reg,
        1,
        data,
        2
    );

    if (err) {
        return err;
    }

    *raw = (int16_t)(
        ((uint16_t)data[0] << 8) |
        data[1]
    );

    return 0;
}

/*SPI "flash" test
 * Right now no flash chip exists, so SPI traffic is generated.*/

static void fake_flash_store(void)
{
    int err;

    err = spi_write(
        spi_dev,
        &spi_cfg,
        &spi_tx
    );

    if (err) {
        printk(
            "SPI write failed: %d\n",
            err
        );
    } else {
        printk(
            "SPI sent: AA 55 AA 55\n"
        );
    }
}

/*BLE temperature transmission*/

static void ble_send_temperature(float temperature)
{
    char message[32];
    int whole = (int)temperature;

    int fraction =
        (int)((temperature - whole) * 100.0f);

    snprintf(
        message,
        sizeof(message),
        "TEMP=%d.%02d",
        whole,
        fraction
    );

    printk(
        "Temperature: %s C\n",
        message
    );

    if (notifications_enabled) {

        int err = bt_gatt_notify(
            NULL,

            &temp_service.attrs[2],

            message,

            strlen(message)
        );

        if (err) {
            printk(
                "BLE notify failed: %d\n",
                err
            );
        } else {
            printk(
                "BLE sent: %s\n",
                message
            );
        }
    }
}

/*main*/

int main(void)
{
    int err;

    printk("\n");
    printk("ACME PAN1770 demo starting\n");

    /*check SPI*/

    if (!device_is_ready(spi_dev)) {
        printk("SPI3 not ready\n");
        return 0;
    }
    if (!device_is_ready(i2c_dev)) {
        printk("I2C0 not ready\n");
        return 0;
    }

    printk("I2C0 ready\n");

    printk("SPI3 ready\n");

    /*start Bluetooth*/

    err = bt_enable(NULL);

    if (err) {
        printk(
            "Bluetooth init failed: %d\n",
            err
        );

        return 0;
    }

    printk("Bluetooth ready\n");

    /*start advertising*/

    err = bt_le_adv_start(
        BT_LE_ADV_CONN_FAST_1,
        ad,
        ARRAY_SIZE(ad),
        NULL,
        0
    );

    if (err) {
        printk(
            "Advertising failed: %d\n",
            err
        );

        return 0;
    }

    printk(
        "Advertising as %s\n",
        DEVICE_NAME
    );

    /*main sampling loop*/

    while (1) {

        /*read temperature
         * fake- later TMP117 I2C.
         */

        float temperature =
            tmp117_read_temperature();

        /*"store" using SPI
         *
         * currently AA 55 pattern- later actual flash memory.
         */

        fake_flash_store();

        /*send temperature over BLE.
         */

        ble_send_temperature(
            temperature
        );

        /*wait before next sample.
         */

        k_sleep(
            K_MSEC(
                SAMPLE_INTERVAL_MS
            )
        );
    }

    return 0;
}