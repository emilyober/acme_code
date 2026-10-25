# ACME

Development project using the Panasonic PAN1770 BLE development board based on the Nordic nRF52840.

## Current Goals

The overall goal is to build one embedded application that:

- reads temperature from a TMP117 sensor over I2C
- stores temperature samples in external SPI flash memory
- sends temperature samples over Bluetooth Low Energy
- allows SPI and I2C signals to be observed on an oscilloscope
- allows the sampling interval to be configured
- compares the power required to transmit data over BLE versus storing data in flash

## Current Hardware

- Panasonic PAN1770 evaluation board
- Keysight DSOX3024G oscilloscope
- TMP117 temperature sensor: not connected yet
- external SPI flash memory: not connected yet

## Project Folders

### `test_ble`

This project started from the Zephyr blinky sample and was modified to test Bluetooth Low Energy communication.

Current functionality:

- PAN1770 advertises over BLE
- device can be detected and connected to using nRF Connect on a phone
- custom BLE GATT service is used
- temperature-style test data can be sent using BLE notifications
- verified transmission of:

```text
TEMP=23.50
```

This confirms that the PAN1770 can communicate with a phone over Bluetooth Low Energy.

### `pan1770_spi_test`

This project is used to test SPI output and the combined BLE/SPI application.

Current functionality:

- SPI3 enabled on the PAN1770
- SPI clock output on Arduino header D13
- SPI MOSI output on Arduino header D11
- test SPI pattern transmitted:

```text
AA 55 AA 55
```

- SPI SCK and MOSI signals verified using an oscilloscope
- fake temperature samples can be generated
- SPI activity and BLE temperature transmission can run from the same program
- sample interval can be changed in software

Example test flow:

```text
Fake temperature
      |
      v
Generate SPI transaction
      |
      v
Send temperature over BLE
      |
      v
Wait for sample interval
      |
      v
Repeat
```

## Verified Results

### BLE

The PAN1770 was detected from a phone using nRF Connect.

A BLE notification containing:

```text
TEMP=23.50
```

was successfully received.

### SPI

SPI output was measured using the Keysight oscilloscope.

Connections used:

```text
PAN1770 D13 -> Oscilloscope CH1 -> SPI SCK
PAN1770 D11 -> Oscilloscope CH2 -> SPI MOSI
PAN1770 GND -> Oscilloscope ground
```

The oscilloscope showed both SPI clock and data activity.

## Next Steps

1. Add TMP117 temperature sensor over I2C.
2. Read the TMP117 temperature register.
3. Convert the raw 16-bit temperature value to degrees Celsius.
4. Program TMP117 high and low temperature limits.
5. Connect and test the TMP117 ALERT pin.
6. Observe I2C SDA and SCL on the oscilloscope.
7. Add external SPI flash memory.
8. Store temperature samples in flash.
9. Compare energy used for BLE transmission versus SPI flash storage.
10. Combine TMP117, SPI flash, BLE, and sampling timing into one final application.

## Final Intended Architecture

```text
TMP117
  |
  | I2C
  v
PAN1770
  |
  +----> SPI flash
  |
  +----> BLE ----> phone/laptop
```

The final firmware will periodically read temperature, store the sample in flash, transmit it over BLE, then wait for the configured sampling interval.