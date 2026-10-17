# Camera Calibration

Design for a **Calibrate** button that measures the connected camera's PTZ
characteristics, replacing the values currently hard-coded in
`src/cameraclient.cpp`.

## Motivation

Camera preset transitions move pan and tilt at different speeds so both axes
arrive at the same time and the camera travels in a straight line. Picking those
speeds requires knowing how fast each axis moves at each VISCA speed. VISCA
speeds are far from linear: on the current camera, speed 1 is 25 units/s, speed 5
is 85 units/s, and tilt barely gets faster above speed 12.

These velocities are hard-coded in `PanVelocity` and `TiltVelocity` from
measurements of the current camera. A different camera model will likely have
a different speed curve, so its pan and tilt will no longer finish together.

## What Gets Calibrated

| Value | Currently | Measured by |
|---|---|---|
| Max pan/tilt speed | Hard-coded `MaxPanSpeed = 24`, `MaxTiltSpeed = 20` | Pan-tilt MaxSpeed Inquiry |
| Pan velocity per speed | Hard-coded `PanVelocity` table | Driving pan at each speed and timing it |
| Tilt velocity per speed | Hard-coded `TiltVelocity` table | Driving tilt at each speed and timing it |
| Zoom Direct scale | Learned during normal use (`Camera/ZoomDirectScale`) | One Zoom Direct and readback |

The zoom scale is already learned automatically. It is included so a newly
connected camera is correct from its first preset recall instead of after it.

## User Interface

In **Settings > Camera Presets > Transition Speed (All Presets)**:

- **Calibrate...** button
  - Disabled while the camera is not connected, with the tooltip
    "Camera is not connected", matching **Get Current**.
- Status label next to it
  - "Calibrated 2026-10-17" (in the system's short date format) or "Not calibrated".
- **Reset** button
  - Clears the calibration and goes back to the built-in defaults. The zoom
    scale is kept, since it is relearned during normal use anyway.

Clicking **Calibrate...** shows a confirmation first:

> The camera will move through its pan, tilt, and zoom range for a minute or two.
> Only do this while the camera is not live. It will return to its current
> position when finished.

During calibration, a `QProgressDialog` shows the current step (e.g. "Measuring
pan speed 12 of 24") with a **Cancel** button.

Calibration results are measurements, not preferences, so they are saved as
soon as calibration finishes, independent of **OK**, **Apply**, and **Cancel**
in the settings dialog. The same applies to **Reset**.

## Procedure

1. **Record the start position** with pan/tilt and zoom inquiries, for restoring
   at the end.
2. **Query the max speeds** with Pan-tilt MaxSpeed Inquiry (`81 09 06 11 FF`,
   reply `90 50 ww zz FF`). If the camera doesn't support it, fall back to 24 and
   20, the standard VISCA maximums.
3. **Move to the middle of the range** with Pan-tilt Home (`81 01 06 04 FF`) so
   the speed measurements have room to move in both directions.
4. **Measure pan speeds.** For each speed from 1 to the max pan speed:
   1. Start Pan-tilt Drive (`81 01 06 01 VV WW 0p 03 FF`), alternating left and
      right so the camera stays near the middle.
   2. Poll the pan/tilt position continuously for a fixed time. Use longer times
      at low speeds for enough movement to measure and shorter times at high
      speeds to stay within range (about 1.6 s at speeds 1-4, 1.0 s up to 12,
      0.7 s above that).
   3. Send Pan-tilt Stop and wait for the position to settle.
   4. Velocity = distance / time over the middle half of the samples, which
      leaves out acceleration and deceleration.
   5. Return home before the next speed if the camera has drifted more than
      500 units of pan or 150 of tilt from it. Alternating directions keeps it
      near home, but each move is longer than the last.
5. **Measure tilt speeds** the same way, with shorter times (tilt range is much
   smaller than pan).
6. **Measure the zoom scale.** Send Zoom Direct to a mid-range value (8000),
   wait for it to settle, and divide the reported position by 8000.
7. **Restore the start position** with Pan-tilt Absolute and Zoom Direct (using
   the new zoom scale).
8. **Clean up and save** (see below).

Total time on the current camera is a minute or two.

### Cleaning Up the Measurements

- **Hitting a limit.** If the position stops changing before the drive time is
  up, the axis reached its mechanical limit. Use only the samples from before
  it stopped. If too few remain, retry that speed from home with half the time.
- **Non-increasing values.** Measurements are noisy where the speed curve flattens
  out. The current camera measured tilt 14 at 336 units/s, below tilt 12 at 350.
  Force each value to be at least the one before it.
- **Failure.** If a speed can't be measured after a retry, or position replies
  stop arriving, abort, restore the start position, keep the previous
  calibration, and report the error.

## Storage

Saved in the `Camera` settings group, next to `ZoomDirectScale`:

| Key | Type | Example |
|---|---|---|
| `MaxPanSpeed` | int | `24` |
| `MaxTiltSpeed` | int | `20` |
| `PanVelocity` | list of doubles, index 0 unused | `0, 25, 31, 44, ...` |
| `TiltVelocity` | list of doubles, index 0 unused | `0, 24, 31, 43, ...` |
| `CalibratedAt` | date/time | `2026-10-17T14:02:11` |

Absent keys mean not calibrated: use the built-in defaults, which are the
current hard-coded tables.

Changing the camera host in **Network** settings doesn't clear the calibration,
since the address may change without the camera changing. The status label
lets the user see the calibration is from before the change.

## Implementation

### CameraClient

- **Calibration state machine.** The probe script blocked while waiting for
  replies; the app can't block the event loop. Calibration follows the same
  pattern as `PtzJob`: a `std::optional<CalibrationJob>` advanced by a
  `QTimer` and by position replies as they arrive.
- **Public interface**
  - `void startCalibration()`
  - `void cancelCalibration()`, which stops movement and restores the start
    position
  - Signals: `calibrationProgress(int step, int stepCount, QString description)`
    and `calibrationFinished(bool ok, QString error)`
- **Exclusive with PTZ jobs.** Starting calibration cancels any running PTZ job.
  Recalling a preset during calibration cancels the calibration without
  restoring the start position: during live use, the preset takes priority.
- **Velocity tables.** Replace the `constexpr` `PanVelocity`, `TiltVelocity`,
  `MaxPanSpeed`, and `MaxTiltSpeed` with members loaded from settings on
  construction, falling back to the current values as defaults.
  `speedForVelocity()` already takes a `std::span`, so it works unchanged.
- **New VISCA commands:** Pan-tilt MaxSpeed Inquiry, Pan-tilt Home, and
  Pan-tilt Drive.
- **Reply handling.** `handleViscaReply()` tells replies apart by their size. The
  max speed reply has 2 data bytes, which doesn't clash with power (1), zoom (4),
  or pan/tilt (8 or 9).

### Settings

- `settingsdata`: `CameraCalibration` struct with `get`/`set`/`clear`
  functions following the existing pattern.
- `SettingsDialog`: the buttons and status label above. Like **Get Current**,
  it emits a signal that `MainWindow::openSettings()` connects to `CameraClient`,
  and receives the result and connection state through slots.

## Testing

- Calibrate the current camera and compare the saved tables with the probe
  results in the table below.
- Cancel partway through; the camera should stop and return to its start
  position, and the previous calibration should remain.
- Recall a preset partway through; calibration should stop and the preset
  should run normally.
- Disconnect the camera partway through; calibration should fail with an error
  and keep the previous calibration.
- **Reset**, then confirm transitions use the defaults.

### Reference Measurements

From the probe of the current camera (2026-10-17), in position units per second:

| Speed | 1 | 2 | 3 | 4 | 5 | 6 | 8 | 10 | 12 | 14 | 16 | 18 | 20 | 22 | 24 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Pan | 25 | 31 | 44 | 60 | 85 | 110 | 180 | 268 | 376 | 523 | 584 | 677 | 759 | 848 | 866 |
| Tilt | 24 | 31 | 43 | 60 | 83 | 112 | 177 | 260 | 350 | 336 | 373 | 378 | 372 | | |

Max speed inquiry: pan 24, tilt 20. Zoom Direct scale: 1.0554 from 2000 to 15000;
the camera reports 16384 at full tele, which Zoom Direct reaches at about 15524.
