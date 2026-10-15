/*
 * ============================================================================
 * Lam Research Challenge 3.0 - Stage 2: Master Dual-Case Automation
 * Team: Finite Element Gang
 * ============================================================================
 * EXECUTION SEQUENCE:
 *   1. Case 1 starts with 's'. Solenoid OPENS, P1=50%, P2=100%, P3=OFF.
 *   2. Sump Surge Test:
 *        - Water tilts float UP (GPIO 35 = LOW).
 *        - Pump-01 instantly trips to OFF (0%) and is LOCKED OFF for 20 SECONDS.
 *        - Pump-02 drains the sump alone at 100% PWM for the full 20 seconds.
 *   3. Case 1 Resumption (10 Seconds):
 *        - After 20s drain, Pump-01 restarts at 50% PWM, Pump-02 stays at 100%
 *          for 10 seconds to prove stable recirculation recovery on camera.
 *   4. 15-Second Transition Countdown:
 *        - Solenoid CLOSES immediately (10L tank isolated). Pumps pause.
 *        - Live countdown in Serial Monitor: Operator opens manual valve BV-03.
 *   5. Case 2 Flush:
 *        - All 3 pumps fire at 100% simultaneously.
 *   6. Flush Tank Cutoff (Volume-Independent Dynamic Detection):
 *        - Detects when flow establishes (> 140 Hz), then triggers when flow
 *          collapses to < 40 Hz for 2 consecutive seconds (Loss of Liquid / Dry-Run).
 *        - All 3 pumps safely shut down to OFF (0%). Zero human touch.
 * ============================================================================
 */

#include <Arduino.h>

// ---------------- HARDWARE PIN ALLOCATIONS ----------------
const int PIN_FLOW_SENSOR  = 14;  // FT-01 Pulse Interrupt
const int PIN_FLOAT_SWITCH = 35;  // LS-01 Sump Float Switch (GPIO 35)
const int PIN_SOLENOID     = 27;  // OV-01 Solenoid via D4184 MOSFET

// Motor Driver U2 (Pump-01: Process Feed)
const int PIN_P1_LPWM = 32;
const int PIN_P1_LEN  = 13;
const int PIN_P1_REN  = 26;

// Motor Driver U3 (Pump-02: Recirculation Return)
const int PIN_P2_LPWM = 18;
const int PIN_P2_LEN  = 16;
const int PIN_P2_REN  = 17;

// Motor Driver U4 (Pump-03: Flush Supply)
const int PIN_P3_LPWM = 23;
const int PIN_P3_LEN  = 21;
const int PIN_P3_REN  = 22;

// ---------------- PWM SETTINGS (ESP32 Core 3.0+) ----------------
const int PWM_FREQ = 5000;
const int PWM_RES  = 8;

const int DUTY_50_PCT  = 128;
const int DUTY_100_PCT = 255;

// ---------------- FLOW CALIBRATION CONSTANTS ----------------
// IMPORTANT: Replace these with the FINAL accepted calibration constants
// after completing the dedicated calibration program.
float cal_a = 0.002678;
float cal_b = 1.4265;
const float PULSES_PER_LITER = 6279.0;

// ---------------- USER CONFIGURABLE THRESHOLDS ----------------
const unsigned long SURGE_STARTUP_IMMUNITY_MS = 3000;
const unsigned long FLOAT_DEBOUNCE_MS         = 500;
const unsigned long SURGE_DRAIN_TIME_MS       = 20000;
const unsigned long RESUME_RUN_TIME_MS        = 10000;
const unsigned long TRANSITION_TIME_MS        = 15000;

// Dynamic Case 2 Flush Cutoff
const unsigned long FLUSH_INITIAL_GRACE_MS       = 10000;
const float         FLUSH_ESTABLISHED_FREQ_HZ    = 140.0;
const float         FLUSH_DRY_CUTOFF_FREQ_HZ     = 40.0;
const int           FLUSH_CUTOFF_CONSECUTIVE_SEC = 2;

// ---------------- STATE MACHINE STATES ----------------
enum SystemState {
  STATE_STANDBY,
  STATE_CASE1_RECIRC,
  STATE_CASE1_SURGE_DRAINING,
  STATE_CASE1_RESUMED,
  STATE_TRANSITION,
  STATE_CASE2_FLUSHING,
  STATE_SYSTEM_SHUTDOWN
};

SystemState currentState = STATE_STANDBY;

// ---------------- RUNTIME VARIABLES ----------------
volatile unsigned long pulseCount = 0;
unsigned long prevPulses = 0;
unsigned long lastTelemetryTime = 0;

unsigned long stateStartTime = 0;
unsigned long surgeDrainStartTime = 0;
unsigned long resumeStartTime = 0;
unsigned long flushStartPulseCount = 0;
unsigned long floatLowStartTime = 0;
bool flushFlowEstablished = false;
int lastCountdownSec = -1;
int flushDropConsecutiveSeconds = 0;

void IRAM_ATTR flowISR() {
  pulseCount++;
}

void setSolenoid(bool open) {
  digitalWrite(PIN_SOLENOID, open ? HIGH : LOW);
}

void setPumpSpeeds(int p1, int p2, int p3) {
  ledcWrite(PIN_P1_LPWM, p1);
  ledcWrite(PIN_P2_LPWM, p2);
  ledcWrite(PIN_P3_LPWM, p3);
}

void emergencyStop() {
  currentState = STATE_SYSTEM_SHUTDOWN;
  setPumpSpeeds(0, 0, 0);
  setSolenoid(false);
  Serial.println("\n[EMERGENCY STOP] >>> ALL ACTUATORS KILLED. SYSTEM SAFE. <<<");
  Serial.println("[EMERGENCY STOP] Type 's' to restart in STANDBY.\n");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(PIN_SOLENOID, OUTPUT);
  digitalWrite(PIN_SOLENOID, LOW);

  pinMode(PIN_FLOAT_SWITCH, INPUT);

  pinMode(PIN_FLOW_SENSOR, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_FLOW_SENSOR), flowISR, FALLING);

  pinMode(PIN_P1_LEN, OUTPUT); digitalWrite(PIN_P1_LEN, HIGH);
  pinMode(PIN_P1_REN, OUTPUT); digitalWrite(PIN_P1_REN, HIGH);
  ledcAttach(PIN_P1_LPWM, PWM_FREQ, PWM_RES);
  ledcWrite(PIN_P1_LPWM, 0);

  pinMode(PIN_P2_LEN, OUTPUT); digitalWrite(PIN_P2_LEN, HIGH);
  pinMode(PIN_P2_REN, OUTPUT); digitalWrite(PIN_P2_REN, HIGH);
  ledcAttach(PIN_P2_LPWM, PWM_FREQ, PWM_RES);
  ledcWrite(PIN_P2_LPWM, 0);

  pinMode(PIN_P3_LEN, OUTPUT); digitalWrite(PIN_P3_LEN, HIGH);
  pinMode(PIN_P3_REN, OUTPUT); digitalWrite(PIN_P3_REN, HIGH);
  ledcAttach(PIN_P3_LPWM, PWM_FREQ, PWM_RES);
  ledcWrite(PIN_P3_LPWM, 0);

  Serial.println("\n=======================================================");
  Serial.println("  LAM Challenge Stage 2 - Master Dual-Case Automation  ");
  Serial.println("  Team: Finite Element Gang                            ");
  Serial.println("=======================================================");
  Serial.println("Rig initialized in STANDBY.");
  int initFS = digitalRead(PIN_FLOAT_SWITCH);
  Serial.printf("  -> Float Switch Pre-Check: %s\n",
    (initFS == LOW) ? "CLOSED / UP! (Warning: Sump water is already high!)" : "OPEN / DOWN (OK - Ready for surge)");
  Serial.println("  -> Type 's' to START CASE 1");
  Serial.println("  -> Type 'x' at any time for EMERGENCY STOP");
  Serial.println("=======================================================\n");
}

void loop() {
  unsigned long now = millis();

  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd.equalsIgnoreCase("s") || cmd.equalsIgnoreCase("start")) {
      if (currentState == STATE_STANDBY || currentState == STATE_SYSTEM_SHUTDOWN) {
        currentState = STATE_CASE1_RECIRC;
        stateStartTime = now;
        floatLowStartTime = 0;
        setSolenoid(true);
        setPumpSpeeds(DUTY_50_PCT, DUTY_100_PCT, 0);
        Serial.println("\n[PHASE 1] CASE 1 STARTED: NORMAL RECIRCULATION LOOP");
        Serial.println("  - OV-01 Solenoid: OPEN");
        Serial.println("  - Pump-01: 50% PWM | Pump-02: 100% PWM | Pump-03: OFF");
        Serial.println("  - Sump surge protection armed (3s startup immunity active)...\n");
      }
    } else if (cmd.equalsIgnoreCase("next") || cmd.equalsIgnoreCase("f")) {
      if (currentState == STATE_CASE1_RECIRC ||
          currentState == STATE_CASE1_SURGE_DRAINING ||
          currentState == STATE_CASE1_RESUMED) {
        currentState = STATE_TRANSITION;
        stateStartTime = now;
        lastCountdownSec = -1;
        setSolenoid(false);
        setPumpSpeeds(0, 0, 0);
      }
    } else if (cmd.equalsIgnoreCase("x") || cmd.equalsIgnoreCase("stop")) {
      emergencyStop();
    }
  }

  switch (currentState) {
    case STATE_STANDBY:
      break;

    case STATE_CASE1_RECIRC: {
      if (now - stateStartTime >= SURGE_STARTUP_IMMUNITY_MS) {
        bool rawFloat = (digitalRead(PIN_FLOAT_SWITCH) == LOW);

        if (rawFloat) {
          if (floatLowStartTime == 0) {
            floatLowStartTime = now;
          } else if (now - floatLowStartTime >= FLOAT_DEBOUNCE_MS) {
            currentState = STATE_CASE1_SURGE_DRAINING;
            surgeDrainStartTime = now;
            floatLowStartTime = 0;
            setPumpSpeeds(0, DUTY_100_PCT, 0);

            Serial.println("\n[!] >>> SURGE DETECTED! FLOAT SWITCH SUSTAINED TRIGGER <<< [!]");
            Serial.println("[!] Pump-01: TRIPPED TO OFF (0%) - LOCKED OFF FOR 20 SECONDS!");
            Serial.println("[!] Pump-02: DRAINING SUMP AT 100%...\n");
          }
        } else {
          floatLowStartTime = 0;
        }
      }
      break;
    }

    case STATE_CASE1_SURGE_DRAINING: {
      if (now - surgeDrainStartTime >= SURGE_DRAIN_TIME_MS) {
        currentState = STATE_CASE1_RESUMED;
        resumeStartTime = now;
        setSolenoid(true);
        setPumpSpeeds(DUTY_50_PCT, DUTY_100_PCT, 0);

        Serial.println("\n=======================================================");
        Serial.println("  [CASE 1: RESUMED] NORMAL RECIRCULATION RESTORED      ");
        Serial.println("=======================================================");
        Serial.println("  - Pump-01 restarted at 50% PWM.");
        Serial.println("  - Running steady-state for 10s before Case 2 transition...");
        Serial.println("=======================================================\n");
      }
      break;
    }

    case STATE_CASE1_RESUMED: {
      if (now - resumeStartTime >= RESUME_RUN_TIME_MS) {
        currentState = STATE_TRANSITION;
        stateStartTime = now;
        lastCountdownSec = -1;
        setSolenoid(false);
        setPumpSpeeds(0, 0, 0);

        Serial.println("\n=======================================================");
        Serial.println("  [PHASE 2] CASE 1 COMPLETE -> TRANSITIONING TO CASE 2 ");
        Serial.println("=======================================================");
        Serial.println("  - OV-01 Solenoid: CLOSED (10L Tank Isolated!)");
        Serial.println("  ==> OPERATOR: OPEN MANUAL BALL VALVE BV-03 NOW! <==");
        Serial.println("=======================================================\n");
      }
      break;
    }

    case STATE_TRANSITION: {
      unsigned long elapsed = now - stateStartTime;
      int remainingSec = (TRANSITION_TIME_MS - elapsed) / 1000;

      if (remainingSec != lastCountdownSec && remainingSec >= 0) {
        Serial.printf("[COUNTDOWN] >>> OPEN BV-03! Flushing starts in %2d seconds <<<\n", remainingSec);
        lastCountdownSec = remainingSec;
      }

      if (elapsed >= TRANSITION_TIME_MS) {
        currentState = STATE_CASE2_FLUSHING;
        stateStartTime = now;
        flushStartPulseCount = pulseCount;
        flushDropConsecutiveSeconds = 0;
        flushFlowEstablished = false;
        setSolenoid(false);
        setPumpSpeeds(DUTY_100_PCT, DUTY_100_PCT, DUTY_100_PCT);

        Serial.println("\n=======================================================");
        Serial.println("  [PHASE 3] CASE 2 STARTED: ALL 3 PUMPS FLUSHING (100%)");
        Serial.println("=======================================================");
        Serial.println("  - OV-01 Solenoid: CLOSED");
        Serial.println("  - All 3 pumps running at 100% PWM");
        Serial.println("=======================================================\n");
      }
      break;
    }

    case STATE_CASE2_FLUSHING:
      break;

    case STATE_SYSTEM_SHUTDOWN:
      break;
  }

  if (now - lastTelemetryTime >= 1000) {
    noInterrupts();
    unsigned long currentPulses = pulseCount;
    interrupts();

    unsigned long deltaPulses = currentPulses - prevPulses;
    float freqHz = (float)deltaPulses;

    float estFlowLpm = (freqHz < 5.0) ? 0.0 : ((cal_a * freqHz) + cal_b);
    float estFlowMls = estFlowLpm * (1000.0 / 60.0);
    float totalLiters = (float)currentPulses / PULSES_PER_LITER;

    if (currentState == STATE_CASE1_RECIRC) {
      Serial.printf("[CASE 1: NORMAL] Freq: %5.1f Hz | Flow: %4.2f L/min (%4.1f mL/s) | Total: %4.2f L\n",
        freqHz, estFlowLpm, estFlowMls, totalLiters);

    } else if (currentState == STATE_CASE1_SURGE_DRAINING) {
      unsigned long drainRemain =
        (SURGE_DRAIN_TIME_MS > (now - surgeDrainStartTime))
        ? ((SURGE_DRAIN_TIME_MS - (now - surgeDrainStartTime)) / 1000)
        : 0;

      Serial.printf("[CASE 1: SURGE DRAIN] Freq: %5.1f Hz | P1:OFF P2:100%% | %2lus left | Total: %4.2f L\n",
        freqHz, drainRemain, totalLiters);

    } else if (currentState == STATE_CASE1_RESUMED) {
      unsigned long resumeRemain =
        (RESUME_RUN_TIME_MS > (now - resumeStartTime))
        ? ((RESUME_RUN_TIME_MS - (now - resumeStartTime)) / 1000)
        : 0;

      Serial.printf("[CASE 1: RESUMED] Freq: %5.1f Hz | P1:50%% P2:100%% | Flow: %4.2f L/min | Transition in: %2lus | Total: %4.2f L\n",
        freqHz, estFlowLpm, resumeRemain, totalLiters);

    } else if (currentState == STATE_CASE2_FLUSHING) {
      unsigned long flushElapsed = (now - stateStartTime) / 1000;
      float flushedVolume =
        (float)(currentPulses - flushStartPulseCount) / PULSES_PER_LITER;

      Serial.printf("[CASE 2: FLUSHING | %2lus] 3 PUMPS:100%% | Freq:%5.1f Hz | Flow:%4.2f L/min | Flushed:%4.2f L\n",
        flushElapsed, freqHz, estFlowLpm, flushedVolume);

      if (!flushFlowEstablished && freqHz >= FLUSH_ESTABLISHED_FREQ_HZ) {
        flushFlowEstablished = true;
        Serial.println("  [*] >>> STEADY FLUSH FLOW ESTABLISHED <<<");
      }

      bool flowDropped = false;

      if (flushFlowEstablished ||
          (now - stateStartTime > FLUSH_INITIAL_GRACE_MS)) {

        if (freqHz < FLUSH_DRY_CUTOFF_FREQ_HZ) {
          flushDropConsecutiveSeconds++;

          if (flushDropConsecutiveSeconds >=
              FLUSH_CUTOFF_CONSECUTIVE_SEC) {
            flowDropped = true;
          }
        } else {
          flushDropConsecutiveSeconds = 0;
        }
      }

      if (flowDropped) {
        currentState = STATE_SYSTEM_SHUTDOWN;
        setPumpSpeeds(0, 0, 0);
        setSolenoid(false);

        Serial.println("\n=======================================================");
        Serial.println("  [FINISHED] LOW-FLOW / DRY-RUN CONDITION DETECTED     ");
        Serial.println("  [AUTO-SHUTDOWN] ALL THREE PUMPS OFF                  ");
        Serial.println("=======================================================\n");
      }

    } else if (currentState == STATE_STANDBY) {
      Serial.println("[STANDBY] Ready. Type 's' to start.");
    }

    prevPulses = currentPulses;
    lastTelemetryTime = now;
  }
}
