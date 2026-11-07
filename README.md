# FiberCheck: Low-Cost Optical Loss Test Set

A handheld, microcontroller-based **Optical Loss Test Set (OLTS)**: a 1310 nm laser
source and a 5-range InGaAs optical power meter on one STM32 board. It measures the
insertion loss of fibre components (patch cords, connectors, splitters, couplers) and
fibre links, from **+3 dBm down to -70 dBm**.

The source can blink at the standard fibre-identification tones (270 Hz, 1 kHz,
2 kHz). The meter detects them with the **Goertzel algorithm**. In simulation this
stays within 0.2 dB at -75 dBm, about 15 dB below where a plain DC reading fails.

![Block diagram](docs/block_diagram.svg)

## Project status

| Part | Status |
| --- | --- |
| Firmware DSP (Goertzel, auto-ranging, dBm conversion) | Done, unit-tested (`make test`) |
| Front-end + DSP simulation | Done (`make sim`) |
| Analog front-end design (ranges, stability, guarding) | Done, see [docs/design.md](docs/design.md) |
| Schematic and 4-layer PCB (KiCad) | In progress, see [hardware/](hardware/) |
| Bench prototype (Nucleo-G474RE + 850 nm proof of concept) | Planned |

## How it works

**Analog front end.** A zero-bias InGaAs photodiode feeds a transimpedance amplifier
(TIA) with five decade-spaced gain ranges, read by the STM32G474's ADC with 16x
hardware oversampling.

| Range | Rf | Cf | Full scale (2.8 V) | Optical power at full scale, 1310 nm |
| --- | --- | --- | --- | --- |
| 0 | 10 MΩ | 1 pF | 280 nA | -35 dBm |
| 1 | 1 MΩ | 10 pF | 2.8 µA | -25 dBm |
| 2 | 100 kΩ | 100 pF | 28 µA | -15 dBm |
| 3 | 10 kΩ | 1 nF | 280 µA | -5 dBm |
| 4 | 1 kΩ | 10 nF | 2.8 mA | +5 dBm |

Rf × Cf = 10 µs on every range, so every range is stable and has the same 16 kHz
bandwidth.

**DSP.** 100 ms blocks (N = 3200 at 32 kSPS, 10 Hz bins). All three tones fall exactly
on bin centres, and 50 Hz mains does not leak into them. A 50 % on/off square wave has
fundamental amplitude (2/π)·P, so the firmware reports P = (π/2)·A1.

**Auto-ranging.** Switch to less gain above 2.8 V and to more gain below 0.25 V. The
ranges are one decade apart, so 2.8 V becomes 0.28 V after switching: no chatter.

## Simulation results

`make sim` runs the real firmware functions on a modelled TIA output. The model
includes ±1 mV offset drift, 2 mV of 50 Hz pickup, 150 µV rms noise and a 16-bit ADC
on a 3.0 V reference, with 300 trials per level.

| Power (dBm) | DC method, 95th pct error (dB) | Tone method, 95th pct error (dB) |
| --- | --- | --- |
| -50 | 0.046 | 0.027 |
| -60 | 0.454 | 0.031 |
| -65 | 1.616 | 0.041 |
| -70 | 26.98 | 0.075 |
| -75 | 24.00 | 0.193 |
| -80 | 19.00 | 0.584 |

An auto-range sweep from -75 to +5 dBm and back makes exactly 8 range changes (ideal: 8).

The constant 0.026 dB tone offset comes from sampling the 2 kHz square wave at 16
points per cycle; it is fixed and removed by calibration. The impairment values are
assumptions. Change them in `sim/sim_meter.c` to see where each method breaks.

## Build and run

Needs `gcc` and `make` (Linux, macOS, or WSL on Windows).

```sh
make test   # 9 unit tests for the DSP functions
make sim    # front-end simulation, DC vs tone method
make clean
```

The firmware DSP in `firmware/` is plain C99 with no dependencies. It compiles
unchanged for the STM32G474 (Cortex-M4F) in STM32CubeIDE.

## Repository layout

```
firmware/include/meter_dsp.h   DSP API and auto-range thresholds
firmware/src/meter_dsp.c       Goertzel, DC reading, dBm conversion, auto-range
tests/test_dsp.c               Unit tests
sim/sim_meter.c                Front-end + DSP simulation
docs/design.md                 Design notes: architecture, analog, PCB, test plan
docs/block_diagram.svg         Block diagram
hardware/                      KiCad schematic and PCB (in progress)
```
