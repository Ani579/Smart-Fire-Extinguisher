//Smart Fire-Extinguisher

#include <Servo.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ──────────── OLED Configuration ────────────
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  64
#define OLED_RESET     -1        // no reset pin
#define OLED_ADDR      0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ──────────── Pin Definitions ────────────
// Servos
#define SERVO1_PIN     11   // left water cannon
#define SERVO2_PIN     10   // right water cannon

// Ultrasonic
#define TRIG_PIN        8
#define ECHO_PIN        7

// Gas / Smoke Sensor
#define GAS_ANALOG_PIN A1   // analog reading
#define GAS_DIGITAL_PIN 2   // digital threshold output

// Buzzer
#define BUZZER_PIN     12

// LEDs
#define RED_LED_PIN     6   // fire alert
#define GREEN_LED_PIN   3   // safe / normal

// Potentiometer (sensitivity)
#define POT_PIN        A0

// ──────────── Thresholds & Constants ────────────
#define DISTANCE_THRESHOLD_CM  50    // fire within this range triggers response
#define DEFAULT_SMOKE_THRESH  300    // default analog smoke threshold
#define SERVO_SWEEP_MIN        30    // servo sweep lower bound (degrees)
#define SERVO_SWEEP_MAX       150    // servo sweep upper bound (degrees)
#define SERVO_SWEEP_STEP        5    // degrees per step
#define SWEEP_DELAY_MS         30    // delay between sweep steps (ms)
#define DISPLAY_UPDATE_MS     250    // OLED refresh interval (ms)
#define ALARM_TONE_HZ        2000    // buzzer alarm frequency

// ──────────── Global Objects & State ────────────
Servo servo1;
Servo servo2;

int   smokeThreshold   = DEFAULT_SMOKE_THRESH;
float distanceCm       = 0;
int   smokeAnalog      = 0;
bool  smokeDigital     = false;   // HIGH = smoke detected on digital pin
bool  fireDetected     = false;
bool  smokeDetected    = false;
int   servoAngle       = 90;     // current cannon angle
int   sweepDirection   = 1;      // +1 or -1
unsigned long lastDisplayUpdate = 0;

// ──────────── Helper: Measure Ultrasonic Distance ────────────
float measureDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // 30 ms timeout
  if (duration == 0) return 999.0;                 // no echo → far away
  return (duration * 0.0343) / 2.0;                // cm
}

// ──────────── Helper: Read Sensors ────────────
void readSensors() {
  // Ultrasonic distance
  distanceCm = measureDistance();

  // Gas / Smoke sensor
  smokeAnalog  = analogRead(GAS_ANALOG_PIN);
  smokeDigital = (digitalRead(GAS_DIGITAL_PIN) == LOW);  // active-low in many modules

  // Potentiometer → adjust smoke sensitivity (map 0-1023 → 100-800)
  int potVal = analogRead(POT_PIN);
  smokeThreshold = map(potVal, 0, 1023, 100, 800);

  // Decision logic
  fireDetected  = (distanceCm < DISTANCE_THRESHOLD_CM);
  smokeDetected = (smokeAnalog > smokeThreshold) || smokeDigital;
}

// ──────────── Helper: Sweep Servos (water cannons) ────────────
void sweepServos() {
  servoAngle += SERVO_SWEEP_STEP * sweepDirection;
  if (servoAngle >= SERVO_SWEEP_MAX) {
    servoAngle = SERVO_SWEEP_MAX;
    sweepDirection = -1;
  } else if (servoAngle <= SERVO_SWEEP_MIN) {
    servoAngle = SERVO_SWEEP_MIN;
    sweepDirection = 1;
  }
  servo1.write(servoAngle);
  servo2.write(180 - servoAngle);  // mirror the second cannon
}

// ──────────── Helper: Park Servos (idle position) ────────────
void parkServos() {
  servoAngle = 90;
  sweepDirection = 1;
  servo1.write(90);
  servo2.write(90);
}

// ──────────── Helper: Update OLED Display ────────────
void updateDisplay() {
  if (millis() - lastDisplayUpdate < DISPLAY_UPDATE_MS) return;
  lastDisplayUpdate = millis();

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // ── Title Bar ──
  display.setTextSize(1);
  display.setCursor(4, 0);
  display.print(F("SMART FIRE EXTINGUISH"));
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  // ── Sensor Readings ──
  display.setCursor(0, 14);
  display.print(F("Dist : "));
  if (distanceCm > 400) {
    display.println(F("---  cm"));
  } else {
    display.print(distanceCm, 1);
    display.println(F(" cm"));
  }

  display.setCursor(0, 24);
  display.print(F("Smoke: "));
  display.print(smokeAnalog);
  display.print(F("/"));
  display.println(smokeThreshold);

  display.setCursor(0, 34);
  display.print(F("Sens : "));
  display.println(smokeThreshold);

  // ── Status Banner ──
  display.drawLine(0, 44, 127, 44, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 48);

  if (fireDetected && smokeDetected) {
    display.setTextSize(2);
    display.setCursor(10, 48);
    display.print(F("!! FIRE !!"));
  } else if (fireDetected) {
    display.setTextSize(1);
    display.setCursor(4, 48);
    display.print(F("HEAT SOURCE NEARBY"));
    display.setCursor(4, 57);
    display.print(F("Cannons: ACTIVE"));
  } else if (smokeDetected) {
    display.setTextSize(1);
    display.setCursor(4, 48);
    display.print(F("SMOKE DETECTED!"));
    display.setCursor(4, 57);
    display.print(F("Cannons: ACTIVE"));
  } else {
    display.setTextSize(2);
    display.setCursor(22, 48);
    display.print(F("NORMAL"));
  }

  display.display();
}

// ──────────── Helper: Activate Fire Response ────────────
void activateResponse() {
  // Red LED on, Green LED off
  digitalWrite(RED_LED_PIN, HIGH);
  digitalWrite(GREEN_LED_PIN, LOW);

  // Buzzer alarm — alternating tone
  tone(BUZZER_PIN, ALARM_TONE_HZ);

  // Sweep water cannons toward the fire
  sweepServos();
}

// ──────────── Helper: Deactivate (Normal Mode) ────────────
void deactivateResponse() {
  // Green LED on, Red LED off
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, HIGH);

  // Buzzer off
  noTone(BUZZER_PIN);

  // Park servos at center
  parkServos();
}

// ──────────── Setup ────────────
void setup() {
  Serial.begin(115200);
  Serial.println(F("== IOTRICITY S3 — Smart Fire Extinguisher =="));

  // Pin modes
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(GAS_DIGITAL_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);

  // Servos
  servo1.attach(SERVO1_PIN);
  servo2.attach(SERVO2_PIN);
  parkServos();

  // OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("OLED init FAILED"));
    while (true);  // halt
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  // Splash screen
  display.setCursor(16, 10);
  display.setTextSize(1);
  display.println(F("IOTRICITY S3"));
  display.setCursor(8, 28);
  display.println(F("Smart Fire"));
  display.setCursor(8, 40);
  display.println(F("Extinguisher v1.0"));
  display.display();
  delay(2000);

  // Start with green LED (safe)
  digitalWrite(GREEN_LED_PIN, HIGH);
  digitalWrite(RED_LED_PIN, LOW);
  noTone(BUZZER_PIN);

  Serial.println(F("System ready."));
}

// ──────────── Main Loop ────────────
void loop() {
  readSensors();

  // Serial telemetry
  Serial.print(F("Dist="));
  Serial.print(distanceCm, 1);
  Serial.print(F(" cm | Smoke="));
  Serial.print(smokeAnalog);
  Serial.print(F(" (thr="));
  Serial.print(smokeThreshold);
  Serial.print(F(") | Fire="));
  Serial.print(fireDetected ? "YES" : "no");
  Serial.print(F(" | Smoke="));
  Serial.println(smokeDetected ? "YES" : "no");

  // Response logic
  if (fireDetected || smokeDetected) {
    activateResponse();
  } else {
    deactivateResponse();
  }

  // Update OLED
  updateDisplay();

  delay(SWEEP_DELAY_MS);
}
