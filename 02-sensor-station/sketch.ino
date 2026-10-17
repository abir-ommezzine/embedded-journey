#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MPU6050.h>
#include <DHT.h>

#define DHT_PIN   15
#define DHT_TYPE  DHT22
#define SCREEN_W  128
#define SCREEN_H  64
#define OLED_ADDR 0x3C

DHT dht(DHT_PIN, DHT_TYPE);
Adafruit_SSD1306 oled(SCREEN_W, SCREEN_H, &Wire, -1);
Adafruit_MPU6050 mpu;

float smoothTemp = NAN;      // the smoothed temperature
float hum = NAN;             // the last humidity value
const float ALPHA = 0.3;

unsigned long lastDht = 0;      // when we last read the DHT22
unsigned long lastScreen = 0;   // when we last updated the screen

void setup() {
  Serial.begin(115200);
  dht.begin();
  Wire.begin(21, 22);

  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED not found");
    while (true) delay(1000);
  }
  if (!mpu.begin(0x68, &Wire)) {
    Serial.println("MPU6050 not found");
    while (true) delay(1000);
  }
  oled.setTextColor(SSD1306_WHITE);
}

void loop() {
  unsigned long now = millis();          // time since start, in milliseconds

  // Job 1: read the DHT22 every 2 seconds
  if (lastDht == 0 || now - lastDht >= 2000) {
    lastDht = now;
    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (isnan(t) || isnan(h)) {
      Serial.println("Sensor read failed");
    } else {
      hum = h;
      if (isnan(smoothTemp)) smoothTemp = t;
      else smoothTemp = ALPHA * t + (1 - ALPHA) * smoothTemp;
      Serial.print("raw=");      Serial.print(t, 1);
      Serial.print("  smooth="); Serial.println(smoothTemp, 1);
    }
  }

  // Job 2: read the MPU6050 and redraw the screen every 200 ms
  if (now - lastScreen >= 200) {
    lastScreen = now;

    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);

    oled.clearDisplay();
    oled.setTextSize(1);

    oled.setCursor(0, 0);
    oled.println("Sensor station");

    oled.setCursor(0, 14);
    oled.print("Temp: ");
    if (isnan(smoothTemp)) oled.print("--");
    else oled.print(smoothTemp, 1);
    oled.println(" C");

    oled.setCursor(0, 24);
    oled.print("Hum : ");
    if (isnan(hum)) oled.print("--");
    else oled.print(hum, 0);
    oled.println(" %");

    oled.setCursor(0, 40);
    oled.println("Accel (m/s2)");

    oled.setCursor(0, 52);
    oled.print("X:"); oled.print(a.acceleration.x, 1);
    oled.print(" Y:"); oled.print(a.acceleration.y, 1);
    oled.print(" Z:"); oled.print(a.acceleration.z, 1);

    oled.display();                      // nothing appears until this line
  }
}