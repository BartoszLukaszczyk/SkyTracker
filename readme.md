# GIGA Projekt: Portable Alt-Az Sky Tracker

Author: **Bartosz Łukaszczyk**\
License: **MIT License**

---

## About the Project

GIGA Projekt is a portable, motorized Alt-Az tracking platform designed to follow astronomical objects in real time. It uses an ESP32 microcontroller to control two stepper motors (Pan and Tilt axes) based on celestial coordinates downloaded from the NASA JPL Horizons API. The device is calibrated via hardware limit switches and can be controlled using a dedicated Qt-based Android app via Bluetooth.

This project is fully open-source, including hardware, 3D models, and software.

---

## Key Features

- Bluetooth communication between Android and ESP32
- JPL Horizons integration (Sun, Moon, Planets)
- Spherical interpolation for smooth movement every 4 seconds
- Live tracking with correction support
- Manual control mode
- Homing procedure using limit switches

---

## Mechanical Design

- Pan axis shaft is made of metal (printed one was too fragile)
- Gear ratio Pan: **14/180**, Tilt: **14/84**
- Tilt cradle mounted on smooth shafts and ball bearings
- M3 and M4 heat-set threaded inserts used throughout
- STL and F3D files are available in the `models/` directory for easy customization
- All required parts are listed in `docs/BOM.pdf`

### Mechanical Advice

- Insert a piece of metal sheet under the Pan stepper motor mounting screws to prevent deformation of the printed base plate

---

## Electronics

- ESP32 DevKit, communicating over BLE
- MKS DLC V2 + A4988 drivers
- IMU (MPU-6050) and compass (QMC5883L)
- Limit switches for both axes
- 12V 8A power supply

Optional modules (GPS and fan with thermistor) were initially planned but removed during development:

The GPS module caused reliability issues and was replaced by the smartphone's internal GPS.

The fan, controlled by a thermistor, was counterproductive — it was the main source of heat buildup, especially affecting the buck converter. Removing it simplified the system and improved thermal stability.

Block diagram and wiring diagrams are included in `docs/`.

---

## Software Overview

### Android App

- Written in C++ using **Qt Creator 6.9.1**, built with **qmake**
- Requires pairing with ESP32 via Android system Bluetooth settings
- Device must be selected from **paired devices** inside the app
- ⚠️ “Connected” message may appear even if pairing fails – verify manually


### ESP32 Firmware
- Written in C++ using **Arduino IDE**
- Receives commands from Android app over Bluetooth serial

---

### 🎮 Manual Control

- Single tap = 5 motor steps
- Hold = continuous smooth motion

---

### 🧭 Homing / Calibration

1. Move both axes toward endstops
2. Back off 100 steps
3. Read orientation from IMU + compass
4. Set virtual position to **180° Azimuth / 45° Elevation**

This is the reference position for tracking.

---

### 🌌 Object Tracking

1. User selects object from list
2. App gets phone's GPS location
3. Ephemerides fetched via **JPL Horizons API**
4. 1-hour dataset downloaded (1-minute resolution)
5. Az/El positions calculated, corrected for Earth's rotation
6. **Spherical interpolation** every 4 seconds
7. Motor steps computed based on current gear ratios
8. Motion starts between minute 1–2 after object selection

- Manual adjustments are allowed during tracking
- Phone must remain connected via Bluetooth
- Power-saving settings may require app to stay foregrounded

---

## Project Structure

```
gigaprojekt/
├── src/
│   ├── Qt/          # Qt Android application
│   └── ESP/         # ESP32 firmware
├── models/          # STL, F3D files for 3D printing
├── docs/            # BOM, schematics, diagrams
├── images/          # Device photos
├── README.md
├── LICENSE          # MIT License
└── .gitignore
```

---

## How to Build

### Qt App (Android)

1. Open `src/Qt/gigaprojekt.pro` in Qt Creator
2. Configure Android kit with qmake
3. Build & deploy to paired Android phone
4. Pair the ESP32 from Android system settings

### ESP32 Firmware

1. Open `src/ESP/main.ino` in Arduino IDE
2. Select ESP32 DevKit board
3. Install required libraries (`Wire`, `MPU6050`, etc.)
4. Upload firmware

---

## Contact

For questions or contributions, feel free to open an issue or pull request.

