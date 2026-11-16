#include <Wire.h>
#include <LittleFS.h>
#include <math.h>
#include "MAX30105.h"
#include <Adafruit_LSM6DSOX.h>
#include "heartrate.h"

MAX30105 ppg_sensor;
Adafruit_LSM6DSOX accel_sensor;

// one line per minute
const char *hr_path = "/hr.csv";
const char *hr_header = "seconds_since_boot,avg_bpm,valid_seconds";

// one line per 5 seconds
const char *angle_path = "/angle.csv";
const char *angle_header = "seconds_since_boot,avg_angle";
const int ANGLE_SECONDS = 5;

// prints one log file over serial
void dump_log(const char *path) {
  File f = LittleFS.open(path, "r");

  if (!f) {
    Serial.print("no file: ");
    Serial.println(path);
    return;
  }

  while (f.available()) Serial.write(f.read());

  f.close();
}

void dump_logs() {
  dump_log(hr_path);
  dump_log(angle_path);
}

// starts the log over with just the column names
void clear_log(const char *path, const char *header) {
  File f = LittleFS.open(path, "w");
  f.println(header);
  f.close();
}

void clear_logs() {
  clear_log(hr_path, hr_header);
  clear_log(angle_path, angle_header);
  Serial.println("logs cleared");
}

void append_line(const char *path, const char *line) {
  File f = LittleFS.open(path, "a");
  f.println(line);
  f.close();
}

void fail(const char *msg) {
  while (true) {
    Serial.println(msg);
    delay(1000);
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin();

  if (!ppg_sensor.begin()) fail("ppg sensor not found");

  ppg_sensor.setup(hr::BRIGHTNESS, 1, 3, hr::RATE, 411, 4096);
  ppg_sensor.setPulseAmplitudeRed(0);
  ppg_sensor.setPulseAmplitudeIR(0);

  if (!accel_sensor.begin_I2C()) fail("accelerometer not found");

  accel_sensor.setAccelRange(LSM6DS_ACCEL_RANGE_2_G); // +/- 2g
  accel_sensor.setAccelDataRate(LSM6DS_RATE_104_HZ);

  // formatOnFail = true
  // the first time the fs starts there's nothing to mount so littlefs will format it
  if (!LittleFS.begin(true)) fail("littlefs failed");

  // brand new files: write the column names first
  if (!LittleFS.exists(hr_path)) clear_log(hr_path, hr_header);
  if (!LittleFS.exists(angle_path)) clear_log(angle_path, angle_header);

  // mark each power-on, since seconds_since_boot starts over at 0
  append_line(hr_path, "boot,,");
  append_line(angle_path, "boot,");
}

void loop() {
  static unsigned long samples = 0;

  static float bpm_sum = 0;
  static int valid_seconds = 0;
  static float angle_sum = 0;
  static int angle_count = 0;

  // serial commands: d = dump the logs, c = clear them
  if (Serial.available()) {
    char cmd = Serial.read();
    if (cmd == 'd') dump_logs();
    else if (cmd == 'c') clear_logs();
    else if (cmd == 'x') Serial.println("logger running.");
  }

  ppg_sensor.check();
  while (ppg_sensor.available()) {
    float green = ppg_sensor.getFIFOGreen();
    ppg_sensor.nextSample();

    // convert accel from m/s^2 to g
    sensors_event_t a, g, t;
    accel_sensor.getEvent(&a, &g, &t);
    float x = a.acceleration.x / SENSORS_GRAVITY_STANDARD;
    float y = a.acceleration.y / SENSORS_GRAVITY_STANDARD;
    float z = a.acceleration.z / SENSORS_GRAVITY_STANDARD;

    hr::addSample(green, x, y, z);
    samples++;

    // wrist angle in degrees: how far z tilts up or down from the x-y plane
    angle_sum += atan2f(z, sqrtf(x * x + y * y)) * 180 / M_PI;
    angle_count++;

    // once a second: run the heart rate algorithm
    if (samples % hr::RATE == 0) {
      float bpm = hr::getBPM();

      if (bpm) {
        bpm_sum += bpm;
        valid_seconds++;
      }
    }

    // every 5 seconds: save the average angle
    if (samples % (ANGLE_SECONDS * hr::RATE) == 0) {
      char line[40];
      snprintf(line, sizeof(line), "%lu,%.1f", samples / hr::RATE, angle_sum / angle_count);
      append_line(angle_path, line);
      angle_sum = 0;
      angle_count = 0;
    }

    // once a minute: save the average bpm
    if (samples % (60 * hr::RATE) == 0) {
      float avg = valid_seconds ? bpm_sum / valid_seconds : 0;
      char line[40];
      snprintf(line, sizeof(line), "%lu,%.1f,%d", samples / hr::RATE, avg, valid_seconds);
      append_line(hr_path, line);
      bpm_sum = 0;
      valid_seconds = 0;
    }
  }
}
