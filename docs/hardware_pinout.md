# Bio-Mouse Bench Wiring & Voltage Divider Guide

## 1. 5V-to-3.3V UART Voltage Divider Schematic
The Arduino Uno `D1 (TX)` pin outputs `5.0V` logic, while the ESP32-CAM `U0R (GPIO 3)` pin is rated for `3.3V` logic. Always wire the `1kΩ` and `2kΩ` resistors as shown below before connecting `D1` to `U0R`:

```text
Arduino Pin 1 (TX) [5.0V] ----+---- [ 1kΩ Resistor ] ----+----> ESP32-CAM U0R (GPIO 3) [3.3V]
                                                         |
                                                  [ 2kΩ Resistor ]
                                                         |
Common Ground Rail (GND)  -------------------------------+----> ESP32-CAM GND
