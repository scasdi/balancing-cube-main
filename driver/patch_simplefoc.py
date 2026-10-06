"""Patch SimpleFOC for the Arduino STM32 core's new enableTimerClock signature.

The core changed:

    void enableTimerClock(TIM_HandleTypeDef *htim)   ->   (TIM_TypeDef *instance)

SimpleFOC still passes the handle, so it will not compile. TIM_HandleTypeDef
carries the TIM_TypeDef* the function now wants in its Instance member, so the
call just needs that member instead.

Run as a pre-build script so the fix survives a clean build or a re-fetch of the
library - patching .pio/libdeps by hand does not. Idempotent: it looks for the
broken call and leaves an already-patched file alone.

Delete this script and the extra_scripts line once a SimpleFOC release builds
against the current core on its own.
"""

import os

Import("env")                                      # noqa: F821 - PlatformIO injects this

BROKEN = "enableTimerClock(handle);"
FIXED  = "enableTimerClock(handle->Instance);"

TARGET = os.path.join(
    env.subst("$PROJECT_LIBDEPS_DIR"), env.subst("$PIOENV"),  # noqa: F821
)


def patch():
    if not os.path.isdir(TARGET):
        print("patch_simplefoc: libdeps not present yet - will patch on the next build")
        return

    for root, _dirs, files in os.walk(TARGET):
        if "stm32_mcu.cpp" not in files:
            continue
        path = os.path.join(root, "stm32_mcu.cpp")
        with open(path, "r", encoding="utf-8", errors="ignore") as f:
            text = f.read()

        if BROKEN not in text:
            print(f"patch_simplefoc: {path} needs no patch")
            continue

        with open(path, "w", encoding="utf-8") as f:
            f.write(text.replace(BROKEN, FIXED))
        print(f"patch_simplefoc: patched {path}")


patch()
