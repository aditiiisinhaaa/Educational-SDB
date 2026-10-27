#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_VL53L0X.h>
#include "MPU9250.h"

#define SDA_PIN 21
#define SCL_PIN 22

const uint8_t XSHUT_PINS[3]  = {25, 26, 27};
const uint8_t LIDAR_ADDR[3]  = {0x30, 0x31, 0x32};
const uint8_t BMP_ADDR       = 0x76;   
const uint8_t MPU_ADDR       = 0x68;

const uint32_t SAMPLE_MS = 50;         


Adafruit_BMP280   bmp;
MPU9250           mpu;
Adafruit_VL53L0X  lox[3];

int lidarMM[3] = {-1, -1, -1};         
uint32_t lastSample = 0;



bool initLidars() {
  
  for (int i = 0; i < 3; i++) {
    pinMode(XSHUT_PINS[i], OUTPUT);
    digitalWrite(XSHUT_PINS[i], LOW);
  }
  delay(10);

  
  for (int i = 0; i < 3; i++) {
    digitalWrite(XSHUT_PINS[i], HIGH);
    delay(10);
    if (!lox[i].begin(LIDAR_ADDR[i], false, &Wire)) {
      Serial.printf("VL53L0X #%d failed\n", i + 1);
      return false;
    }
    lox[i].startRangeContinuous(30);   
  }
  return true;
}

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);

  
  if (!initLidars()) while (1) delay(1000);

  
  if (!bmp.begin(BMP_ADDR)) {
    Serial.println("BMP280 not found (try 0x77)");
    while (1) delay(1000);
  }
  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                  Adafruit_BMP280::SAMPLING_X2,    // temp
                  Adafruit_BMP280::SAMPLING_X16,   // pressure
                  Adafruit_BMP280::FILTER_X16,
                  Adafruit_BMP280::STANDBY_MS_63);

  
  if (!mpu.setup(MPU_ADDR)) {
    Serial.println("MPU9250 not found");
    while (1) delay(1000);
  }

  
  Serial.println("ms,ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps,mx_mG,my_mG,mz_mG,"
                 "roll,pitch,yaw,temp_C,press_Pa,alt_m,d1_mm,d2_mm,d3_mm");
}

void loop() {
  
  mpu.update();

  
  for (int i = 0; i < 3; i++) {
    if (lox[i].isRangeComplete()) {
      uint16_t r = lox[i].readRange();
      uint8_t st = lox[i].readRangeStatus();
      lidarMM[i] = (st == 4 || r >= 8190) ? -1 : r;   
    }
  }

  
  if (millis() - lastSample >= SAMPLE_MS) {
    lastSample = millis();

    float temp  = bmp.readTemperature();
    float press = bmp.readPressure();
    float alt   = bmp.readAltitude(1013.25);   

    Serial.printf("%lu,%.3f,%.3f,%.3f,%.2f,%.2f,%.2f,%.1f,%.1f,%.1f,%.2f,%.2f,%.2f,%.2f,%.1f,%.2f,%d,%d,%d\n",
      millis(),
      mpu.getAccX(), mpu.getAccY(), mpu.getAccZ(),
      mpu.getGyroX(), mpu.getGyroY(), mpu.getGyroZ(),
      mpu.getMagX(), mpu.getMagY(), mpu.getMagZ(),
      mpu.getRoll(), mpu.getPitch(), mpu.getYaw(),
      temp, press, alt,
      lidarMM[0], lidarMM[1], lidarMM[2]);
  }
}