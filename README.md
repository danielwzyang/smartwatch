# smartwatch

esp32 + max30101 ppg sensor + accelerometer + display

board: seeed xiao esp32c3 (pick "XIAO_ESP32C3" in arduino ide)

```
arduino/        arduino sketches that go on the esp32 (each sketch needs its own folder)
tools/          any scripts that run on your laptop (ex: dataset stuff)
data/sample/    small processed test data that's ok to commit
data/raw/       any big datasets (git ignores this folder but we might need it when testing)
docs/           any notes on pin mapping, photos of wiring etc
```

arduino libraries:
- SparkFun MAX3010x Pulse and Proximity Sensor Library (ppg)
- Adafruit LSM6DS (accelerometer)
- Adafruit SHARP Memory Display + Adafruit GFX Library (display)

## data

PPG-DaLiA (Reiss et al., 2019, CC BY 4.0): https://doi.org/10.24432/C53890

## setup (once)

1. accept the github invite
2. clone it:
   ```
   git clone https://github.com/danielwzyang/smartwatch.git
   ```
   pushing needs a login.
   github doesn't take your password anymore, so either install the github cli and run `gh auth login`, or set up an ssh key
3. install arduino ide 2: https://www.arduino.cc/en/software
4. arduino ide -> boards manager (ctrl + shift + b) -> install "esp32 by Espressif Systems"

## every time

```
git pull                        # before you start
# do your stuff, only edit your own folder (like arduino/ppg_read/)
git add .
git commit -m "what you did"
git push
```

no branches to save time, just push to main

if you can't figure it out just email me the files

