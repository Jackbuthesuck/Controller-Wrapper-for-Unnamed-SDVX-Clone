# USC Laser Input Contract

The wrapper emulates the input style used by commercial SDVX controllers.

## Virtual controller axes

| Function | Virtual axis | USC controller axis |
|---|---:|---:|
| Left laser | `Lx` | `0` |
| Right laser | `Rx` | `2` |

XInput reports signed 16-bit values:

```text
-32768 ... 0 ... 32767
```

USC/SDL reads those axes as approximately `-1.0 ... +1.0`.

## USC mode

Leave Direct Mode disabled:

```ini
Controller_DirectMode = false
```

In normal controller mode USC treats the axis as a wrapped absolute position:

```text
movement = current_axis - previous_axis
```

USC corrects the `+1/-1` boundary when the axis wraps, which is how commercial encoder controllers can rotate indefinitely. Direct Mode is for devices that already report movement deltas; the wrapper does not use it.

## Wrapper conversion

Commercial controllers use incremental rotary encoders. The wrapper approximates one with the physical stick's polar angle:

```text
theta = atan2(y, x)
```

The first valid stick position becomes the reference. Subsequent positions are converted to a wrapped phase:

```text
phase = -(theta - reference_theta)
phase wrapped to [-pi, +pi]
axis_value = phase / pi
```

The negative sign makes clockwise physical movement positive for USC calibration. The phase is converted to the signed XInput range and held as an absolute axis value. USC calculates movement between reports.

In the inner region, the stick's horizontal displacement acts as a spring-centered rotation-speed control:

```text
stick X = 0       -> no rotation
stick X > 0       -> clockwise rotation
stick X < 0       -> counter-clockwise rotation
```

The inner-region speed is currently two complete rotations per second at full horizontal deflection. Entering the outer ring re-anchors the polar reference to preserve the current phase; leaving it returns to X-axis speed control.

## Center deadzone

Stick angle is undefined near the center, so the wrapper uses hysteresis:

```text
Start tracking: magnitude >= 0.35
Stop tracking:  magnitude <  0.27
```

Inside the deadzone, the last valid absolute axis value is held. This prevents center noise from becoming fake knob movement.

## Calibration expectations

USC's controller calibration accumulates raw controller movement and computes:

```text
sensitivity = 6 / accumulated_movement
```

The displayed value decreases while rotating because USC continuously recomputes that expression. Rotate exactly one revolution and press Start to accept the value.

The wrapper's axis phase covers `2*pi` over a complete revolution, so one revolution corresponds to roughly `2.0` normalized axis units:

```text
2*pi / pi = 2
```

A result around `3.0` is a reasonable starting point.

## Diagnostic output

The wrapper prints every 250 ms:

```text
[diag] XInput L raw=(x,y) mag=... angle=... -> Lx=... |
       R raw=(x,y) mag=... angle=... -> Rx=...
```

- `raw`: physical signed stick values.
- `mag`: normalized distance from stick center.
- `angle`: physical `atan2` angle in radians.
- `Lx`/`Rx`: absolute wrapped values sent to the virtual Xbox controller.

If `mag` is below `0.35`, the output is intentionally held rather than recalculated.
