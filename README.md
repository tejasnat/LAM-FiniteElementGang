# LAM Research Challenge 3.0 — Stage 2

## Finite Element Gang

**Team ID:** LRC-26-0388  
**Challenge:** Lam Research Challenge 3.0  
**Stage:** Stage 2 — Practical Engineering Challenge

This repository contains the final Stage 2 documentation and ESP32 code developed by **Finite Element Gang** for the automated liquid-flow system.

The project demonstrates two operating cases:

- **Case 1 — Control + Measurement:** closed-loop process-fluid circulation, FT-01 flow measurement and automatic sump-surge protection.
- **Case 2 — Flushing + Shutdown:** flush-water operation using all three pumps, isolation of the main process tank and automated system shutdown.

## Repository Contents

### `documentation/`

Contains the completed Stage 2 challenge submission:

- **`LRC_26_0388_Finite_Element_Gang_Stage_2.pdf`** — completed challenge document containing the hydraulic-system design, P&ID, calculations, experimental measurements, calibration results, flow-response plots, hydraulic theory, fault analysis and submitted ESP32 code.

### `code/`

Contains the two ESP32 Arduino codebases used for the project:

- **`code/master/master_state_machine.ino`** — main automation program for the complete Case 1 → transition → Case 2 operating sequence.
- **`code/calibration/flow_sensor_calibration.ino`** — dedicated FT-01 calibration program used to record pulse count and frequency during controlled calibration trials.
- **`code/README.md`** — short description of the two programs and their purpose.

## Current Repository Structure

```text
LAM-FiniteElementGang/
├── README.md
├── code/
│   ├── README.md
│   ├── master/
│   │   └── master_state_machine.ino
│   └── calibration/
│       └── flow_sensor_calibration.ino
└── documentation/
    └── LRC_26_0388_Finite_Element_Gang_Stage_2.pdf
```

---

**Team:** Finite Element Gang  
**Team ID:** LRC-26-0388
