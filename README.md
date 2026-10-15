# LAM Research Challenge 3.0 — Stage 2

## Finite Element Gang

**Team ID:** LRC-26-0388  
**Challenge:** Lam Research Challenge 3.0  
**Stage:** Stage 2 — Practical Engineering Challenge

This repository contains the final Stage 2 submission, ESP32 automation code, calibration code and project journey video for **Finite Element Gang**.

The project demonstrates:

- **Case 1 — Control + Measurement:** closed-loop process-fluid circulation, FT-01 flow measurement and automatic sump-surge protection.
- **Case 2 — Flushing + Shutdown:** flush-water operation using all three pumps, isolation of the main process tank and automated system shutdown.

## Project Video

### Journey Video

A short video documenting the team's Stage 2 build journey and the development and integration of the hydraulic, mechanical and control system.

**[Watch the Finite Element Gang — Stage 2 Journey Video](https://photos.app.goo.gl/5rSF9LaYKav16Jqz6)**

## Files

- **`LRC_26_0388_Finite_Element_Gang_Stage_2.pdf`** — completed Stage 2 challenge submission containing the hydraulic-system design, P&ID, calculations, experimental measurements, calibration results, flow-response plots, hydraulic theory, fault analysis and submitted code.
- **`master_state_machine.ino`** — main ESP32 automation program for the complete Case 1 → transition → Case 2 sequence.
- **`flow_sensor_calibration.ino`** — dedicated FT-01 calibration program used for timed pulse-counting and calibration trials.

## Repository Structure

```text
LAM-FiniteElementGang/
├── README.md
├── LRC_26_0388_Finite_Element_Gang_Stage_2.pdf
├── master_state_machine.ino
└── flow_sensor_calibration.ino
```

---

**Team:** Finite Element Gang  
**Team ID:** LRC-26-0388
