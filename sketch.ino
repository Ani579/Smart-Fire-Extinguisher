////Smart Fire-Extinguisher

#include <Servo.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ──────────── OLED Configuration ────────────
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  64
#define OLED_RESET     -1
#define OLED_ADDR      0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ──────────── Pin Definitions ────────────
#define SERVO1_PIN      11   // left water cannon
#define SERVO2_PIN      10   // right water cannon
#define TRIG_PIN         8
#define ECHO_PIN         7
#define GAS_ANALOG_PIN  A1
#define GAS_DIGITAL_PIN  2
#define BUZZER_PIN      12
#define RED_LED_PIN      6
#define GREEN_LED_PIN    3
#define POT_PIN         A0

// ──────────── Thresholds & Constants ────────────
#define DISTANCE_THRESHOLD_CM   50   // anything closer = potential fire
#define DEFAULT_SMOKE_THRESH   300
#define SERVO_SCAN_MIN          30   // scan range lower bound (°)
#define SERVO_SCAN_MAX         150   // scan range upper bound (°)
#define SERVO_SCAN_STEP          5   // degrees per scan step
#define SCAN_STEP_DELAY_MS      60   // pause at each scan step for reading
#define RESCAN_INTERVAL_MS    3000   // re-scan every 3 s while locked
#define DISPLAY_UPDATE_MS      250
#define ALARM_TONE_HZ         2000

// ──────────── State Machine ────────────
enum SystemState {
  STATE_NORMAL,    // no threat — cannons parked
  STATE_SCANNING,  // threat detected — sweeping to find fire angle
  STATE_LOCKED     // fire located — cannons aimed at target
};

SystemState systemState = STATE_NORMAL;

// ──────────── Global Objects ────────────
Servo servo1;
Servo servo2;

int   smokeThreshold    = DEFAULT_SMOKE_THRESH;
float distanceCm        = 0;
int   smokeAnalog       = 0;
bool  smokeDigital      = false;
bool  fireDetected      = false;
bool  smokeDetected     = false;

// Scanning state
int   scanAngle         = SERVO_SCAN_MIN;  // current angle during scan
int   scanDirection     = 1;               // +1 or -1
float bestDistance       = 999.0;           // shortest distance found
int   bestAngle         = 90;              // angle of shortest distance

// Locked state
int   lockedAngle       = 90;
float lockedDistance     = 999.0;
unsigned long lockTime  = 0;               // when we locked on

unsigned long lastDisplayUpdate = 0;

// ──────────── Ultrasonic Measurement ────────────
float measureDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) return 999.0;
  return (duration * 0.0343) / 2.0;
}

// ──────────── Read All Sensors ────────────
void readSensors() {
  distanceCm = measureDistance();

  smokeAnalog  = analogRead(GAS_ANALOG_PIN);
  smokeDigital = (digitalRead(GAS_DIGITAL_PIN) == LOW);

  int potVal    = analogRead(POT_PIN);
  smokeThreshold = map(potVal, 0, 1023, 100, 800);

  fireDetected  = (distanceCm < DISTANCE_THRESHOLD_CM);
  smokeDetected = (smokeAnalog > smokeThreshold) || smokeDigital;
}

// ──────────── Aim Both Cannons at an Angle ────────────
void aimCannons(int angle) {
  servo1.write(angle);
  servo2.write(angle);   // both point at the same direction
}

// ──────────── Park Cannons (centre, idle) ────────────
void parkCannons() {
  servo1.write(90);
  servo2.write(90);
}

// ──────────── Begin a New Scan Sweep ────────────
void startScan() {
  scanAngle    = SERVO_SCAN_MIN;
  scanDirection = 1;
  bestDistance  = 999.0;
  bestAngle    = 90;
  systemState  = STATE_SCANNING;
}

// ──────────── One Step of the Scan ────────────
//  Moves the servos to scanAngle, takes a distance reading,
//  remembers the angle with the shortest distance, then
//  advances scanAngle.  Returns true when the full sweep is done.
bool scanStep() {
  // Aim both cannons at the current scan angle
  aimCannons(scanAngle);
  delay(SCAN_STEP_DELAY_MS);   // let the servo settle & read

  float d = measureDistance();

  if (d < bestDistance) {
    bestDistance = d;
    bestAngle   = scanAngle;
  }

  // Advance
  scanAngle += SERVO_SCAN_STEP * scanDirection;

  if (scanAngle > SERVO_SCAN_MAX || scanAngle < SERVO_SCAN_MIN) {
    // Sweep complete
    return true;
  }
  return false;
}

// ──────────── Lock Cannons on Target ────────────
void lockOnTarget() {
  lockedAngle    = bestAngle;
  lockedDistance  = bestDistance;
  lockTime       = millis();
  systemState    = STATE_LOCKED;

  aimCannons(lockedAngle);

  Serial.print(F(">> LOCKED at "));
  Serial.print(lockedAngle);
  Serial.print(F("° — dist "));
  Serial.print(lockedDistance, 1);
  Serial.println(F(" cm"));
}

// ──────────── Indicators: Alert ON ────────────
void alertOn() {
  digitalWrite(RED_LED_PIN, HIGH);
  digitalWrite(GREEN_LED_PIN, LOW);
  tone(BUZZER_PIN, ALARM_TONE_HZ);
}

// ──────────── Indicators: Alert OFF ────────────
void alertOff() {
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, HIGH);
  noTone(BUZZER_PIN);
}

// ──────────── Update OLED ────────────
void updateDisplay() {
  if (millis() - lastDisplayUpdate < DISPLAY_UPDATE_MS) return;
  lastDisplayUpdate = millis();

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Title
  display.setTextSize(1);
  display.setCursor(4, 0);
  display.print(F("SMART FIRE EXTINGUISH"));
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  // Sensor data
  display.setCursor(0, 14);
  display.print(F("Dist : "));
  if (distanceCm > 400) display.println(F("---  cm"));
  else { display.print(distanceCm, 1); display.println(F(" cm")); }

  display.setCursor(0, 24);
  display.print(F("Smoke: "));
  display.print(smokeAnalog);
  display.print(F("/"));
  display.println(smokeThreshold);

  // State-specific section
  display.drawLine(0, 34, 127, 34, SSD1306_WHITE);

  switch (systemState) {
    case STATE_NORMAL:
      display.setTextSize(2);
      display.setCursor(22, 40);
      display.print(F("NORMAL"));
      break;

    case STATE_SCANNING:
      display.setTextSize(1);
      display.setCursor(4, 38);
      display.print(F("SCANNING..."));
      display.setCursor(4, 48);
      display.print(F("Angle: "));
      display.print(scanAngle);
      display.print(F("\xF8"));         // degree symbol
      display.setCursor(4, 57);
      display.print(F("Best : "));
      display.print(bestDistance, 1);
      display.print(F("cm @"));
      display.print(bestAngle);
      display.print(F("\xF8"));
      break;

    case STATE_LOCKED:
      display.setTextSize(1);
      display.setCursor(4, 37);
      if (fireDetected && smokeDetected) {
        display.print(F("!! FIRE + SMOKE !!"));
      } else if (fireDetected) {
        display.print(F("FIRE DETECTED"));
      } else {
        display.print(F("SMOKE DETECTED"));
      }
      display.setCursor(4, 48);
      display.print(F("Locked: "));
      display.print(lockedAngle);
      display.print(F("\xF8"));
      display.setCursor(4, 57);
      display.print(F("Range : "));
      display.print(lockedDistance, 1);
      display.print(F(" cm"));
      break;
  }

  display.display();
}

// ──────────── Setup ────────────
void setup() {
  Serial.begin(115200);
  Serial.println(F("== IOTRICITY S3 — Smart Fire Extinguisher =="));

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(GAS_DIGITAL_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);

  servo1.attach(SERVO1_PIN);
  servo2.attach(SERVO2_PIN);
  parkCannons();

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("OLED init FAILED"));
    while (true);
  }

  // Splash screen
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(16, 10);
  display.println(F("IOTRICITY S3"));
  display.setCursor(8, 28);
  display.println(F("Smart Fire"));
  display.setCursor(8, 40);
  display.println(F("Extinguisher v1.0"));
  display.display();
  delay(2000);

  alertOff();
  Serial.println(F("System ready."));
}

// ──────────── Main Loop ────────────
void loop() {

  switch (systemState) {

    // ─── NORMAL: no threat ───
    case STATE_NORMAL:
      readSensors();
      if (fireDetected || smokeDetected) {
        Serial.println(F(">> Threat detected — starting scan"));
        alertOn();
        startScan();
      } else {
        alertOff();
        parkCannons();
      }
      break;

    // ─── SCANNING: sweeping to locate fire ───
    case STATE_SCANNING:
      alertOn();
      if (scanStep()) {
        // Full sweep complete
        if (bestDistance < DISTANCE_THRESHOLD_CM) {
          lockOnTarget();
        } else {
          // Smoke triggered but ultrasonic found nothing close —
          // lock at the best angle anyway (spray toward closest reading)
          lockOnTarget();
        }
      }
      break;

    // ─── LOCKED: cannons aimed at fire ───
    case STATE_LOCKED:
      readSensors();

      if (!fireDetected && !smokeDetected) {
        // Threat cleared
        Serial.println(F(">> Threat cleared — returning to NORMAL"));
        systemState = STATE_NORMAL;
        alertOff();
        parkCannons();
        break;
      }

      alertOn();
      aimCannons(lockedAngle);   // hold position

      // Periodically re-scan to track fire movement
      if (millis() - lockTime > RESCAN_INTERVAL_MS) {
        Serial.println(F(">> Re-scanning to track fire"));
        startScan();
      }
      break;
  }

  updateDisplay();

  // Serial telemetry
  Serial.print(F("State="));
  Serial.print(systemState == STATE_NORMAL ? "NORMAL" :
               systemState == STATE_SCANNING ? "SCAN" : "LOCKED");
  Serial.print(F(" | Dist="));
  Serial.print(distanceCm, 1);
  Serial.print(F("cm | Smoke="));
  Serial.print(smokeAnalog);
  Serial.print(F("/"));
  Serial.print(smokeThreshold);
  if (systemState == STATE_LOCKED) {
    Serial.print(F(" | Aim="));
    Serial.print(lockedAngle);
    Serial.print(F("°"));
  }
  Serial.println();
}
