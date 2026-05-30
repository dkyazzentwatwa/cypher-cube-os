# Tricorder

Status: `ready`

One-screen dashboard of every onboard sensor.

## Controls

- Tap: trigger a fresh WiFi scan.
- Long press BOOT: return to launcher.

## Readouts

- Battery: percent, voltage, charge state (AXP2101).
- Time: date and time (PCF85063 RTC).
- IMU: accelerometer (g), gyroscope (deg/s), and orientation (QMI8658).
- WiFi: strongest nearby AP SSID and RSSI.

## Hardware

Uses `WaveshareAmoledSensors` (battery + RTC + IMU) and the ESP32-S3 WiFi radio.
No SD assets required.
