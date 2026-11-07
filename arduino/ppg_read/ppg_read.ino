// written with help from claude, not tested on hardware yet

// ppg_read: reads the max30101 and prints the raw ppg values

// what you need
// - seeed xiao esp32c3 (headers already soldered)
// - sparkfun max30101 (qwiic)
// - qwiic-to-breadboard jumper cable
// - usb-c data cable (charge-only cables won't work, the board will look dead)
// - breadboard

// checking if a cable is a data cable
// plug the xiao in and see if a new port shows up in the arduino ide port dropdown
// or run ls /dev/ttyACM* (linux) or ls /dev/cu.* (mac) before and after plugging in
// if nothing new shows up it's probably charge-only
// cables that came with a phone or laptop are almost always data cables

// setup (once per laptop)
// 1. arduino ide -> boards manager -> install "esp32 by Espressif Systems"
// 2. arduino ide -> library manager -> install "SparkFun MAX3010x Pulse and Proximity Sensor Library"
// 3. linux only: if upload says permission denied, run sudo usermod -aG dialout $USER then log out and back in

// step 1: test the board by itself (nothing plugged in)
// 1. put the xiao on the breadboard and plug it into your laptop
// 2. top dropdown -> pick "XIAO_ESP32C3" and its port
// 3. tools -> "USB CDC On Boot" -> Enabled (otherwise serial prints nothing)
// 4. upload this sketch
// 5. tools -> serial monitor, set baud to 115200
// 6. you should see "max30101 not found, check wiring"
// that's good, it means the board, upload and serial all work

// step 2: wire the sensor (unplug usb first)
// plug the qwiic end into either port on the max30101, then the other end into the breadboard
// black (GND) -> GND
// red (3.3V) -> 3V3, not 5V, 5V will fry the sensor
// blue (SDA) -> D4
// yellow (SCL) -> D5

// step 3: see your pulse
// 1. plug usb back in and upload again
// 2. tools -> serial plotter, baud 115200
// 3. rest your fingertip on the sensor with a light touch, pressing hard squeezes the blood out and kills the signal
// 4. hold still, you should see three lines (green, ir, red) going up and down with your pulse about once a second
// 5. uncheck ir and red in the plotter to just look at green, that's the one our algorithm uses

// troubleshooting
// no port shows up: try another usb-c cable (probably charge-only) or another usb port
// upload fails / "failed to connect": hold BOOT on the xiao, press and release RESET, let go of BOOT, upload again
// serial monitor / plotter is empty: check "USB CDC On Boot" is Enabled and baud is 115200, then re-upload
// "max30101 not found": check the wiring, sda/scl are the easiest to swap, make sure it's on 3V3
// lines are flat or tiny: finger isn't on the sensor or you're pressing too hard, then try turning up ledBrightness (0x3F, then 0x7F)
// lines are flat at the very top: the signal is maxed out, turn ledBrightness down
// wave is super noisy: hold still and block room light with your other hand

// when it works
// 1. fill in docs/pinmap.md
// 2. take a photo of the wiring and put it in docs/photos/
// 3. if you changed any settings, leave a comment saying what and why
// 4. commit and push (see readme)

#include <Wire.h>
// sparkfun's library is named after the max30105 but works for the max30101 too
#include "MAX30105.h"

MAX30105 sensor;

void setup() {
  Serial.begin(115200);
  // give the serial monitor a sec to connect
  delay(1000);

  // default i2c pins on the xiao c3: SDA = D4, SCL = D5
  Wire.begin();
  if (!sensor.begin(Wire, I2C_SPEED_FAST)) {
    while (true) {
      Serial.println("max30101 not found, check wiring");
      delay(1000);
    }
  }

  // these settings matter, our algorithm assumes 50 samples/sec and green light

  // 0 = off, 255 = max
  // higher brightness bc ppg on wrist vs finger is smaller
  byte ledBrightness = 0x7F;
  // no averaging, keeps the output at exactly 50 hz
  byte sampleAverage = 1;
  // 3 = red + ir + green
  byte ledMode = 3;
  // samples per second
  int sampleRate = 50;
  // longest pulse = most sensitive
  int pulseWidth = 411;
  int adcRange = 4096;

  sensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);

  sensor.setPulseAmplitudeRed(0);
  sensor.setPulseAmplitudeIR(0);
}

void loop() {
  // pull any new samples off the sensor
  static int n = 0;
  sensor.check();

  while (sensor.available()) {
    // "name:value" format makes the serial plotter label each line
    if (n++ % 5 == 0) {
      // print every 5 samples bc the serial plotter only shows 50 points
      Serial.print("green:");
      Serial.println(sensor.getFIFOGreen());
    }

    sensor.nextSample();
  }
}
