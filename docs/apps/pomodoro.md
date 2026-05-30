# Pomodoro

Status: `ready`

Focus timer using the classic Pomodoro cadence, with a time-of-day clock and a
cyclable accent color.

## Intervals

- Focus block (default 25 min, adjustable 5–60 in 5-min steps) → 5:00 short break.
- A 15:00 long break after every 4th focus block.
- While idle, use the `−` / `+` buttons under the timer to set the focus length;
  the choice persists across launches.
- At the end of each interval the screen flashes (and beeps, once the audio path
  is verified) and **waits** for you to tap Start before the next interval — no
  surprise auto-start.

## Controls

- Tap **Start / Pause / Resume** — run or pause the current interval.
- Tap **Reset** — back to Focus 1, idle.
- **Short BOOT press** — cycle the accent theme color (cyan → green → amber →
  magenta → red → white). The choice persists across launches.
- **Long BOOT press** — return to launcher.

## Display

Big `MM:SS` countdown, the current phase, progress dots for the 4-focus cycle,
and the battery + time-of-day chrome in the header (RTC).

## Hardware

Uses `WaveshareAmoledSensors` (battery + RTC) and persists the theme color in a
`pomodoro` NVS namespace. The end-of-interval beep uses the ES8311 speaker path,
which is shared with the rest of the system.
