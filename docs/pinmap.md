# pin map

board: seeed xiao esp32c3

| part | part pin | esp32 pin | notes |
|---|---|---|---|
| xiao | 3V3 | + rail | red jumper. 3V3 not 5V, 5V puts too much voltage on the sensor and i2c pins |
| xiao | GND | - rail | black jumper |
| max30101 | 3.3V | + rail | qwiic cable red |
| max30101 | GND | - rail | qwiic cable black |
| max30101 | SDA | D4 (GPIO 6) | qwiic cable blue |
| max30101 | SCL | D5 (GPIO 7) | qwiic cable yellow |

the max30101's header pins aren't wired, everything goes through the qwiic cable

the red and black wires across the far end of the breadboard connect the left rails to the right rails,
so both sides are 3.3V / GND

## photos

upload any wiring photos in `docs/photos/` so we can keep track of our progress

| photo | what it shows |
|---|---|
| [nov7.jpg](photos/nov7.jpg) | first working setup: xiao + max30101 over qwiic, lsm6dsox placed but not connected |
