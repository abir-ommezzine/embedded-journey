#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

#define DHT_PIN   15
#define DHT_TYPE  DHT22
#define OLED_ADDR 0x3C

DHT dht(DHT_PIN, DHT_TYPE);
Adafruit_SSD1306 oled(128, 64, &Wire, -1);

// A message that travels through the queues
struct Reading { float temp; float hum; };

// The shared "notebook" where tasks write how long their work took
struct Stats { uint32_t readUs; uint32_t displayUs; uint32_t dropped; };
Stats stats = {0, 0, 0};

QueueHandle_t     displayQueue;
QueueHandle_t     logQueue;
SemaphoreHandle_t statsMutex;   // the key to the notebook

// ---------- Task 1: read the sensor every 2 seconds ----------
void readerTask(void *param) {
  TickType_t lastWake = xTaskGetTickCount();
  for (;;) {
    uint32_t t0 = micros();
    Reading r;
    r.temp = dht.readTemperature();
    r.hum  = dht.readHumidity();
    uint32_t took = micros() - t0;

    if (!isnan(r.temp) && !isnan(r.hum)) {
      // timeout 0 = do not wait if the queue is full
      bool ok1 = xQueueSend(displayQueue, &r, 0) == pdTRUE;
      bool ok2 = xQueueSend(logQueue, &r, 0) == pdTRUE;

      if (xSemaphoreTake(statsMutex, portMAX_DELAY) == pdTRUE) {
        stats.readUs = took;
        if (!ok1 || !ok2) stats.dropped++;
        xSemaphoreGive(statsMutex);
      }
    }
    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(2000));   // keeps an exact 2 s rhythm
  }
}

// ---------- Task 2: show the latest reading on the screen ----------
void displayTask(void *param) {
  Reading r;
  for (;;) {
    // sleeps here, using no CPU, until a message arrives
    if (xQueueReceive(displayQueue, &r, portMAX_DELAY) == pdTRUE) {
      uint32_t t0 = micros();

      oled.clearDisplay();
      oled.setTextSize(1);
      oled.setCursor(0, 0);
      oled.println("RTOS station");
      oled.setTextSize(2);
      oled.setCursor(0, 20);
      oled.print(r.temp, 1);
      oled.println(" C");
      oled.setTextSize(1);
      oled.setCursor(0, 50);
      oled.print("Humidity: ");
      oled.print(r.hum, 0);
      oled.println(" %");
      oled.display();
      uint32_t took = micros() - t0;
      if (xSemaphoreTake(statsMutex, portMAX_DELAY) == pdTRUE) {
        stats.displayUs = took;
        xSemaphoreGive(statsMutex);
      }
    }
  }
}

// ---------- Task 3: print a log line ----------
void loggerTask(void *param) {
  Reading r;
  for (;;) {
    if (xQueueReceive(logQueue, &r, portMAX_DELAY) == pdTRUE) {
      Stats s;
      if (xSemaphoreTake(statsMutex, portMAX_DELAY) == pdTRUE) {
        s = stats;                       // copy fast, then give the key back
        xSemaphoreGive(statsMutex);
      }
      Serial.printf("[%lu ms] T=%.1f C  H=%.0f %% | read=%lu us  display=%lu us  dropped=%lu | stack left=%u bytes\n",
                    (unsigned long)millis(), r.temp, r.hum,
                    (unsigned long)s.readUs, (unsigned long)s.displayUs,
                    (unsigned long)s.dropped,
                    (unsigned)uxTaskGetStackHighWaterMark(NULL));
    }
  }
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  Wire.begin(21, 22);
  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED not found");
    while (true) delay(1000);
  }
  oled.setTextColor(SSD1306_WHITE);

  displayQueue = xQueueCreate(5,  sizeof(Reading));   // room for 5 messages
  logQueue     = xQueueCreate(10, sizeof(Reading));   // room for 10 messages
  statsMutex   = xSemaphoreCreateMutex();

  //              function     name       stack  param priority handle
  xTaskCreate(readerTask,  "reader",  4096, NULL, 2, NULL);
  xTaskCreate(displayTask, "display", 4096, NULL, 1, NULL);
  xTaskCreate(loggerTask,  "logger",  4096, NULL, 1, NULL);
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));   // the main loop has nothing to do now
}