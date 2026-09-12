[日本語](README.md) | [English](README.en.md)

# M5Stack StopWatch Utilities

The C152 fetches quotes directly over Wi-Fi. At home it uses the saved home network; outside it can use the iPhone's Personal Hotspot. No iOS app, Expo Go, BLE relay or development signing is needed. Current firmware is in `firmware/`.

Hold blue to configure up to two networks. Join the device's `StopWatch-FX` network with its displayed password, then open `http://192.168.4.1`. Save the home SSID/password and iPhone name/hotspot password. At least one network is required. Blank password retains the password for an unchanged SSID; blank SSID removes that slot. Setup expires after two minutes.

On the iPhone enable Allow Others to Join and Maximize Compatibility, and keep the Personal Hotspot settings screen open until connected. The device cannot remotely enable the hotspot through Apple's Instant Hotspot mechanism.

Home is attempted first (8 seconds), then iPhone (8 seconds). Failed connection attempts stop Wi-Fi and retry after 60 seconds while awake. Normal updates are every 5 seconds, every 15 seconds at <=20% battery, manual at <=10%. On battery, dim after 15 seconds and sleep the display/stop networking after 30 seconds without input. Active network calls finish within their timeouts before Wi-Fi turns off. Charging keeps the screen on with 5-second updates. Battery lifetime is not measured.

Hold yellow anywhere to open the five-app launcher: USD/JPY, Aquarium, Battery, Wi-Fi Settings, Pomodoro. Click yellow to move down, blue to launch, or tap a row. In USD/JPY, short clicks/taps request updates; blue hold or touch hold opens setup. All other screens stop normal quote traffic. Last prices and source timestamps remain visible when stale or unavailable.

Build with `rtk proxy pio run -d firmware`; see README.md for tests, upload commands and official sources. Hardware compilation and host tests are verified; actual hotspot connectivity and battery lifetime require physical testing after Wi-Fi setup.

On 2026-09-12, v1.1.1 passed the board build, existing host tests, USB upload and flash hash verification. Device serial diagnostics confirmed boot, home Wi-Fi, HTTPS HTTP 200/API status 5, and restoration to 80MHz. Live quotes remain unverified during API maintenance. Visual display, iPhone hotspot and battery lifetime remain unverified. These are historical v1.1.1 results. Open Battery from the launcher to see the installed version.

## v1.1.1 connection fix

TLS key exchange at 80MHz could starve CPU 0’s idle task and trigger watchdog resets after Wi-Fi connected. HTTPS calls now temporarily use 240MHz and restore the previous clock after client cleanup. Certificate verification and the watchdog remain enabled. API status 5 is displayed as `API MAINTENANCE`; failures retry every 30 seconds while awake. USB diagnostics include HTTP/API status and CPU MHz.

Hardware regression check (requires pyserial; opening USB may reset the device): `rtk proxy python firmware/test/serial_connection.py <port> 45`. It requires at least three connected, quote-ready samples and fails on crashes/repeated boots. During API maintenance, append `--allow-maintenance` to verify HTTP 200/API status 5 and restoration to 80MHz instead; this does not validate live quotes.

## Integrated apps

The aquarium includes nine fish, feeding, tap ripples, IMU motion, bubbles and the four-minute light cycle from [Medaka](https://github.com/KeijiTokunaga/m5stack-stopwatch-medaka), commit `887c6ff430a97aa6093403a9541e5973990d3fbd`. Yellow/blue clicks feed left/right; touch hold freezes/resumes lighting. After 60 seconds without activity it dims and renders at up to 5 fps without turning the screen off. The simulation pauses while another app is displayed and resumes on return.

[Pomodoro](https://github.com/KeijiTokunaga/m5stack-pomodoro), commit `eed0a0682518116887c4608da266fc40e915b7ce`, retains 25-minute focus, 5-minute breaks and a 15-minute break after four completed focus sessions. Yellow click or the central box starts/pauses; blue click or SOUND toggles audio; blue hold resets the current phase. Tap NEXT to skip without earning a completed session. Yellow hold now opens the launcher instead of skipping.

The timer continues in other apps and with the display asleep. A phase ends with three seconds of vibration and optional sound; the next phase waits paused. State is saved on actions/transitions and every 60 seconds while running. Reboot restores the saved state paused; powered-off time is not counted and up to about 60 seconds of progress can be lost.

Wi-Fi setup can be exited with yellow hold; its access point is stopped. With no saved network, setup opens at boot but offline apps remain accessible. Aquarium state and app selection are not persisted. IMU and speaker are initialized for the integrated apps; microphone and BLE remain unused.

v1.2.0: board build and host tests passed (launcher, quotes/energy, ten-minute aquarium stress, timer lifecycle). On 2026-09-13, USB upload and hash verification succeeded. Serial diagnostics confirmed v1.2.0 boot, home Wi-Fi, HTTP 200/API status 0 and live quotes; the 45-second check passed with 11 quote-ready samples and no crash. Integrated display/touch/buttons/IMU/sound/vibration remain unverified. See README.md for test commands.
