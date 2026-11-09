#include <Wire.h>
#include "MAX30105.h"
// https://github.com/sparkfun/SparkFun_MAX3010x_Sensor_Library
#include "heartrate.h"

MAX30105 ppg_sensor;

void setup() {
  Serial.begin(115200);
  Wire.begin();

  while (!ppg_sensor.begin()) {
    Serial.println("ppg sensor not found");
    delay(1000);
  }
  
  // setup(brightness, averaging, mode, rate, pulse, adc)
  ppg_sensor.setup(hr::BRIGHTNESS, 1, 3, hr::RATE, 411, 4096);
    // 0x7F = led brightness (31/255 base 10)
    // 1 = sampling average of 1 (no averaging)
    // 3 = red + ir + green (only mode with green)
    // RATE = 50 Hz (from heartrate.h)
    // 411 = how long the led pulse flashes for in microseconds
    // 411 is the longest / most sensitive
    // 4096 = largest current that the sensor can measure (adc range)
    // idk lowkey arbitrarily picked from example

  // turn off red and ir to save battery
  ppg_sensor.setPulseAmplitudeRed(0);
  ppg_sensor.setPulseAmplitudeIR(0);
}

void loop() {
  static int samples = 0;
  static int bpm = 0;
  ppg_sensor.check();

  while (ppg_sensor.available()) {
    // accelerometer isn't hooked up yet so just pass in accel = (0, 0, 0)
    // FIFO for oldest samples first
    hr::addSample(ppg_sensor.getFIFOGreen(), 0, 0, 0);
    ppg_sensor.nextSample();
    samples++;

    // only plot every 4th sample because the plotter only shows 50 pts
    // 50 pts = 4 seconds
    if (samples % hr::RATE == 0) bpm = hr::getBPM();
    if (samples % 4 == 0) {
      Serial.print("pulse:");
      Serial.print(hr::filtered_ppg[hr::N - 1]);
      Serial.print(",bpm:");
      if (bpm) Serial.println(bpm);
      else Serial.println("---");
    }

  }
}
