# Open Field Logger Lab

**Open Field Logger Lab** is a low-cost educational data logger from
[Leeman Geophysical](https://leemangeophysical.com/) for learning basic field data logging with
0-5 V analog sensors.

It is designed for students to assemble, test, and use during field measurement projects for
our [GEARS workshops](https://leemangeophysicalllc.github.io/GEARS/) and to then have a useful
tool after the course. The goal is to teach practical instrumentation concepts without turning
the logger itself into the main project.

## Purpose

This project is intended to help students learn:

- through-hole soldering
- sensor wiring
- analog voltage measurement
- calibration
- sampling rate tradeoffs
- SD card data logging
- basic field deployment practices
- the effect of real-world conditions on measurements

The logger is intentionally simple, low-cost, and easy to modify.

## What this is not

Open Field Logger Lab is **not** a precision commercial data logger.
It is not intended for safety-critical measurements, unattended long-term monitoring,
harsh environmental deployment, or calibrated scientific measurements without
additional validation.

It is a teaching platform for learning how field logging works.

## Hardware overview

Rev A is built around:

- USB-C ESP32 DevKit-style microcontroller module
- ADS1115 16-bit ADC
- four nominal 0-5 V analog single ended inputs
- microSD card logging
- battery-backed real-time clock
- 9-18 VDC input power
- 5 V sensor power output
- simple status LEDs
- optional onboard NTC temperature divider

The analog inputs are intended for simple voltage-output sensors, signal conditioning circuits,
and student-built measurement experiments.

## Student assembly philosophy

Students solder only through-hole parts. Surface-mount parts are assembled before the
workshop for time concerns. We do, however, have a surface mount soldering challenge
for those that want to try SMT!

The through-hole assembly includes parts such as connectors, fuse clips, power components,
sockets, and selected educational components.

## Repository contents

Planned contents:

```text
hardware/      KiCad design files, schematics, PCB layout, fabrication outputs, BOM
firmware/      ESP32 firmware for logging and self-test
docs/          Datasheets and other relevant files as well as assembly instructions
examples/      Example CSV files and simple analysis notebooks/scripts
