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
- Optional modules (GPS, fan + thermistor) were initially planned but removed due to complexity and noise. ESP32 provides sufficient time sync via Bluetooth, and the fan introduced mechanical vibrations.

Block diagram and wiring diagrams are included in `docs/`.

---

## Software Overview

### Qt App (Android)

- Written in C++ using **Qt Creator 6.9.1** and built using `qmake`
- UI with real-time manual control (5-step click or smooth hold)
- Device must be first paired in Android BT settings

### ESP32 Firmware

- Written in C++ using Arduino IDE
- Receives commands like `UP`, `DOWN`, `STEP`, `PREP`, `FRIGHT`, etc.
- Homing routine: moves to limit switches, backs off 100 steps, then sets position to 180S/45El

### Tracking Logic

- User selects object (Sun, Moon, planets)
- App retrieves phone GPS location and calls JPL Horizons API
- Ephemerides downloaded for 1 hour ahead, 1-minute resolution
- Data converted RA/DEC → Alt/Az (with GMST correction)
- Spherical interpolation at 4-second intervals
- Result: sequence of `deltaPan`, `deltaTilt` every 4 seconds
- Tracking starts \~1 minute after target selection
- App must stay connected and in foreground (depending on Android battery settings)

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

