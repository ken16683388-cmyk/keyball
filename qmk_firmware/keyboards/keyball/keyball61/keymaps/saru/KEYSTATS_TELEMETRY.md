# KeyStats QMK telemetry protocol v1

The `saru` keymap sends aggregate-source events to the Windows KeyStats companion over the
same QMK Raw HID endpoint used by VIA. VIA remains enabled and no custom OUT command is added.

Each IN report is exactly 32 bytes:

| Offset | Size | Meaning |
|---:|---:|---|
| 0 | 3 | ASCII `KST` magic |
| 3 | 1 | Protocol version (`1`) |
| 4 | 1 | Message: `1` key, `2` layer state, `3` heartbeat |
| 5 | 1 | Key flags: pressed, tap, interrupted, dual-role, layer-tap, mod-tap |
| 6 | 2 | Little-endian sequence number |
| 8 | 1 | Source layer after transparent-key resolution |
| 9 | 1 | Highest active layer |
| 10 | 1 | Matrix row |
| 11 | 1 | Matrix column |
| 12 | 2 | QMK keycode |
| 14 | 1 | QMK tap count |
| 15 | 1 | Active/one-shot modifiers |
| 16 | 4 | Firmware uptime in milliseconds |
| 20 | 4 | Effective layer state |
| 24 | 4 | Default layer state |
| 28 | 1 | Capability bits |
| 29 | 1 | Matrix rows |
| 30 | 1 | Matrix columns |
| 31 | 1 | XOR of bytes 0 through 30 |

The firmware emits key press/release events, exact layer transitions, and a heartbeat every five
seconds. The sequence lets KeyStats report lost HID packets. The Windows companion validates the
magic/version/type/checksum and ignores normal VIA responses.

The companion automatically writes message type `0x80` as a keep-alive. Firmware telemetry is
enabled only while those commands arrive and times out after ten seconds. This prevents Raw HID IN
back-pressure from delaying normal typing when KeyStats is not running. No manual mode switch is
needed.

KeyStats does not persist this event stream. It immediately reduces reports into counts by layer,
matrix position, keycode, physical bigram and dual-role outcome. Typed text is not written to disk
or sent over a network.
