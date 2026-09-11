#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>

// =====================================================
// MPU6050
// =====================================================

#define MPU_ADDR 0x68

#define SDA_PIN 21
#define SCL_PIN 22


// =====================================================
// ROVER WI-FI
// =====================================================

const char* WIFI_NAME = "ROVER_ESP32";
const char* WIFI_PASSWORD = "12345678";

const char* ROVER_IP = "192.168.4.1";


// =====================================================
// GESTURE SETTINGS
// =====================================================

// Increase this if the glove is too sensitive
const float TILT_THRESHOLD = 0.40;


// =====================================================
// GESTURE TYPES
// =====================================================

enum Gesture {
  STOP,
  FORWARD,
  BACKWARD,
  LEFT,
  RIGHT
};


// Current and previous gesture
Gesture currentGesture = STOP;
Gesture lastGesture = STOP;


// =====================================================
// MPU6050 INITIALIZATION
// =====================================================

void setupMPU()
{
  Wire.begin(SDA_PIN, SCL_PIN);

  delay(100);

  // Wake up MPU6050
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0x00);
  Wire.endTransmission();

  delay(100);
}


// =====================================================
// READ MPU6050 ACCELEROMETER
// =====================================================

bool readMPU(float &ax, float &ay, float &az)
{
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);

  if (Wire.endTransmission(false) != 0)
  {
    return false;
  }

  Wire.requestFrom(MPU_ADDR, 6);

  if (Wire.available() < 6)
  {
    return false;
  }

  int16_t rawAX = (Wire.read() << 8) | Wire.read();
  int16_t rawAY = (Wire.read() << 8) | Wire.read();
  int16_t rawAZ = (Wire.read() << 8) | Wire.read();

  // ±2g range
  ax = rawAX / 16384.0;
  ay = rawAY / 16384.0;
  az = rawAZ / 16384.0;

  return true;
}


// =====================================================
// DETECT GESTURE
// =====================================================

Gesture detectGesture(float ax, float ay)
{
  // Forward
  if (ay < -TILT_THRESHOLD)
  {
    return FORWARD;
  }

  // Backward
  if (ay > TILT_THRESHOLD)
  {
    return BACKWARD;
  }

  // Left
  if (ax < -TILT_THRESHOLD)
  {
    return LEFT;
  }

  // Right
  if (ax > TILT_THRESHOLD)
  {
    return RIGHT;
  }

  // Neutral hand = STOP
  return STOP;
}


// =====================================================
// GESTURE NAME
// =====================================================

const char* gestureName(Gesture gesture)
{
  switch (gesture)
  {
    case FORWARD:
      return "FORWARD";

    case BACKWARD:
      return "BACKWARD";

    case LEFT:
      return "LEFT";

    case RIGHT:
      return "RIGHT";

    default:
      return "STOP";
  }
}


// =====================================================
// SEND COMMAND TO ROVER
// =====================================================

void sendCommand(Gesture gesture)
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WiFi disconnected");
    return;
  }

  String path;

  switch (gesture)
  {
    case FORWARD:
      path = "/forward";
      break;

    case BACKWARD:
      path = "/backward";
      break;

    case LEFT:
      path = "/left";
      break;

    case RIGHT:
      path = "/right";
      break;

    default:
      path = "/stop";
      break;
  }

  String url = "http://" + String(ROVER_IP) + path;

  Serial.print("Sending: ");
  Serial.println(url);

  HTTPClient http;

  http.begin(url);
  http.setTimeout(300);

  int responseCode = http.GET();

  Serial.print("Response: ");
  Serial.println(responseCode);

  http.end();
}


// =====================================================
// CONNECT TO ROVER WI-FI
// =====================================================

void connectToRover()
{
  Serial.println();
  Serial.println("Connecting to ROVER_ESP32...");

  WiFi.mode(WIFI_STA);

  WiFi.begin(WIFI_NAME, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected");

  Serial.print("Controller IP: ");
  Serial.println(WiFi.localIP());

  Serial.print("Rover IP: ");
  Serial.println(ROVER_IP);
}


// =====================================================
// SETUP
// =====================================================

void setup()
{
  Serial.begin(115200);

  delay(1000);

  Serial.println();
  Serial.println("==============================");
  Serial.println(" G-SHIELD GESTURE CONTROLLER");
  Serial.println("==============================");

  setupMPU();

  Serial.println("MPU6050 ready");

  connectToRover();

  // Always start with rover stopped
  sendCommand(STOP);
}


// =====================================================
// LOOP
// =====================================================

void loop()
{
  float ax;
  float ay;
  float az;

  // ---------------------------------------------------
  // Read MPU
  // ---------------------------------------------------

  if (!readMPU(ax, ay, az))
  {
    Serial.println("MPU6050 read error");
    delay(100);
    return;
  }


  // ---------------------------------------------------
  // Detect gesture
  // ---------------------------------------------------

  currentGesture = detectGesture(ax, ay);


  // ---------------------------------------------------
  // Print values
  // ---------------------------------------------------

  Serial.print("AX: ");
  Serial.print(ax, 2);

  Serial.print("  AY: ");
  Serial.print(ay, 2);

  Serial.print("  AZ: ");
  Serial.print(az, 2);

  Serial.print("  Gesture: ");
  Serial.println(gestureName(currentGesture));


  // ---------------------------------------------------
  // Send only when gesture changes
  // ---------------------------------------------------

  if (currentGesture != lastGesture)
  {
    sendCommand(currentGesture);

    lastGesture = currentGesture;
  }


  delay(100);
}