# USC Controller Wrapper Status Report

## Handoff summary

This is a separate sibling project for USC (Unnamed SDVX Clone), located at:

```text
V:\Code\USC Controller Wrapper\
```

The goal is to read two physical controller sticks, convert each stick vector to an angle with `atan2(Y, X)`, and expose the two angles as virtual controller axes that USC can bind as its two knobs.

## Current project state

The following files currently exist:

- `USC Controller Wrapper.sln` — Visual Studio solution with `Debug|x64` and `Release|x64` configurations.
- `USC Controller Wrapper.vcxproj` — C++20 x64 console project.
- `main.cpp` — XInput-to-ViGEm prototype.
- `README.md` — Setup, mapping, binding, limitation, and future-work documentation.
- `.gitignore` — Basic Visual Studio/build-output exclusions.

The folder is not currently initialized as a Git repository. No ViGEmClient submodule is currently present.

## Implemented prototype behavior

`main.cpp` currently:

1. Finds the first available XInput controller, or falls back to the first attached DirectInput game controller.
2. Reads the left and right stick vectors. The DirectInput fallback assumes the common DualShock 4 `lX/lY` and `lZ/lRz` layout.
3. Applies a `0.15` deadzone.
4. Computes each stick angle with `atan2(Y, X)`.
5. Maps the angle from `[-pi, pi]` to the normal signed XInput axis range.
6. Creates a virtual Xbox 360 controller through ViGEmClient.
7. Sends the left-stick angle to virtual `sThumbLX` and the right-stick angle to virtual `sThumbRX`.
8. Mirrors the physical buttons and triggers into the virtual report.

This is an absolute-angle mapping. It is not yet an accumulated or relative rotary-encoder-style knob implementation.

## Build status

The project was attempted with MSBuild, but compilation stopped because the ViGEmClient SDK header was not available:

```text
error C1083: Cannot open include file: 'ViGEm/Client.h'
```

`VIGEMCLIENT_SDK` is not set in the current environment. The DS4Windows/ViGEmBus driver installation does not automatically provide the C/C++ SDK headers and library required to compile this project.

The project currently expects this SDK layout:

```text
$(VIGEMCLIENT_SDK)\include\ViGEm\Client.h
$(VIGEMCLIENT_SDK)\lib\x64\ViGEmClient.lib
```

The current `.vcxproj` also needs the normal linker dependency added if it is not already inherited:

```text
ViGEmClient.lib
```

## ViGEmClient repository investigation

The repository referenced by the upstream documentation is:

```text
https://github.com/nefarius/ViGEmClient
```

It is archived/read-only. It can be added as a submodule or subtree, but the repository source itself is not necessarily a ready-to-use prebuilt SDK folder matching the current `VIGEMCLIENT_SDK` paths. The next agent should inspect its solution/build instructions and choose one of these approaches:

1. Build ViGEmClient separately and point `VIGEMCLIENT_SDK` at the resulting include/lib layout.
2. Add the repository as `external/ViGEmClient` and integrate its source/project into this solution.
3. Use the repository's recommended vcpkg integration if compatible with the installed Visual Studio toolset.

Do not assume that installing ViGEmBus alone fixes the C++ build.

## Recommended next steps

1. Open the correct solution from `V:\Code\USC Controller Wrapper\`, not the Sentakki solution.
2. Decide whether to initialize this folder as a Git repository before adding the ViGEmClient submodule.
3. Add or build ViGEmClient and make `Client.h` available.
4. Add/link `ViGEmClient.lib` for x64 Debug and Release.
5. Rebuild the USC project.
6. Run the wrapper with DS4Windows exposing the physical controller through XInput.
7. Confirm that a virtual Xbox 360 controller appears and that USC can bind virtual `Lx` and `Rx` as the two knobs.
8. Test the negative-X angle boundary; the current absolute mapping can jump at the `-pi/pi` wraparound.
9. Decide whether USC needs absolute angle output or a relative/unwrapped rotation model.

## Known limitations and design decisions

### Software conditions

- XInput is preferred; DirectInput fallback is implemented for the common DualShock 4 axis layout.
- The first available XInput controller is selected automatically.
- The first attached DirectInput game controller is selected automatically when XInput is unavailable.
- DirectInput button and trigger mirroring is not implemented yet.
- DirectInput enumeration, controller selection, calibration, inversion, and configurable sensitivity are not implemented.
- The stick center has no meaningful angle, so values inside the deadzone output zero.
- The bounded axis mapping wraps at the negative-X direction.
- Reconnect handling and diagnostic status output are not implemented.

### Hardware/environment conditions

- DS4Windows must expose the desired physical controller through XInput.
- ViGEmBus must be installed and working for the virtual controller to appear.
- The ViGEmClient development SDK must be available separately for compilation.
- The correct Visual Studio solution and working directory must be used; the USC project is not part of the Sentakki repository.

### Software problems currently blocking progress

- The project cannot compile until `ViGEm/Client.h` and the matching ViGEmClient library are supplied.
- The SDK path and library linkage have not yet been validated on this machine.

### Planned software improvements

- Add an explicit controller-selection menu.
- Add DirectInput controller selection, configurable axis assignments, and button/trigger mirroring.
- Add calibration and configurable deadzone/inversion/sensitivity.
- Add absolute-angle and relative/unwrapped knob modes.
- Add controller reconnect handling and visible axis diagnostics.

## Handoff note

The README is suitable as the user-facing setup guide. This file is the working status report for the next agent: the project skeleton and mapping prototype exist, but ViGEmClient integration and the first successful build remain unfinished.
