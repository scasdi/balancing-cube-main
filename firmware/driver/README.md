# Motor driver firmware

Sources for the **B-G431B-ESC1 (STM32G431)**, not for the ESP32.

They live outside `src/` because PlatformIO builds everything under `src/`,
and each of these files defines its own `setup()` / `loop()`, which collides
with `src/main.cpp`. Two of them also use STM32 pin names (`PA8`, `PC10`)
that do not exist on ESP32.

Nothing here is compiled by the `esp32dev` environment. Building them needs a
separate PlatformIO environment targeting the G431, which is not set up yet.
