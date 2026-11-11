# Firmware

The sketch targets the ATmega328P Arduino Nano. It scans the eight channels of
the CD4051BE on the breadboard. The submitted schematic uses the original
16-channel multiplexer.

| Function | Setting |
| --- | --- |
| MUX S0/S1/S2 | D2/D3/D4 |
| MUX inhibit | Held LOW externally |
| ADC input | A0 |
| Fault LED | D6, active HIGH |
| ADC reference | Arduino DEFAULT, configured as 4.84 V for the bench rail |
| I2C peripheral address | 0x08 |
| USB serial | 115200 baud |

## Scan and conversion

The sketch selects each channel, waits 2 ms, and takes one 10-bit ADC reading.
Set the configured reference voltage to the measured rail used by the Nano.

```text
VA0 = ADC count × AREF / 1023
Vdiode = 0.43824 × AREF − 0.28735 × VA0
```

The second equation comes from the design report's signal-conditioning model.
The sketch interpolates the sensor's voltage-to-temperature table. A diode
voltage outside 1.30–2.44 V is invalid and is skipped when finding the maximum.
The lowest channel index wins ties. D6 blinks with a 250 ms half-period while a valid reading exceeds 60 °C.
USB serial prints the channel number and temperature during each scan.

## I2C reply

A controller read at address 0x08 returns four bytes.

| Bytes | Field |
| --- | --- |
| 0 | Bit 0 is set when the highest valid temperature exceeds 60 °C. Other bits are zero. |
| 1 | Zero-based hottest channel when overtemperature is set, otherwise 0xFF. |
| 2–3 | Highest valid temperature as signed 16-bit hundredths of a degree Celsius, little-endian. 0x8000 means unavailable. |

USB serial reports individual channel temperatures. The I2C controller should
request four bytes. A read that overlaps a scan update can contain bytes from
two consecutive scans.

## Bench verification

Select Arduino Nano and the processor/bootloader option that matches the
board. Confirm all eight mux positions, the configured reference voltage, the
D6 LED wiring, and a four-byte read from a separate I2C controller. The project
progress log records a bench conversion check and lists I2C as untested.
