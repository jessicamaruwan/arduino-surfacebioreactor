# arduino-surfacebioreactor
Mini ESP32 bioreactor controller (temperature, CO₂, oxygen concentrator timer)
# Yarrowia lipolytica Surface Culture Bioreactor Controller (ESP32)

Automatic control and data logging for a yeast surface culture bioreactor, by **Jessica Maruwan**.

## Background

Lipids and fats from food and agricultural production are an underused waste stream. The yeast *Yarrowia lipolytica* can turn this lipid biowaste into **astaxanthin**, a valuable red antioxidant. Astaxanthin is usually made with algae, which need light and therefore a large surface area. Yeast grows without light, so it can be cultivated far more compactly.

The yeast needs oxygen, a nitrogen source (YNB-CA medium) and a carbon source (glucose or lipid). Astaxanthin production is triggered by a high carbon-to-nitrogen (C/N) ratio, and humidity appears to help growth.

The earlier version of this reactor had three problems:
- The oxygen concentrator had to be switched on and off by hand
- There was no temperature control
- There was no CO₂ measurement (used to estimate cell growth) and no humidity measurement

## What this code does

1. **PID temperature control:** a Peltier thermoelectric module (TEC1-12706) is driven through a MOSFET with an ESP32 PWM signal to hold the reactor at **28 °C**. It currently heats only. Warming the reactor from 22 °C to 28 °C takes about an hour.
2. **Oxygen supply control:** a stepper motor with a linear-motion converter pushes a lead against the oxygen concentrator's touchscreen to press its power button. The concentrator is switched on at start-up. Every 22 hours it is switched off for 2 hours to rest, then switched back on.
3. **Monitoring:** temperature and humidity come from a DHT11 sensor, and CO₂ from a K-30 sensor.
4. **Data logging:** humidity, temperature and CO₂ are sent to a Google Sheet over WiFi through an IFTTT webhook.

The three tasks run side by side using `millis()` timers instead of `delay()`:

| Event | Interval | Task |
|---|---|---|
| 1 | 1 minute | Send readings to Google Sheets |
| 2 | 10 seconds | Read sensors, print to the Serial Monitor, update the PID output |
| 3 | 22 hours | Cycle the oxygen concentrator off for 2 hours, then back on |

## Hardware

| Component | Role | ESP32 pin(s) |
|---|---|---|
| ESP32 (ESP-WROOM-32) | Main controller with WiFi | — |
| DHT11 | Temperature and humidity sensor | GPIO 32 |
| K-30 CO₂ sensor + Arduino Uno | The CO₂ sensor did not work directly with the ESP32, so an Arduino reads it and passes the value on over serial | RX2 = GPIO 16, TX2 = GPIO 17 |
| Logic level converter | Converts between the Arduino's 5 V and the ESP32's 3.3 V | — |
| IRLB8721 MOSFET | Switches the Peltier from the PWM signal (5 kHz, 8-bit). Its low gate threshold voltage lets the ESP32 turn it on directly. A resistor holds the gate low so it isn't left floating | GPIO 4 |
| Peltier TEC1-12706 + 40 mm heat sink with fan | Heating. The other side of the Peltier sits on an aluminium sheet that covers the reactor | — |
| 28BYJ-48 stepper motor + ULN2003 driver | Presses the concentrator's touchscreen (2.3 revolutions to reach it) | GPIO 12, 27, 14, 26 (IN1, IN3, IN2, IN4) |
| 5 V 4 A power supply | Peltier and stepper motor | — |
| 12 V 0.3 A power supply | Heat-sink fan | — |

Power the stepper motor from the external supply, not from the ESP32.

### Approximate cost (one reactor)

| Part | Cost (USD) |
|---|---|
| ESP32 | 11 |
| Arduino Uno | 19 |
| Logic level converter (SparkFun) | 3.50 |
| K-30 CO₂ sensor | 99 |
| 28BYJ-48 stepper + ULN2003 driver | 8 |
| 5 V 4 A power supply | 12 |
| Peltier TEC1-12706 | 9.80 (pack of 2) |
| IRLB8721 MOSFET | 10 (pack of 10) |
| 40 mm heat sink with fan | 15 (pack of 2) |
| DHT11 | 3.50 |
| **Total** | **≈ 191** |

These costs don't include the reactor box or the aluminium sheet. The CO₂ sensor makes up about half the total.

## Software requirements

- [Arduino IDE](https://www.arduino.cc/en/software)
- **ESP32 board support** (Espressif), installed through *Boards Manager*. The sketch uses `ledcSetup()`/`ledcAttachPin()` because `analogWrite()` isn't available on older ESP32 cores. These functions belong to **ESP32 core 2.x**. In core 3.x they were replaced by `ledcAttach()`, so either install a 2.x version or update those two lines.
- **DHT sensor library** by Adafruit (plus *Adafruit Unified Sensor*), installed through *Library Manager*
- `Stepper`, `WiFi`, `HTTPClient` and `SPI` come with the Arduino/ESP32 core

## Setup

1. Open `Temp_and_CO2_control_and_stepper_motor/Temp_and_CO2_control_and_stepper_motor.ino` in the Arduino IDE.
2. Fill in your own details near the top of the file:
   ```cpp
   const char * ssid = "YOUR_WIFI_NAME";
   const char * password = "YOUR_WIFI_PASSWORD";
   String IFTTT_Key = "YOUR_IFTTT_WEBHOOK_KEY";
   ```
3. Create an IFTTT applet: **If** *Webhooks → Receive a web request* (the event name must match `eventName` in the code), **Then** *Google Sheets → Add row to spreadsheet*. Map `Value1` = humidity, `Value2` = temperature (°C) and `Value3` = CO₂.
4. Select your ESP32 board and port, then upload.
5. Open the Serial Monitor at **9600 baud**. It shows humidity, temperature, CO₂ and the PWM value.

## Adjustable settings

| Setting | Variable | Default |
|---|---|---|
| Temperature setpoint | `set_temperature` | 28 °C |
| PID gains | `kp`, `ki`, `kd` | 90, 60, 80 (best combination found so far) |
| Logging interval | `eventTime1` | 60 000 ms (1 min) |
| Sensor and PID interval | `eventTime2` | 10 000 ms (10 s) |
| Oxygen concentrator on-time | `eventTime3` | 79 200 000 ms (22 h) |
| Oxygen concentrator off-time | `delay(7200000)` | 2 h |
| Stepper speed | `motSpeed` | 15 rpm |

## Current status and limitations

- Temperature control, oxygen concentrator control and data logging work together on one ESP32. No one needs to tend the reactor while it runs.
- The CO₂ sensor still needs to be wired into the ESP32 setup. Until then, CO₂ is logged as 0. The Arduino code that reads the K-30 is not part of this repository.
- The Peltier only heats; it can't cool yet.
- The 2-hour concentrator off-period uses `delay()`, which pauses the whole program. Temperature control and logging stop during those 2 hours.
- The CO₂ value is converted to a whole number before it is sent to Google Sheets.

## Future work

- **Heating and cooling:** use a bidirectional thermoelectric driver (H-bridge) to reverse the Peltier current. It would be controlled over I²C by writing to its two 10-bit DAC registers (DAC0 and DAC1), which set the output voltage and therefore how much the Peltier heats or cools.
- **More Peltier modules:** with a 5 V 3 A supply and about 0.625 A per Peltier (8 Ω), the driver can run up to 4 modules.
- **Wire up the CO₂ sensor** and use the readings to control the nutrient feed.


