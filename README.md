# home-gadgets

Small ESP32 gadgets for home, built from off-the-shelf modules with Dupont
wires only (no soldering). Text on screens is Traditional Chinese.

| Project | Status | What it does |
|---|---|---|
| [`seat-timer/`](seat-timer/) | In daily use | mmWave radar notices when you sit down; after 30 minutes it blinks red and beeps until you stand up. E-paper shows date and weather. |
| `pill-reminder/` | Planned | Medication box that checks whether the morning / evening dose was taken, plus pet medicine and vaccine reminders. |

## Layout

Each project is a self-contained PlatformIO project in its own folder:

```
<project>/
  platformio.ini   build envs (one per hardware variant)
  src/             firmware; src/secrets.h is local only (see secrets.example.h)
  test/            host-side tests (plain g++)
  tools/           helper firmware and scripts
  docs/            wiring diagram, screen previews
docs/              notes shared across projects
```

Build a project from its folder, e.g. `cd seat-timer && pio run -e color -t upload`.

## Secrets

`src/secrets.h` holds the Wi-Fi password and your weather location. It is in
`.gitignore`; copy `src/secrets.example.h` to create it.
