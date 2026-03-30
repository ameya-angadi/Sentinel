/*
 * Project Name: Sentinel
 * Designed For: Raspberry Pi Pico/Pico W
 *
 *
 * License: GPL3+
 * This project is licensed under the GNU General Public License v3.0 or later.
 * You are free to use, modify, and distribute this software under the terms
 * of the GPL, as long as you preserve the original license and credit the original
 * author. For more details, see <https://www.gnu.org/licenses/gpl-3.0.en.html>.
 *
 * Copyright (C) 2026  Ameya Angadi
 *
 * Code Created And Maintained By: Ameya Angadi
 * Last Modified On: March 30, 2026
 * Version: 1.0.0
 *
 */

#include <Wire.h>
#include <DHT.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

#define DHTPIN 15
DHT dht(DHTPIN, DHT11);
Adafruit_BMP280 bmp(&Wire); 
Adafruit_MPU6050 mpu;

// Calibration Globals
float baseMag = 0;
float offX = 0, offY = 0, offZ = 0;
float filtX = 0, filtY = 0, filtZ = 0;
const float alpha = 0.8; // Filter strength (0.8 to 0.9 is best)

void initSensors() {
  Wire.end(); Wire1.end();
  Wire.setSDA(4); Wire.setSCL(5); Wire.begin();
  bmp.begin(0x76);
  Wire1.setSDA(2); Wire1.setSCL(3); Wire1.begin();
  mpu.begin(0x68, &Wire1);
  dht.begin();
}

void calibrateMPU() {
  Serial.println("Calibrating... Keep Still!");
  float sumX = 0, sumY = 0, sumZ = 0, sumMag = 0;
  int samples = 100;

  for(int i = 0; i < samples; i++) {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    sumX += a.acceleration.x;
    sumY += a.acceleration.y;
    sumZ += a.acceleration.z;
    sumMag += sqrt(pow(a.acceleration.x, 2) + pow(a.acceleration.y, 2) + pow(a.acceleration.z, 2));
    delay(10);
  }
  
  offX = sumX / samples;
  offY = sumY / samples;
  offZ = sumZ / samples;
  baseMag = sumMag / samples;
  Serial.printf("Baseline Set: %.2f m/s^2\n", baseMag);
}

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200); // TX: GP0
  initSensors();
  calibrateMPU(); // Run Tare Calibration
}

void loop() {
  sensors_event_t a, g, temp_mpu;
  if (!mpu.getEvent(&a, &g, &temp_mpu)) { initSensors(); return; }

  // 1. DYNAMIC HIGH-PASS FILTER (The "Gravity Canceller")
  // We estimate gravity (Low Pass)
  filtX = (alpha * filtX) + ((1.0 - alpha) * a.acceleration.x);
  filtY = (alpha * filtY) + ((1.0 - alpha) * a.acceleration.y);
  filtZ = (alpha * filtZ) + ((1.0 - alpha) * a.acceleration.z);

  // We subtract gravity to get only Linear Acceleration (High Pass)
  float linX = a.acceleration.x - filtX;
  float linY = a.acceleration.y - filtY;
  float linZ = a.acceleration.z - filtZ;

  // 2. CALCULATE VIBRATION INTENSITY (G-Force)
  // This is the "Magnitude" of just the vibration components
  float vibeMag = sqrt(pow(linX, 2) + pow(linY, 2) + pow(linZ, 2)) / 9.81;

  // 3. GET ENVIRONMENTAL DATA
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  float p = bmp.readPressure() / 100.0F;

  // --- PACKET SENDING ---
  // $[T, H, P, RelX, RelY, RelZ, RelVibe]*
  Serial1.print("$[");
  Serial1.print(t, 1);    Serial1.print(",");
  Serial1.print(h, 0);    Serial1.print(",");
  Serial1.print(p, 1);    Serial1.print(",");
  Serial1.print(linX, 2); Serial1.print(","); // Cleaned X
  Serial1.print(linY, 2); Serial1.print(","); // Cleaned Y
  Serial1.print(linZ, 2); Serial1.print(","); // Cleaned Z
  Serial1.print(vibeMag, 2);                  // Cleaned Combined G
  Serial1.println("]*");

  delay(100); 
}