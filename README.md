# Lunt

Lunt is a battery-style environmental sensor node. It measures temperature, humidity and pressure, shares them over Bluetooth LE, and can be updated safely in the field.

## Requirements

Lunt must:

1. Read the BME280 every 10 s, and immediately when Button 0 is pressed.
2. Show readings on the serial log and over BLE (standard Environmental Sensing Service, readable in any phone app).
3. Accept a new firmware image over USB serial and over BLE from a phone.
4. Keep a new image only if it passes a self-test on first boot. Otherwise, go back to the previous image automatically.
5. Have a low-power build that can be measured.

## Out of scope

Cloud connection, cellular, a custom PCB, production signing keys, and battery hardware. These are noted as next steps, not built.

## Hardware and tools

| Item | Use |
| --- | --- |
| nRF54L15 DK | Target board (Cortex-M33, BLE radio, RRAM) |
| BME280 breakout | Sensor on I2C: SCL `P1.11`, SDA `P1.12`, VDD set to 3.3 V |
| AuTerm | Firmware upload over USB serial |
| Phone: nRF Connect for Mobile and Device Manager | Read BLE data, firmware upload over BLE |
| Power Profiler Kit II *(optional)* | Measure current |
