#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_AMG88xx.h>
#include <NewPing.h>

// --- Pin Definitions ---
#define TRIGGER_PIN  12
#define ECHO_PIN     13
#define MAX_DISTANCE 200
#define SWITCH_PIN   2   // Microswitch on Pin 2 and GND

// L298N Motor Driver Pins
#define ENA 5
#define IN1 6
#define IN2 7
#define IN3 8
#define IN4 9
#define ENB 10

#define BUZZER_PIN 4

NewPing sonar(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE);
Adafruit_AMG88xx amg;

float pixels[64];
char operationMode = 'M'; // 'M' = Manual, 'A' = Auto Heat-Seeking
char currentCommand = 'S';
unsigned long lastTelemetryTime = 0;
unsigned long lastCommandTime = 0;        
unsigned long switchPressStartTime = 0;   
bool isPinned = false;

void executeMovement(char cmd);
char calculateHeatSeekingTarget(float* p, unsigned int dist);

void setup() {
  Serial.begin(115200);
  Wire.begin();

  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(SWITCH_PIN, INPUT_PULLUP);

  lastCommandTime = millis();

  // Boot status beep: 1 short beep = AMG8833 OK, 3 beeps = AMG8833 Error
  if (!amg.begin()) {
    for (int i = 0; i < 3; i++) {
      digitalWrite(BUZZER_PIN, HIGH); delay(100);
      digitalWrite(BUZZER_PIN, LOW); delay(100);
    }
  } else {
    digitalWrite(BUZZER_PIN, HIGH); delay(80);
    digitalWrite(BUZZER_PIN, LOW);
  }
}

void loop() {
  unsigned long currentTime = millis();

  // --- 1. Immediate Serial Command & Mode Listener ---
  while (Serial.available() > 0) {
    char cmd = (char)Serial.read();
    if (cmd == 'M' || cmd == 'A') {
      operationMode = cmd;
      currentCommand = 'S';
      lastCommandTime = currentTime;
    } 
    else if (cmd == 'F' || cmd == 'B' || cmd == 'L' || cmd == 'R' || cmd == 'S' || cmd == 'Z') {
      if (operationMode == 'M') {
        currentCommand = cmd;
      }
      lastCommandTime = currentTime; 
    }
  }

  // Communication Watchdog (2.0s timeout)
  if (currentTime - lastCommandTime > 2000) {
    currentCommand = 'S'; 
  }

  // --- 2. Anti-Entrapment Microswitch Overrides (Pin 2) ---
  bool switchPressed = (digitalRead(SWITCH_PIN) == LOW);

  if (switchPressed) {
    if (switchPressStartTime == 0) {
      switchPressStartTime = currentTime;
      currentCommand = 'S';
      digitalWrite(BUZZER_PIN, HIGH);
      executeMovement('R');
      delay(400);
      executeMovement('S');
      digitalWrite(BUZZER_PIN, LOW);
    } 
    else if (currentTime - switchPressStartTime > 3000 && !isPinned) {
      isPinned = true;
      currentCommand = 'S';
    }

    if (isPinned) {
      executeMovement('S');
      digitalWrite(BUZZER_PIN, (currentTime / 200) % 2);
      if (currentTime - lastTelemetryTime >= 500) {
        lastTelemetryTime = currentTime;
        Serial.println("{\"status\":\"ROBOT_TRAPPED\"}");
      }
      return; 
    }
  } else {
    switchPressStartTime = 0;
    isPinned = false;
  }

  // --- 3. Sensor Reading & Telemetry JSON Stream (Every 200ms) ---
  unsigned int distance = sonar.ping_cm();
  if (distance == 0) distance = 200;

  if (currentTime - lastTelemetryTime >= 200) {
    lastTelemetryTime = currentTime;
    amg.readPixels(pixels);

    Serial.print("{\"d\":");
    Serial.print(distance);
    Serial.print(",\"p\":[");
    for (int i = 0; i < 64; i++) {
      Serial.print(pixels[i], 1);
      if (i < 63) Serial.print(",");
    }
    Serial.println("]}");
  }

  // --- 4. Motion Execution (Auto vs. Manual) ---
  if (operationMode == 'A') {
    if (currentTime - lastCommandTime <= 2000) {
      currentCommand = calculateHeatSeekingTarget(pixels, distance);
    } else {
      currentCommand = 'S';
    }
    executeMovement(currentCommand);
  } else {
    if (distance <= 12 && currentCommand == 'L') { 
      currentCommand = 'S';
      digitalWrite(BUZZER_PIN, HIGH);
      delay(50);
      digitalWrite(BUZZER_PIN, LOW);
    } else {
      executeMovement(currentCommand);
    }
  }
}

char calculateHeatSeekingTarget(float* p, unsigned int dist) {
  if (dist <= 15) return 'S';

  float maxTemp = -999.0;
  int targetCol = -1;

  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      float temp = p[row * 8 + col];
      if (temp > maxTemp) {
        maxTemp = temp;
        targetCol = col;
      }
    }
  }

  if (maxTemp < 30.5) return 'S';

  if (targetCol <= 2) return 'F';      // Steer Left
  else if (targetCol >= 5) return 'B'; // Steer Right
  else return 'L';                     // Drive Forward
}

void executeMovement(char cmd) {
  analogWrite(ENA, 180); 
  analogWrite(ENB, 180);

  switch (cmd) {
    case 'L': // Forward
      digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
      digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
      break;
    case 'R': // Backward
      digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
      digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
      break;
    case 'F': // Left Turn
      digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
      digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
      break;
    case 'B': // Right Turn
      digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
      digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
      break;
    case 'Z': // Horn Alert
      digitalWrite(BUZZER_PIN, HIGH);
      delay(100);
      digitalWrite(BUZZER_PIN, LOW);
      currentCommand = 'S';
      break;
    case 'S':
    default:
      digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
      digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
      break;
  }
}
