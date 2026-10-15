# Firmware

This folder contains two independent ESP32 Arduino codebases for the Stage 2 rig.

## 1. Master automation code

Path: `master/master_state_machine.ino`

Runs the complete Case 1 -> transition -> Case 2 sequence, including:
- Pump PWM control
- Solenoid control
- Sump float-switch protection
- FT-01 pulse counting
- Flow telemetry
- Case 2 low-flow shutdown logic
- Emergency stop

> The calibration constants in the master file are marked as provisional. Replace them with the final experimentally accepted values before the final demonstration.

## 2. FT-01 calibration code

Path: `calibration/flow_sensor_calibration.ino`

A dedicated utility for Q2.2/Q2.3. It:
- Runs Pump-02 for calibration
- Counts FT-01 pulses for a fixed 30 s trial
- Accepts the manually collected water volume
- Calculates pulse frequency and actual flow
- Stores multiple experimental trials
- Calculates the least-squares relation `Q = a*f + b`
- Calculates `R^2`, average error, and maximum error

Use at least five **real measured** operating conditions. Do not enter an analytical/interpolated point as an experimental calibration trial.
