/*
 * ============================================================================
 * Lam Research Challenge 3.0 - Stage 2
 * FT-01 Flow Sensor Calibration Utility
 * Team: Finite Element Gang
 * ============================================================================
 *
 * PURPOSE
 * - Measure FT-01 pulses over a fixed sampling interval.
 * - Allow manual collection of water volume during the same interval.
 * - Calculate:
 *      f = N / t
 *      Q_actual = (V / t) * 60
 * - Store multiple experimental trials.
 * - Compute the live least-squares calibration:
 *      Q = a*f + b
 *   and R^2 from the experimental trials.
 *
 * SERIAL COMMANDS
 *   on            -> Pump-02 ON at 100% PWM
 *   off           -> Pump-02 OFF
 *   t             -> Start a 30-second pulse-counting trial
 *   v <liters>    -> Enter collected volume after the trial, e.g. v 1.055
 *   list          -> Print all stored trials
 *   fit           -> Print current linear regression and R^2
 *   clear         -> Delete stored trials
 *   help          -> Print commands
 *
 * TEST PROCEDURE
 * 1. Fill the sump and prepare the collection vessel.
 * 2. Set the desired ball-valve opening and allow flow to stabilize.
 * 3. Type: t
 * 4. Collect discharge for the full 30-second interval.
 * 5. Measure collected volume accurately.
 * 6. Type: v <volume in liters>
 * 7. Repeat at >=5 stable flow conditions.
 *
 * IMPORTANT
 * - Do not enter an analytically estimated point as an experimental trial.
 * - The collected-volume measurement is the reference/ground-truth flow.
 * ============================================================================
 */

#include <Arduino.h>

// ---------------- PIN ALLOCATION ----------------
const int PIN_FLOW_SENSOR = 14;

// Pump-02 driver
const int PIN_P2_LPWM = 18;
const int PIN_P2_LEN  = 16;
const int PIN_P2_REN  = 17;

// ---------------- PWM SETTINGS ----------------
const int PWM_FREQ = 5000;
const int PWM_RES  = 8;
const int DUTY_100_PCT = 255;

// ---------------- CALIBRATION SETTINGS ----------------
const unsigned long TRIAL_DURATION_MS = 30000; // 30 seconds
const int MAX_TRIALS = 10;

// ---------------- RUNTIME DATA ----------------
volatile unsigned long pulseCount = 0;

bool trialRunning = false;
bool waitingForVolume = false;

unsigned long trialStartTime = 0;
unsigned long trialStartPulses = 0;
unsigned long lastTrialPulses = 0;
float lastTrialSeconds = 0.0;
float lastTrialFreqHz = 0.0;

float trialFreq[MAX_TRIALS];
float trialFlow[MAX_TRIALS];
unsigned long trialPulses[MAX_TRIALS];
float trialTime[MAX_TRIALS];
float trialVolume[MAX_TRIALS];
int trialCount = 0;

void IRAM_ATTR flowISR() {
  pulseCount++;
}

void setPump2(bool on) {
  ledcWrite(PIN_P2_LPWM, on ? DUTY_100_PCT : 0);
  Serial.printf("[P2] %s\n", on ? "ON - 100% PWM" : "OFF");
}

void printHelp() {
  Serial.println("\n================ CALIBRATION COMMANDS ================");
  Serial.println("  on             -> Pump-02 ON at 100% PWM");
  Serial.println("  off            -> Pump-02 OFF");
  Serial.println("  t              -> Start 30 s calibration trial");
  Serial.println("  v <liters>     -> Enter collected volume after trial");
  Serial.println("                   Example: v 1.055");
  Serial.println("  list           -> Print all stored trials");
  Serial.println("  fit            -> Calculate Q = a*f + b and R^2");
  Serial.println("  clear          -> Delete all stored trials");
  Serial.println("  help           -> Show this menu");
  Serial.println("======================================================\n");
}

void printTrials() {
  Serial.println("\nTrial,Pulses,Time_s,Frequency_Hz,Volume_L,Actual_Flow_Lpm");

  for (int i = 0; i < trialCount; i++) {
    Serial.printf(
      "%d,%lu,%.2f,%.3f,%.4f,%.4f\n",
      i + 1,
      trialPulses[i],
      trialTime[i],
      trialFreq[i],
      trialVolume[i],
      trialFlow[i]
    );
  }

  if (trialCount == 0) {
    Serial.println("(no completed trials)");
  }

  Serial.println();
}

void calculateRegression() {
  if (trialCount < 2) {
    Serial.println("[FIT] Need at least 2 completed trials.");
    return;
  }

  double sumX = 0.0;
  double sumY = 0.0;
  double sumXY = 0.0;
  double sumX2 = 0.0;

  for (int i = 0; i < trialCount; i++) {
    double x = trialFreq[i];
    double y = trialFlow[i];

    sumX += x;
    sumY += y;
    sumXY += x * y;
    sumX2 += x * x;
  }

  double n = trialCount;
  double denominator = (n * sumX2) - (sumX * sumX);

  if (fabs(denominator) < 1e-12) {
    Serial.println("[FIT] Cannot calculate regression: frequencies are not sufficiently different.");
    return;
  }

  double a = ((n * sumXY) - (sumX * sumY)) / denominator;
  double b = (sumY - a * sumX) / n;

  double meanY = sumY / n;
  double ssRes = 0.0;
  double ssTot = 0.0;

  double maxErrorPct = 0.0;
  double sumErrorPct = 0.0;

  for (int i = 0; i < trialCount; i++) {
    double predicted = a * trialFreq[i] + b;
    double residual = trialFlow[i] - predicted;

    ssRes += residual * residual;

    double centered = trialFlow[i] - meanY;
    ssTot += centered * centered;

    if (trialFlow[i] > 0.0) {
      double errorPct =
        fabs(predicted - trialFlow[i]) / trialFlow[i] * 100.0;

      sumErrorPct += errorPct;

      if (errorPct > maxErrorPct) {
        maxErrorPct = errorPct;
      }
    }
  }

  double r2 = (ssTot > 0.0) ? (1.0 - ssRes / ssTot) : 1.0;
  double avgErrorPct = sumErrorPct / trialCount;

  Serial.println("\n================ CALIBRATION FIT =================");
  Serial.printf("Trials used: %d\n", trialCount);
  Serial.printf("Q = %.8f * f + %.6f  [L/min]\n", a, b);
  Serial.printf("a = %.8f (L/min)/Hz\n", a);
  Serial.printf("b = %.6f L/min\n", b);
  Serial.printf("R^2 = %.6f\n", r2);
  Serial.printf("Average relative error = %.3f %%\n", avgErrorPct);
  Serial.printf("Maximum relative error = %.3f %%\n", maxErrorPct);
  Serial.println("==================================================\n");
}

void startTrial() {
  if (trialRunning) {
    Serial.println("[TRIAL] A trial is already running.");
    return;
  }

  if (waitingForVolume) {
    Serial.println("[TRIAL] Enter collected volume for the previous trial first.");
    Serial.println("        Example: v 1.055");
    return;
  }

  if (trialCount >= MAX_TRIALS) {
    Serial.println("[TRIAL] Maximum stored trial count reached. Use 'clear' first.");
    return;
  }

  noInterrupts();
  trialStartPulses = pulseCount;
  interrupts();

  trialStartTime = millis();
  trialRunning = true;

  Serial.println("\n==================================================");
  Serial.printf("[TRIAL %d] STARTED - collect water for 30.0 s\n", trialCount + 1);
  Serial.println("Keep the valve position unchanged during the trial.");
  Serial.println("==================================================\n");
}

void finishTrial() {
  unsigned long endPulses;

  noInterrupts();
  endPulses = pulseCount;
  interrupts();

  unsigned long elapsedMs = millis() - trialStartTime;

  lastTrialPulses = endPulses - trialStartPulses;
  lastTrialSeconds = elapsedMs / 1000.0;
  lastTrialFreqHz = lastTrialPulses / lastTrialSeconds;

  trialRunning = false;
  waitingForVolume = true;

  Serial.println("\n==================================================");
  Serial.println("[TRIAL] PULSE MEASUREMENT COMPLETE");
  Serial.printf("Pulses N       = %lu\n", lastTrialPulses);
  Serial.printf("Time t         = %.3f s\n", lastTrialSeconds);
  Serial.printf("Frequency f    = %.3f Hz\n", lastTrialFreqHz);
  Serial.println("Now measure the collected water volume.");
  Serial.println("Enter it as: v <liters>");
  Serial.println("Example:     v 1.055");
  Serial.println("==================================================\n");
}

void saveVolume(float volumeLiters) {
  if (!waitingForVolume) {
    Serial.println("[VOLUME] No completed trial is waiting for a volume.");
    return;
  }

  if (volumeLiters <= 0.0) {
    Serial.println("[VOLUME] Volume must be greater than zero.");
    return;
  }

  float actualFlowLpm =
    (volumeLiters / lastTrialSeconds) * 60.0;

  trialPulses[trialCount] = lastTrialPulses;
  trialTime[trialCount] = lastTrialSeconds;
  trialFreq[trialCount] = lastTrialFreqHz;
  trialVolume[trialCount] = volumeLiters;
  trialFlow[trialCount] = actualFlowLpm;

  trialCount++;
  waitingForVolume = false;

  Serial.println("\n================ TRIAL SAVED =====================");
  Serial.printf("Trial           = %d\n", trialCount);
  Serial.printf("Pulses N        = %lu\n", lastTrialPulses);
  Serial.printf("Time t          = %.3f s\n", lastTrialSeconds);
  Serial.printf("Frequency f     = %.3f Hz\n", lastTrialFreqHz);
  Serial.printf("Collected V     = %.4f L\n", volumeLiters);
  Serial.printf("Actual flow Q   = %.4f L/min\n", actualFlowLpm);
  Serial.println("==================================================\n");

  if (trialCount >= 2) {
    calculateRegression();
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(PIN_FLOW_SENSOR, INPUT_PULLUP);
  attachInterrupt(
    digitalPinToInterrupt(PIN_FLOW_SENSOR),
    flowISR,
    FALLING
  );

  pinMode(PIN_P2_LEN, OUTPUT);
  pinMode(PIN_P2_REN, OUTPUT);

  digitalWrite(PIN_P2_LEN, HIGH);
  digitalWrite(PIN_P2_REN, HIGH);

  ledcAttach(PIN_P2_LPWM, PWM_FREQ, PWM_RES);
  ledcWrite(PIN_P2_LPWM, 0);

  Serial.println("\n=======================================================");
  Serial.println(" LAM Stage 2 - FT-01 Experimental Calibration Utility ");
  Serial.println(" Team: Finite Element Gang                            ");
  Serial.println("=======================================================");
  Serial.println("Pump-02 starts OFF for safety.");
  printHelp();
}

void loop() {
  if (trialRunning &&
      millis() - trialStartTime >= TRIAL_DURATION_MS) {
    finishTrial();
  }

  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd.equalsIgnoreCase("on")) {
      setPump2(true);

    } else if (cmd.equalsIgnoreCase("off")) {
      setPump2(false);

    } else if (cmd.equalsIgnoreCase("t")) {
      startTrial();

    } else if (cmd.startsWith("v ") || cmd.startsWith("V ")) {
      float volume = cmd.substring(2).toFloat();
      saveVolume(volume);

    } else if (cmd.equalsIgnoreCase("list")) {
      printTrials();

    } else if (cmd.equalsIgnoreCase("fit")) {
      calculateRegression();

    } else if (cmd.equalsIgnoreCase("clear")) {
      trialCount = 0;
      waitingForVolume = false;
      trialRunning = false;
      Serial.println("[DATA] All stored calibration trials cleared.");

    } else if (cmd.equalsIgnoreCase("help")) {
      printHelp();

    } else {
      Serial.println("[COMMAND] Unknown command. Type 'help'.");
    }
  }
}
