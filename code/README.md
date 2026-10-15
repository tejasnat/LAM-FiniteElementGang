# Code

This folder contains the ESP32 Arduino code used for the Lam Research Challenge 3.0 Stage 2 rig by **Finite Element Gang (Team ID: LRC-26-0388)**.

## Master automation

**Path:** `master/master_state_machine.ino`

Controls the complete Stage 2 sequence:
- Case 1 process-fluid recirculation
- Pump-01 at 50% PWM and Pump-02 at 100% PWM
- Sump surge detection using the float switch
- Solenoid-valve control
- FT-01 pulse acquisition and flow telemetry
- Transition from Case 1 to Case 2
- Case 2 flushing with all three pumps
- Automatic shutdown logic
- Emergency stop and serial diagnostics

## Flow-sensor calibration

**Path:** `calibration/flow_sensor_calibration.ino`

Dedicated FT-01 calibration program used for the Task 2 experiments. It:
- Runs Pump-02 for a 30-second calibration trial
- Counts FT-01 pulses
- Calculates pulse frequency using `f = N/t`
- Provides the data needed to compare sensor output with collected water volume and actual flow rate
