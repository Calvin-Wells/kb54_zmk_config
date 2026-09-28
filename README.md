# kb54_zmk_config

ZMK firmware configuration for the Human Computing KB54, a split wireless keyboard with nRF52840 MCUs and Sharp memory LCD displays.

## Repo Layout

- `config/kb54_nrf52840.keymap` — Keymap definition
- `config/boards/arm/kb54_nrf52840/` — Custom board definition and display widgets
- `config/west.yml` — ZMK version pinned to v0.3 (set revision here)
- `.github/workflows/build.yml` — CI build configuration (keep ZMK version in sync with west.yml)
- `build.yaml` — Build targets and optional settings_reset entries

## Building

Push to GitHub. GitHub Actions automatically builds the firmware. Download the `firmware` artifact zip from the completed workflow run — it contains a `.uf2` file for each half (`kb54_nrf52840_l` for left, `kb54_nrf52840_r` for right).

## Flashing

1. Plug a keyboard half into your computer via USB
2. Double-tap the reset button on that half to enter the UF2 bootloader (it mounts as a USB drive)
3. Copy the matching `.uf2` file onto the mounted drive; it reboots automatically
4. Repeat for the other half

Flash both halves when changing board configuration or ZMK version. For keymap-only changes, technically only the left half (central) needs flashing, but flashing both is safer.

## Troubleshooting Pairing and Settings Reset

If pairing fails or you need a clean slate:

1. In `build.yaml`, uncomment the `settings_reset` entries
2. Build and download the firmware artifact
3. Flash the `settings_reset` firmware to **both halves**
4. Flash normal firmware back to **both halves**
5. Remove the keyboard from your computer's Bluetooth device list
6. Re-pair

To clear just the current BLE profile without rebuilding, hold LWR + RSE (Adjust layer) and press `BT_CLR`.

## ZMK Studio

The left half is built with ZMK Studio over USB and BLE. Open https://zmk.studio, connect, then unlock with LWR + RSE + the Studio unlock key.

Keymap changes made in Studio are stored on the keyboard and take precedence over `config/kb54_nrf52840.keymap`. After flashing a changed keymap file, use "Restore Stock Settings" in Studio for it to take effect.

## Changing ZMK Version

Update both:
- `config/west.yml` — set the `revision` field
- `.github/workflows/build.yml` — set the `@ref` field

Keep these in sync. ZMK v0.3 uses Zephyr 3.x; newer versions may use Zephyr 4.1, which the board definition does not yet support.

See https://zmk.dev/docs for general ZMK documentation.
