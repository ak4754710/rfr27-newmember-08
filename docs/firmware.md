# Firmware

The firmware targets an ATmega328P Arduino Nano and continuously scans the
eight physical channels of the CD4051BE multiplexer. The submitted schematic
still shows the original sixteen-channel CD74HC4067, as required by the
project leads; the assembled firmware target is the eight-channel circuit.

## Provisional hardware configuration

The current schematic does not resolve every firmware pin assignment. The
sketch therefore uses these documented bring-up defaults:

| Function | Pin/value |
| --- | --- |
| MUX S0/S1/S2 | D2/D3/D4 |
| MUX inhibit | Externally held LOW; not driven by firmware |
| ADC input | A0 |
| Fault LED/MOSFET gate | D6, active HIGH |
| ADC reference | Arduino default reference, nominally 5.0 V |
| I2C peripheral address | `0x08` |
| Serial | 115200 baud |

The D6 assignment is provisional because older schematic notes assign D6 to
both MUX enable and LED. Verify the assembled wiring before relying on the
fault indicator.

## Measurement model

Each channel waits 2 ms after selection, then reads the 10-bit ADC. The
conversion uses the nominal reference and the provisional differential
amplifier model:

```text
Vconditioned = ADC * 5.0 / 1023
Vsensor = 2.5 - Vconditioned / 3.9
```

The sensor voltage is converted with piecewise-linear interpolation through
the documented -40°C to 120°C voltage table. Values outside 1.30 V to 2.44 V
are invalid; they are not extrapolated or clamped. The model is inferred from
the documented endpoints and is not calibrated. Higher conditioned voltage
means higher temperature under this model.

For the intended RC values, 1 kOhm and 150 nF give a 150 us time constant and
a cutoff near 1.06 kHz. If the assembled board uses 100 nF instead, the
cutoff is near 1.59 kHz. The 2 ms delay is a conservative development
assumption for the complete analog path; it is not a measured settling or
scan-rate guarantee.

## Fault behavior and diagnostics

The maximum valid temperature and its lowest-index channel are retained.
Exactly 60°C is not an overtemperature fault; values above 60°C are. Any
invalid channel also activates the blinking fault LED. If all channels are
invalid, no maximum is reported. Before the first completed scan, the LED is
off and data is marked not ready.

The serial monitor uses 115200 baud. About every 500 ms it prints the scan
sequence, status, maximum/channel, and each channel's raw ADC count and
temperature. Use the Arduino IDE or Arduino CLI to select **Arduino Nano,
ATmega328P**, upload `software/firmware.ino`, and open the serial monitor at
115200 baud. The board and bootloader selection still need to be confirmed
during bring-up.

## I2C response

The Nano is a read-only I2C peripheral at provisional address `0x08`. A
master request returns exactly 24 bytes:

| Bytes | Meaning |
| --- | --- |
| 0 | Protocol version, `1` |
| 1 | Status: bit 0 data ready, bit 1 overtemperature, bit 2 invalid channel, bit 3 no valid maximum |
| 2 | Invalid-channel bitmask, bit n is channel n |
| 3 | Hottest channel, or `0xFF` when unavailable |
| 4-5 | Scan sequence, unsigned 16-bit little-endian |
| 6-7 | Maximum temperature, signed 16-bit hundredths of °C, little-endian |
| 8-23 | Eight signed 16-bit hundredths-of-°C temperatures, little-endian |

`INT16_MIN` (`0x8000`) means unavailable or invalid. Temperatures are rounded
to the nearest hundredth before encoding. The request callback only returns
the already-published packet; ADC work and serial output remain in the main
loop. The packet format is provisional and must be checked against the
receiver's expected protocol.

## Validation status

The sketch was compiled with `g++ -std=c++17 -Wall -Wextra -Werror` against a
temporary Arduino/Wire stub harness. That harness checked channel bounds, ADC
boundary scaling, interpolation endpoints and representative points,
out-of-range invalidation, maximum/tie handling, threshold strictness, packet
length and byte order, the unavailable sentinel, and LED timing. The actual
Arduino toolchain was unavailable in the development environment, so an
ATmega328P Nano compile and upload still need to be performed.

Physical validation is still required for MUX pin mapping, amplifier polarity
and gain, ADC reference voltage, RC settling, sensor calibration, MOSFET LED
polarity, I2C pull-ups, and receiver compatibility.
