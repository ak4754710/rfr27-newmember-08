# Firmware design

- `flowchart.md`: Mermaid flowchart for scanning 16 temperature-sensitive diodes through the CD74HC4067 multiplexer, converting readings, checking the 60°C fault threshold, and reporting over I2C.
- `firmware_outline.h`: C function declarations for the planned firmware modules; no implementation.

The exact voltage-to-temperature conversion depends on the hardware signal-conditioning design. The project specification inconsistently describes I2C output as either the maximum temperature or all 16 temperatures; the interface remains to be confirmed.
