# Keyball61 saru keymap

Personal Keyball61 firmware based on the upstream `via` keymap.

## Compatibility

- Keeps `VIA_ENABLE = yes`, so VIA/Remap-compatible dynamic keymap commands and Keyball61Tool remain available.
- Uses QMK `0.22.14`, matching the version pinned by the repository workflows.
- Uses the same matrix layout and default keymap as the upstream `via` keymap.

## Personal baseline settings

- Enables the split watchdog for recovery when the two halves fail to establish communication.
- Waits for USB enumeration at startup.
- Adds a 500 ms wake-up delay after USB suspend.

Feature experiments should be added to this keymap instead of changing the shared Keyball61 keyboard configuration or the upstream-compatible `via` keymap.

## Build

```sh
qmk compile -kb keyball/keyball61 -km saru
```

The GitHub Actions workflow **Build a firmware on demand** defaults to `keyball61` and `saru`.
