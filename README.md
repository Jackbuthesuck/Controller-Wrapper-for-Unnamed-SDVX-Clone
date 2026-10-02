# USC Controller Wrapper

A separate wrapper for USC (Unnamed SDVX Clone). It reads one physical XInput controller and exposes two virtual Xbox 360 stick axes through ViGEm:

- Physical left stick angle -> virtual left-stick X (`Lx`) -> first knob.
- Physical right stick angle -> virtual right-stick X (`Rx`) -> second knob.

The physical stick coordinates are converted with:

```text
theta = atan2(Y, X)
axis = theta / pi
```

The output range is `[-32768, 32767]`, which is the normal XInput axis range. A configurable-style deadzone is currently set to `0.15` in `main.cpp`.

## Requirements

- Windows 10 or newer.
- Visual Studio C++ Desktop Development.
- DS4Windows configured to expose the physical controller through XInput.
- ViGEmBus installed. DS4Windows commonly installs this driver.
- ViGEmClient SDK headers and import library for compiling this project.

The ViGEmBus driver alone is not necessarily the same thing as the C/C++ ViGEmClient SDK. Set the `VIGEMCLIENT_SDK` environment variable to the SDK root so the project can find:

```text
$(VIGEMCLIENT_SDK)\include\ViGEm\Client.h
$(VIGEMCLIENT_SDK)\lib\x64\ViGEmClient.lib
```

## Build

1. Open `USC Controller Wrapper.sln`.
2. Select `Debug|x64` or `Release|x64`.
3. Build the project.
4. Ensure DS4Windows exposes the desired controller through XInput.
5. Run the wrapper before launching USC.

The program prefers the first available XInput controller. If no XInput controller is available, it falls back to the first attached DirectInput game controller, using the common DualShock 4 layout. It creates one virtual Xbox 360 controller and continuously updates the virtual left and right stick X axes.

## USC binding

1. Start DS4Windows and confirm the physical controller is available through XInput.
2. Start `USC Controller Wrapper.exe`.
3. Start USC's controller binding screen.
4. Bind the first knob by moving the physical left stick around its circle.
5. Bind the second knob by moving the physical right stick around its circle.
6. Select the virtual controller axes reported by USC, normally `Lx` and `Rx`.

The physical controller itself should not be selected as USC's input device if the wrapper is intended to be the only source. Bind USC to the virtual Xbox controller created by ViGEm.

## Current limitations

- The implementation currently supports XInput input first; DirectInput enumeration and controller selection are not yet included.
- It selects the first connected XInput controller, or the first attached DirectInput game controller when XInput is unavailable.
- DirectInput currently assumes the DualShock 4 layout: left stick `lX/lY` and right stick `lZ/lRz`. Other DirectInput drivers may expose different axis assignments.
- DirectInput button and trigger states are not currently mirrored into the virtual controller.
- The angular mapping has a wraparound at the `-pi/pi` boundary. Moving across the negative-X direction can jump from one axis extreme to the other because a bounded joystick axis cannot represent unlimited turns.
- The center deadzone outputs zero because the stick angle is undefined near the origin.
- The output is an absolute angle mapping, not an accumulated relative knob rotation.
- ViGEmClient SDK discovery is configured through `VIGEMCLIENT_SDK` and may need to be adjusted for the installed SDK layout.

## Future improvements

- Add DirectInput controller selection and configurable axis assignments.
- Add configurable deadzone, inversion, axis assignment, and sensitivity.
- Add angle unwrapping or relative rotation mode if USC's knob behavior requires continuous turning across the wrap boundary.
- Add a calibration screen for center, stick range, and direction.
- Add a way to select the target virtual controller type.
- Add status output showing physical and virtual axis values.
- Add graceful controller reconnect handling.
