#include <WiFi.h>
#include <WebServer.h>

// ======================================================
// WIFI
// ======================================================

const char* WIFI_NAME = "ROVER_ESP32";
const char* WIFI_PASSWORD = "12345678";

WebServer server(80);

// ======================================================
// MOTOR PINS
// ======================================================

#define LEFT_RPWM   25
#define LEFT_LPWM   26

#define RIGHT_RPWM  27
#define RIGHT_LPWM  14

// ======================================================
// ULTRASONIC SENSOR PINS
// ======================================================

// LEFT HC-SR04
#define TRIG_LEFT   32
#define ECHO_LEFT   34

// RIGHT HC-SR04
#define TRIG_RIGHT  33
#define ECHO_RIGHT 35

// ======================================================
// BUZZER
// ======================================================

#define BUZZER_PIN 23

// ======================================================
// MOTOR SPEED
// ======================================================

int motorSpeed = 200;

// ======================================================
// OBSTACLE SETTINGS
// ======================================================

// 200 mm = 20 cm
const float OBSTACLE_DISTANCE_MM = 200.0;

// Sensor reading interval
const unsigned long SENSOR_INTERVAL = 70;

// ======================================================
// MOTION STATES
// ======================================================

enum Motion {
  STOPPED,
  FORWARD,
  BACKWARD,
  LEFT,
  RIGHT
};

Motion currentMotion = STOPPED;

// ======================================================
// SENSOR VARIABLES
// ======================================================

float leftDistanceMM = 9999;
float rightDistanceMM = 9999;

unsigned long lastSensorRead = 0;

// ======================================================
// MOTOR FUNCTIONS
// ======================================================

void stopMotors() {

  ledcWrite(LEFT_RPWM, 0);
  ledcWrite(LEFT_LPWM, 0);

  ledcWrite(RIGHT_RPWM, 0);
  ledcWrite(RIGHT_LPWM, 0);

  currentMotion = STOPPED;
}

// ------------------------------------------------------

void moveForward() {

  ledcWrite(LEFT_RPWM, motorSpeed);
  ledcWrite(LEFT_LPWM, 0);

  ledcWrite(RIGHT_RPWM, motorSpeed);
  ledcWrite(RIGHT_LPWM, 0);

  currentMotion = FORWARD;
}

// ------------------------------------------------------

void moveBackward() {

  ledcWrite(LEFT_RPWM, 0);
  ledcWrite(LEFT_LPWM, motorSpeed);

  ledcWrite(RIGHT_RPWM, 0);
  ledcWrite(RIGHT_LPWM, motorSpeed);

  currentMotion = BACKWARD;
}

// ------------------------------------------------------

void turnLeft() {

  ledcWrite(LEFT_RPWM, 0);
  ledcWrite(LEFT_LPWM, motorSpeed);

  ledcWrite(RIGHT_RPWM, motorSpeed);
  ledcWrite(RIGHT_LPWM, 0);

  currentMotion = LEFT;
}

// ------------------------------------------------------

void turnRight() {

  ledcWrite(LEFT_RPWM, motorSpeed);
  ledcWrite(LEFT_LPWM, 0);

  ledcWrite(RIGHT_RPWM, 0);
  ledcWrite(RIGHT_LPWM, motorSpeed);

  currentMotion = RIGHT;
}

// ======================================================
// ULTRASONIC DISTANCE FUNCTION
// ======================================================

float readUltrasonicMM(int trigPin, int echoPin) {

  // Ensure trigger is LOW
  digitalWrite(trigPin, LOW);
  delayMicroseconds(3);

  // Send 10 microsecond trigger pulse
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Read echo
  // Timeout = 30 milliseconds
  unsigned long duration = pulseIn(echoPin, HIGH, 30000);

  // No echo received
  if (duration == 0) {
    return 9999;
  }

  // Convert echo time to millimeters
  float distanceMM = duration / 5.8;

  return distanceMM;
}

// ======================================================
// UPDATE ULTRASONIC SENSORS
// ======================================================

void updateUltrasonicSensors() {

  if (millis() - lastSensorRead < SENSOR_INTERVAL) {
    return;
  }

  lastSensorRead = millis();

  // ----------------------------------------------------
  // Read LEFT sensor
  // ----------------------------------------------------

  leftDistanceMM =
    readUltrasonicMM(TRIG_LEFT, ECHO_LEFT);

  // Small gap to reduce ultrasonic interference
  delayMicroseconds(500);

  // ----------------------------------------------------
  // Read RIGHT sensor
  // ----------------------------------------------------

  rightDistanceMM =
    readUltrasonicMM(TRIG_RIGHT, ECHO_RIGHT);

  // ----------------------------------------------------
  // CHECK FOR OBSTACLE
  // ----------------------------------------------------

  bool leftBlocked =
    leftDistanceMM < OBSTACLE_DISTANCE_MM;

  bool rightBlocked =
    rightDistanceMM < OBSTACLE_DISTANCE_MM;

  // ----------------------------------------------------
  // BUZZER
  // ----------------------------------------------------

  if (leftBlocked || rightBlocked) {

    digitalWrite(BUZZER_PIN, HIGH);

  } else {

    digitalWrite(BUZZER_PIN, LOW);
  }

  // ----------------------------------------------------
  // SERIAL MONITOR
  // ----------------------------------------------------

  Serial.print("LEFT: ");
  Serial.print(leftDistanceMM);
  Serial.print(" mm");

  Serial.print(" | RIGHT: ");
  Serial.print(rightDistanceMM);
  Serial.print(" mm");

  if (leftBlocked && rightBlocked) {

    Serial.println(" | BOTH SIDES BLOCKED");

  }
  else if (leftBlocked) {

    Serial.println(" | LEFT OBSTACLE");

  }
  else if (rightBlocked) {

    Serial.println(" | RIGHT OBSTACLE");

  }
  else {

    Serial.println(" | CLEAR");
  }
}

// ======================================================
// SAFETY CHECK
// ======================================================

void checkObstacleSafety() {

  bool leftBlocked =
    leftDistanceMM < OBSTACLE_DISTANCE_MM;

  bool rightBlocked =
    rightDistanceMM < OBSTACLE_DISTANCE_MM;

  // ====================================================
  // LEFT TURN
  // ====================================================

  if (currentMotion == LEFT) {

    if (leftBlocked) {

      Serial.println(
        "SAFETY STOP: LEFT TURN BLOCKED!"
      );

      stopMotors();
    }
  }

  // ====================================================
  // RIGHT TURN
  // ====================================================

  else if (currentMotion == RIGHT) {

    if (rightBlocked) {

      Serial.println(
        "SAFETY STOP: RIGHT TURN BLOCKED!"
      );

      stopMotors();
    }
  }

  // ====================================================
  // FORWARD
  // ====================================================

  else if (currentMotion == FORWARD) {

    // IMPORTANT:
    //
    // The sensors are mounted on the SIDES.
    //
    // Therefore, a side obstacle does NOT stop
    // forward movement.

  }

  // ====================================================
  // BACKWARD
  // ====================================================

  else if (currentMotion == BACKWARD) {

    // No rear ultrasonic sensors.
    //
    // Therefore backward movement is allowed.
  }
}

// ======================================================
// WEB COMMANDS
// ======================================================

// FORWARD
void commandForward() {

  // Side obstacles do NOT prevent forward movement.

  moveForward();

  server.send(200, "text/plain", "FORWARD");
}

// ------------------------------------------------------

// BACKWARD
void commandBackward() {

  moveBackward();

  server.send(200, "text/plain", "BACKWARD");
}

// ------------------------------------------------------

// LEFT
void commandLeft() {

  // Check LEFT side

  if (leftDistanceMM < OBSTACLE_DISTANCE_MM) {

    Serial.println(
      "LEFT COMMAND REJECTED - LEFT SIDE BLOCKED!"
    );

    stopMotors();

    server.send(
      200,
      "text/plain",
      "BLOCKED"
    );

    return;
  }

  turnLeft();

  server.send(
    200,
    "text/plain",
    "LEFT"
  );
}

// ------------------------------------------------------

// RIGHT
void commandRight() {

  // Check RIGHT side

  if (rightDistanceMM < OBSTACLE_DISTANCE_MM) {

    Serial.println(
      "RIGHT COMMAND REJECTED - RIGHT SIDE BLOCKED!"
    );

    stopMotors();

    server.send(
      200,
      "text/plain",
      "BLOCKED"
    );

    return;
  }

  turnRight();

  server.send(
    200,
    "text/plain",
    "RIGHT"
  );
}

// ------------------------------------------------------

// STOP
void commandStop() {

  stopMotors();

  server.send(
    200,
    "text/plain",
    "STOP"
  );
}

// ======================================================
// WEBPAGE
// ======================================================

void webpage() {

  String html = R"rawliteral(

<!DOCTYPE html>

<html>

<head>

<meta name="viewport"
content="width=device-width, initial-scale=1">

<title>G-SHIELD Rover</title>

<style>

body {
  font-family: Arial;
  text-align: center;
  background: #111;
  color: white;
}

button {
  width: 150px;
  height: 70px;
  margin: 10px;
  font-size: 22px;
  border-radius: 15px;
}

</style>

</head>

<body>

<h1>G-SHIELD</h1>

<h2>Rover Control</h2>

<button
onmousedown="startCommand('forward')"
onmouseup="stopCommand()"
ontouchstart="startCommand('forward')"
ontouchend="stopCommand()">
FORWARD
</button>

<br>

<button
onmousedown="startCommand('left')"
onmouseup="stopCommand()"
ontouchstart="startCommand('left')"
ontouchend="stopCommand()">
LEFT
</button>

<button
onmousedown="stopCommand()"
ontouchstart="stopCommand()">
STOP
</button>

<button
onmousedown="startCommand('right')"
onmouseup="stopCommand()"
ontouchstart="startCommand('right')"
ontouchend="stopCommand()">
RIGHT
</button>

<br>

<button
onmousedown="startCommand('backward')"
onmouseup="stopCommand()"
ontouchstart="startCommand('backward')"
ontouchend="stopCommand()">
BACKWARD
</button>

<script>

let interval;

function startCommand(command) {

  clearInterval(interval);

  fetch('/' + command);

  interval = setInterval(() => {

    fetch('/' + command);

  }, 100);

}

function stopCommand() {

  clearInterval(interval);

  fetch('/stop');

}

</script>

</body>

</html>

)rawliteral";

  server.send(
    200,
    "text/html",
    html
  );
}

// ======================================================
// SETUP
// ======================================================

void setup() {

  // ====================================================
  // SERIAL
  // ====================================================

  Serial.begin(115200);

  delay(500);

  Serial.println();
  Serial.println("==============================");
  Serial.println("       G-SHIELD ROVER");
  Serial.println("==============================");

  // ====================================================
  // MOTOR PINS
  // ====================================================

  pinMode(LEFT_RPWM, OUTPUT);
  pinMode(LEFT_LPWM, OUTPUT);

  pinMode(RIGHT_RPWM, OUTPUT);
  pinMode(RIGHT_LPWM, OUTPUT);

  // ====================================================
  // PWM
  // ====================================================

  ledcAttach(
    LEFT_RPWM,
    1000,
    8
  );

  ledcAttach(
    LEFT_LPWM,
    1000,
    8
  );

  ledcAttach(
    RIGHT_RPWM,
    1000,
    8
  );

  ledcAttach(
    RIGHT_LPWM,
    1000,
    8
  );

  // Start stopped
  stopMotors();

  // ====================================================
  // ULTRASONIC PINS
  // ====================================================

  pinMode(TRIG_LEFT, OUTPUT);
  pinMode(ECHO_LEFT, INPUT);

  pinMode(TRIG_RIGHT, OUTPUT);
  pinMode(ECHO_RIGHT, INPUT);

  digitalWrite(TRIG_LEFT, LOW);
  digitalWrite(TRIG_RIGHT, LOW);

  // ====================================================
  // BUZZER
  // ====================================================

  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(BUZZER_PIN, LOW);

  // ====================================================
  // WIFI ACCESS POINT
  // ====================================================

  WiFi.softAP(
    WIFI_NAME,
    WIFI_PASSWORD
  );

  Serial.println();

  Serial.print("WiFi Name: ");
  Serial.println(WIFI_NAME);

  Serial.print("WiFi IP: ");
  Serial.println(WiFi.softAPIP());

  // ====================================================
  // WEB SERVER
  // ====================================================

  server.on("/", webpage);

  server.on(
    "/forward",
    commandForward
  );

  server.on(
    "/backward",
    commandBackward
  );

  server.on(
    "/left",
    commandLeft
  );

  server.on(
    "/right",
    commandRight
  );

  server.on(
    "/stop",
    commandStop
  );

  server.begin();

  Serial.println();
  Serial.println("Web server started.");
  Serial.println("Serial Baud: 115200");
  Serial.println("Obstacle Threshold: 200 mm");
  Serial.println();
  Serial.println("LEFT  -> TRIG GPIO32 / ECHO GPIO34");
  Serial.println("RIGHT -> TRIG GPIO33 / ECHO GPIO35");
  Serial.println("BUZZER -> GPIO23");
  Serial.println("==============================");
}

// ======================================================
// MAIN LOOP
// ======================================================

void loop() {

  // Handle Wi-Fi commands
  server.handleClient();

  // Read ultrasonic sensors
  updateUltrasonicSensors();

  // Apply safety conditions
  checkObstacleSafety();
}