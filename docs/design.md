# FiberCheck design notes

## Requirements

- 1310 nm source, CW or 270 Hz / 1 kHz / 2 kHz tone; drift under 0.1 dB over 1 hour after warm-up
- Meter from +3 dBm to -70 dBm, calibrated at 1310, 1490 and 1550 nm
- Accuracy ±0.2 dB after calibration, 0.01 dB resolution
- Insertion loss in dB against a stored reference
- Tone detection for fibre identification
- USB logging; Ethernet reporting and alarms in monitor mode
- Battery powered, at least 8 hours

## Architecture: alternatives considered

| Option | Strength | Why it lost (or won) |
| --- | --- | --- |
| **A. 5-range switched-gain TIA + MCU ADC (chosen)** | Linear on every range, so the tone keeps its shape; each range is one precise resistor that is easy to calibrate | Needs range switching with hysteresis and a low-leakage switch |
| B. Logarithmic amplifier | One range covers every decade | Log output distorts the tone, so Goertzel cannot measure it directly; conformance and temperature drift cost accuracy |
| C. Single-gain TIA + 24-bit delta-sigma ADC | Very high resolution | High-resolution modes are too slow for a 2 kHz tone; one small gain resistor buries nA signals in noise |

DSP load: three Goertzel tones at 32 kSPS are about 100,000 multiply-adds per second,
under 1 % of a 170 MHz Cortex-M4F.

## Analog front end

- **Photodiode:** InGaAs PIN (silicon is blind above about 1100 nm), about 0.9 to 0.95 A/W.
  Zero-bias (photovoltaic) mode for near-zero dark current.
- **Op-amp:** electrometer-grade CMOS (femtoamp bias current, e.g. LMP7721). At -70 dBm
  the photocurrent is only about 90 pA.
- **Stability:** the photodiode capacitance Cd needs a feedback capacitor of at least
  Cf ≥ sqrt(Cd / (2π · Rf · GBW)). Chosen so that Rf · Cf = 10 µs on every range
  (16 kHz bandwidth).
- **Range switching:** all five Rf stay connected to the summing node; the analog switch
  sits at their far end inside the feedback loop, and the ADC senses after the switch.
  The switch's on-resistance drops out, and its leakage flows into the low-impedance output.
- **Single supply:** the TIA output idles at a 100 mV reference derived from the 3.0 V ADC reference.

## Laser source

- Pigtailed 1310 nm Fabry-Perot laser with a built-in monitor photodiode, about -5 dBm into the fibre.
- Automatic power control: an integrator holds the monitor current at a DAC setpoint.
  Loop bandwidth is about 20 Hz, more than 10× below the lowest tone, so it regulates
  the average power. In 50 % tone mode the setpoint is halved, so the on-level matches CW.
- Hardware current limit, soft start, ESD protection.

## PCB (4 layers, KiCad)

| Layer | Use |
| --- | --- |
| L1 | Analog front end, laser driver, components |
| L2 | Solid ground |
| L3 | 3.3 V analog and 3.3 V digital pours |
| L4 | Digital routing |

- A guard ring around the TIA input, driven at the 100 mV reference
- Rf and Cf placed at the op-amp pins; Cf is C0G
- Flux washed after assembly
- Shield can over the front end
- Analog and digital sections meet the ground plane at one point

## Power

| Rail | Source |
| --- | --- |
| 3.3 V digital | Buck-boost from the 1-cell Li-ion battery |
| 3.3 V analog | Low-noise LDO after the buck-boost |
| 3.0 V reference | Precision reference: ADC full scale and the TIA's 100 mV reference |

## Test plan

| Step | Setup | Pass criterion |
| --- | --- | --- |
| 1. Power bring-up | Current-limited supply | Rails within ±3 % |
| 2. Electrical calibration | Known current injected into the TIA (e.g. 1.000 V across 100 MΩ = 10 nA), no light | Gain per range stored, repeatable to 0.01 dB |
| 3. Range linearity | Variable attenuator in 1 dB steps vs a reference meter | Within ±0.2 dB; under 0.05 dB jump at range switches |
| 4. Wavelength calibration | Reference meter at 1310 and 1550 nm | Responsivity factor stored |
| 5. Source stability | 1-hour log after 5-min warm-up | Drift under 0.1 dB |
| 6. Component tests | Patch cord, 1:2 coupler, 1:8 splitter (one-jumper reference) | At or below datasheet maximum |
| 7. Fibre identification | 2 kHz tone into one fibre of a bundle | Correct fibre identified |
| 8. Field validation | GPON drop at the subscriber end; 8-hour monitor mode | Level inside the ONT receive window; alarm on a 3 dB drop |

Insertion loss: IL (dB) = P_ref (dBm) − P_DUT (dBm).
