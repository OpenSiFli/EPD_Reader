# SF32LB527 / E0470A03-AF-S

Board target: `sf32lb527-oed-e0470a03_hcpu`. Native panel 1216×684; reader UI uses the existing inverted-portrait rotation, 684×1216. NOR is 16MiB, PSRAM is 8MiB.

This BSP is separate from `sf32-oed-epd_v12`: the actual board has SY7636, AW9523B, FT5x46 touch and LM3630A frontlight. The pinout and timing were verified with a standalone diagnostic firmware before integration.

| Peripheral | Wiring |
|---|---|
| EPD CLK/SPH/LE/OE | PA4/PA6/PA9/PA5 |
| EPD STV/CPV/GMODE | PA0/PA1/PA2 |
| EPD D0..7 | PA7, PA8, PA37, PA39, PA40, PA41, PA42, PA43 |
| AW9523B | I2C2 PA20/PA30, address 0x59, reset PA3 |
| SY7636 | I2C1 PA10/PA11, address 0x62; EN GE6, VCOM_EN GE4, VDD33 GE14 |
| FT5x46 | I2C1 0x38; reset GE13, IRQ PA44, PA32 control |
| LM3630A | I2C2 0x36; GE3 HWEN, GE11 companion enable; A=cold, B=warm |
| TF | SPI1 MOSI PA24, MISO PA25, CLK PA28, CS PA29; GE0 active-low detect |

## Build

```powershell
git submodule update --init --recursive
.\build-sf32lb527.ps1
python tools/package_527.py --output firmware-527
# First installation only, with a full backup and debug access:
python tools/package_527.py --factory --output factory-527
```

The SDK submodule remains at the repository's pinned revision. A board-specific project profile overrides the reference-board pin/peripheral assumptions. Existing board profiles remain available.

The build produces separate main code and built-in font images under `main.bin/`, the independent USB recovery loader, bootloader, Flash table and E0470A03 waveform. Normal USB updates contain only application/font/waveform. See [USB recovery and protection boundaries](DFU_SF32LB527.md) before the first installation.

| Image | NOR address | Partition capacity |
|---|---|---|
| `ftab.bin` | `0x12000000` | 32KiB |
| `bootloader.bin` | `0x12010000` | 64KiB |
| `recovery.bin` | `0x12020000` | 1536KiB |
| `ER_IROM1.bin` | `0x12200000` | 2MiB |
| `ER_IROM2.bin` | `0x12400000` | 10MiB |
| `epd_waveform.bin` | `0x12EC0000` | 256KiB |
| `update_state.bin` (factory only) | `0x12F00000` | 8KiB |

Before first installation, read and verify the complete 16MiB NOR, not only the application partition. Keep that backup private and separate from the public source and flash bundle. The TF card is not formatted by this BSP. This image layout differs from the original application and standalone diagnostic firmware.

`package_527.py` validates board, vectors, sizes and hashes. Its default `usb_update/` output contains only the three permitted update images and the USB tool. Explicit `--factory` additionally packages boot/recovery, initial state, `sftool_param.json` and `ImgBurnList.ini`; CI places this separately under `factory_install/`. Packaging never accesses the serial port or flashes the board.

The [Windows flash GUI](tools/flash_gui/README.md) supports both sftool serial factory installation and protected native USB updates. It imports ZIP, JSON and INI packages; USB mode selects only application/font/waveform and requires device-side readback verification before requesting reboot. USB transport reuses the CLI wire protocol in `tools/usb_dfu_527.py`.

## Driver integration

- EPD uses the tested 48MHz DBI transport and gate pipeline, about 7.76ms per physical scan. Every waveform phase scans the whole panel continuously. The 3MiB cache first indexes distinct four-pixel old/new combinations and computes each combination's waveform once. A full-frame index costs 415,872 bytes; a captured dense-text pair uses about 1.15MB including all 44 phases. Hot lookup tables use 80KiB of internal SRAM. Each full phase is expanded before scanning, with no visible row-group passes. See [waveform preparation measurements](VALIDATION_WAVEPREP_527.md).
- If a transition exceeds 32,767 combinations, a bounded row dictionary remains available as fallback: literal/repeated spans encode antialiased text, and equivalent rows/phases share storage. It can temporarily borrow up to 2MiB from the 3MiB EPUB/FreeType heap in 64KiB steps, leaving at least 512KiB available, and releases it after refreshing. If even that cannot fit, the driver reports an error before enabling high voltage and retains the old display state.
- Ordinary gray updates (including antialiased text) use GL16; strictly black/white transitions can use DU. GC16 cleaning follows the UI's 5/10/20/every-update setting, with 10 as default. INIT is only used at startup or after loss of panel state. Identical framebuffers are skipped. The legacy parser's intermediate white-page flush and transient loading-hourglass updates are suppressed; a page turn submits the completed page once.
- After 500ms of UI inactivity, a fully laid-out chapter can preview the next text page into a temporary 415,872-byte PSRAM framebuffer, then prepare its waveform with HV off. The UI thread owns real rendering and preview; input cancels preparation between lines/rows. The temporary framebuffer is freed afterwards. The next real framebuffer must match every pixel and the cleaning interval before reusing the waveform. Previous-page, menu/font changes, cancellation and allocation failure fall back to ordinary rendering. Images and chapter boundaries currently use the normal path.
- VCOM defaults to the board-tested -1.10V and temperature to 28°C; these are not per-panel factory calibration. SY7636 shutdown retains the 150ms rail sequence and at least 450ms with EN low before re-enabling. The latter is a deadline checked on the next enable, allowing touch handling and preparation of the next page during discharge.
- Touch exposes the reader's `touch` device interface with validated packet decoding, IRQ-driven reads and primary-contact tracking. Raw portrait coordinates match the 684×1216 UI after the existing rotation mapping. Touching a UI control is dispatched once on release.
- Both settings and the reading overlay retain touch operation on this board. The overlay's left/right arrows change the cleaning interval; it does not offer a touch-off setting without a verified physical way to turn touch back on.
- LM3630A retains the tested low-level current/boost configuration and per-bank brightness cap. Only the initial ≤5% ramp can classify a one-shot OCP/OVP event as startup history; two follow-up reads must be clear. Runtime faults still shut down both enables.
- TF is mounted at `/`, matching this repository's `/fonts` and `/settings.cfg` paths. Application writes are enabled for settings and reader data; there is no automatic format. The SPI MSD source snapshot has read CRC16, bounds and transfer-length checks.
- R39 powers down the shared GE14 rail after storage/display activity becomes idle. The filesystem and font/book descriptors remain in RAM; the block driver reinitializes and checks card identity before the next media access. Touch IRQ and LDO3 stay enabled. See [TF power management](TF_POWER_527.md) for locking, recovery, validation and shared-rail limits.
- Font/size/weight/spacing/margin settings are saved to `/settings.cfg`. Short writes, synchronization and close failures are checked; a failed save leaves the settings page open with an error. Saving is not a power-loss-atomic transaction.
- Function Settings includes automatic page turning: Off / 10s / 15s / 20s. The choice is saved with reading settings; only plain reading counts down, while menus/settings pause it. Manual actions restart the interval and the last page stops automatic advancement. See [automatic page turning](AUTO_TURN_527.md).
- Function Settings also includes clock standby, used by the existing inactivity timeout. The default retained-memory Deep Sleep mode powers down known external peripherals while keeping SRAM/PSRAM, so RTC/PA34 can resume quickly; ten DU black/white updates are followed by a GC16 clean. A Hibernate diagnostic mode remains available. The battery-side five-minute averages including one update per minute were 0.82mA retained versus 1.3mA Hibernate; see [clock standby](STANDBY_CLOCK_527.md) for static current and recovery limits.
- The library uses `/books` when that directory exists, otherwise `/`. Home, settings and cold/warm frontlight controls use the 684×1216 portrait dimensions. Touch hit regions account for renderer margins.
- Display refresh serializes touch, frontlight checks and TF hardware transfers through the shared IO lock, without holding filesystem locks. Hardware protection remains enabled. Input during a full refresh can be delayed or coalesced.

The new board automatically enters retained deep sleep while waiting for input after background work finishes. Touch IRQ PA44 is configured as AON PIN20; touch and frontlight supply rails remain stable. The CPU and memory sleep/resume path has been exercised on hardware; physical touch-wake acceptance and whole-board current measurements remain pending. Battery/charger routing and physical keys have not been validated. The BSP does not reuse old-board ADC/key assignments. Bluetooth/PAN and weather code are retained but require separate end-to-end validation with a phone/network.

The recovery boot path uses PA33 as an active-low GPIO input with pull-up and a 1.5-second hold, independently of the PA34 hardware reset/power key. Boot does not enable the application ADC button driver. Existing devices need a separately verified first-stage boot update over debug UART for this change to take effect; normal USB application updates cannot replace the protected bootloader. USB recovery is also available from Settings, and is selected automatically after interrupted updates or failed image checks.

## Validation status

See [hardware and build validation](VALIDATION_SF32LB527.md) for measured results and limits.

See [font switching and idle sleep](VALIDATION_FONT_POWER_527.md) for bounded PSRAM font caching, retained-chapter reflow, measured switch times, sleep statistics and UART diagnostic limits.

For battery-current diagnosis, [R32 minimum-power mode](MINIMUM_POWER_527.md) explicitly closes known external supplies and enters Hibernate without RAM retention. This manual test disables touch wake and exits by a full power cycle; ordinary reading idle still uses retained Deep Sleep. Prior differential current measurements and remaining unverified rail tests are recorded in [power diagnosis](VALIDATION_POWER_DIAG_527.md).

`python tools/test_527_cache.py` requires a host GCC. It replays the actual C cache routines against GC16, DU, INIT and GL16 at the configured temperature, compares every packed output byte, checks bounded overflow, and verifies the cleaning-interval policy. Hardware optical checks remain necessary for ghosting and gray quality.

It also checks preview hits, mismatches, cancellation and restoration of the displayed old pixel state. `READER_ACTION` reports processing time and card sectors read; `EPD527_PREFETCH` reports cache hits, saved preparation time and time to first scan. These start after event dequeue, not at finger contact; taps dispatch on release. See [measured page-turn latency](VALIDATION_PREFETCH_527.md).

`python tools/test_settings_save.py` uses host G++ to exercise the actual writer with short writes and open/write/sync/close failures. Both checks run for the new board in CI.

The touch packet implementation follows the [FocalTech FT5x06 driver protocol](https://github.com/focaltech-systems/drivers-input-touchscreen-FTS_driver/blob/master/ft5x06.c): a three-byte header followed by six-byte contact records. The detected `0x54` family ID does not distinguish all FT5x46 suffixes. The SPI MSD snapshot retains its RT-Thread copyright/license header; `src/boards/sf32lb527/upstream.json` records the source-file hashes before the board-specific changes.
