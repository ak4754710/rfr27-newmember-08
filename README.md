# RFR Temperature Scanner

This repository contains the hardware design and firmware workspace for the
RFR temperature scanner.

## Current status

The current hardware design is an 8-channel scanner built around a CD4051BE
analog multiplexer. The firmware now implements the scanner, temperature
conversion, fault indication, serial diagnostics, and a provisional I2C
response; physical bring-up and calibration remain outstanding.

## Repository layout

- [`hardware/`](hardware/) - active Altium schematic source and the exported
  schematic PDF.
- [`software/`](software/) - firmware placeholders and the firmware outline.
- [`docs/`](docs/) - the current design report and firmware flowchart.
- [`archive/`](archive/) - historical Altium revisions, generated exports,
  project logs, previews, and superseded placeholder notes.

The active Altium project file is [`Temp_Board.PrjPcb`](Temp_Board.PrjPcb).
Its schematic path points to
[`hardware/Temp_Board/Main.SchDoc`](hardware/Temp_Board/Main.SchDoc).

## Documentation

- [`docs/New Member Project Design Report.docx`](docs/New%20Member%20Project%20Design%20Report.docx) -
  current editable design report.
- [`docs/software-flowchart.md`](docs/software-flowchart.md) - firmware
  planning flowchart.
- [`docs/firmware.md`](docs/firmware.md) - firmware configuration,
  calculations, protocol, and bring-up notes.

The flowchart and [`software/firmware_outline.h`](software/firmware_outline.h)
describe the current 8-channel scanner.
The previously exported PDF is retained under
[`archive/generated/`](archive/generated/) because it predates the 8-channel
updates; the editable report is the current source document.

## Firmware

[`software/firmware.ino`](software/firmware.ino) implements the eight-channel
scan, temperature conversion, fault LED, serial diagnostics, and provisional
I2C response described in [`docs/firmware.md`](docs/firmware.md).

## Development

Firmware work will be developed on a separate branch after the repository
cleanup. The archived files contain superseded revisions and generated
artifacts; the active files above are the current project references.
