/*
 * ============================================================================
 * LAM Challenge 3.0 - Task 2: Automated Flow Sensor Calibration Tool
 * Team: Finite Element Gang
 * ============================================================================
 * Exactly matches Task 2 requirements on Page 6 of the PDF:
 *   - Runs Pump-02 for EXACTLY 30.0 SECONDS into your measuring jug.
 *   - Automatically stops at 30.0s to prevent spillage.
 *   - Reports: Time (t), Total Pulses (N), and Exact Frequency (f = N/t).
 *   - You read the volume collected in the jug (V in Liters).
 * ============================================================================
 * INSTRUCTIONS:
 *   1. Direct the tube exiting Valve BV-02 into your measuring jug.
 *   2. Set BV-02 to the desired position (e.g. 100%, 75%, 50%, 25%).
 *   3. Make sure Sump has enough water (~2.5 to 3.0 Liters).
 *   4. Type 'run' (or 'r') in Serial Monitor and press Enter.
 *   5. Pump-02 runs for exactly 30 seconds and automatically cuts off!
 *   6. Note down N, t, f, and the collected volume V from your jug.
 * ============================================================================
 */

#include <Arduino.h>

const int PIN_FLOW_SENSOR = 14;

// Motor Driver U3 (Pump-02)
const int PIN_P2_LPWM = 18;
const int PIN_P2_LEN  = 16;
const int PIN_P2_REN  = 17;

const int PWM_FREQ = 5000;
const int PWM_RES  = 8;
const int DUTY_100_PCT = 255;

const unsigned long TRIAL_DURATION_MS = 30000; // Exactly 30 seconds per trial

volatile unsigned long pulseCount = 0;
bool trialRunning = false;
unsigned long trialStartTime = 0;
int lastRemainingSec = -1;
int trialNumber = 1;

void IRAM_ATTR flowISR() {
  pulseCount++;
}

void setPump2(int duty) {
  ledcWrite(PIN_P2_LPWM, duty);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(PIN_FLOW_SENSOR, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_FLOW_SENSOR), flowISR, FALLING);

  pinMode(PIN_P2_LEN, OUTPUT); digitalWrite(PIN_P2_LEN, HIGH);
  pinMode(PIN_P2_REN, OUTPUT); digitalWrite(PIN_P2_REN, HIGH);
  ledcAttach(PIN_P2_LPWM, PWM_FREQ, PWM_RES);
  ledcWrite(PIN_P2_LPWM, 0);

  Serial.println("\n=======================================================");
  Serial.println("  LAM Challenge - Task 2 Automated Calibration Tool    ");
  Serial.println("  Team: Finite Element Gang                            ");
  Serial.println("=======================================================");
  Serial.println("Point the BV-02 hose into your measuring jug.");
  Serial.println("Set BV-02 valve position (100%, 75%, 50%, 25%).");
  Serial.println("--> Type 'run' and press Enter to start 30s timed trial <--");
  Serial.println("--> Type 'x' at any time to abort trial <--");
  Serial.println("=======================================================\n");
}

void loop() {
  unsigned long now = millis();

  // Read Commands
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd.equalsIgnoreCase("run") || cmd.equalsIgnoreCase("r")) {
      if (!trialRunning) {
        // Start timed trial
        pulseCount = 0;
        trialRunning = true;
        trialStartTime = now;
        lastRemainingSec = -1;
        setPump2(DUTY_100_PCT);

        Serial.printf("\n>>> [TRIAL %d STARTED] Pump-02 running at 100%% for 30.0 seconds! <<<\n", trialNumber);
      }
    } else if (cmd.equalsIgnoreCase("x") || cmd.equalsIgnoreCase("stop")) {
      trialRunning = false;
      setPump2(0);
      Serial.println("\n[!] TRIAL ABORTED. Pump-02 stopped.");
    }
  }

  // During Trial
  if (trialRunning) {
    unsigned long elapsed = now - trialStartTime;
    int remainingSec = (TRIAL_DURATION_MS - elapsed) / 1000;

    // Print countdown every 5 seconds or last 5 seconds
    if (remainingSec != lastRemainingSec && remainingSec >= 0) {
      if (remainingSec % 5 == 0 || remainingSec <= 5) {
        Serial.printf("  [Counting down] %2d seconds remaining... (Live Pulses: %lu)\n", 
          remainingSec, pulseCount);
      }
      lastRemainingSec = remainingSec;
    }

    // Trial Complete!
    if (elapsed >= TRIAL_DURATION_MS) {
      setPump2(0); // Stop pump instantly
      trialRunning = false;
      float actualTimeSec = (float)elapsed / 1000.0;
      float frequencyHz = (float)pulseCount / actualTimeSec;

      Serial.println("\n=======================================================");
      Serial.printf("           >>> TRIAL %d COMPLETE! <<<                  \n", trialNumber);
      Serial.println("=======================================================");
      Serial.printf("  Trial Time (t)        : %.2f seconds\n", actualTimeSec);
      Serial.printf("  Total Pulses (N)      : %lu pulses\n", pulseCount);
      Serial.printf("  Average Frequency (f) : %.2f Hz  (f = N / t)\n", frequencyHz);
      Serial.println("-------------------------------------------------------");
      Serial.println(">> ACTION: Look at your measuring jug!");
      Serial.println(">> Note the collected volume (V) in Liters or mL.");
      Serial.println("=======================================================");
      Serial.println("Empty the jug back into the tank, change valve position,");
      Serial.println("and type 'run' for the next trial!\n");

      trialNumber++;
    }
  }
}
