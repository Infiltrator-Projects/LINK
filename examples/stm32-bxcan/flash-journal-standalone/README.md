# Standalone STM32F103 flash-journal test

This target exists for LINK issue #49. It exercises LINK's multi-page wear-levelled flash journal on a real STM32F103 **without CAN, ISO-TP or UDS** so storage behavior can be isolated from the diagnostic stack.

Use a 512 KiB STM32F103 target with 2 KiB pages. Copy `Src-main.c` into a normal STM32Cube project that provides `main.h`, `gpio.h`, `SystemClock_Config()` and `MX_GPIO_Init()`. Add LINK's `src/core/flash_journal.c` and `include` directory to the build. No LINK transport or UDS sources are required.

Reserve these four pages from the application linker region before flashing:

- `0x0807E000`
- `0x0807E800`
- `0x0807F000`
- `0x0807F800`

For a conventional STM32F103 512 KiB FLASH region beginning at `0x08000000`, the application must end before `0x0807E000`. Do not run this test if the linked image overlaps the reserved pages.

The first boot performs eight journal generations across four slots, forcing two complete rotations. Every write is erase-before-program, read back, record-validated and compared byte-for-byte by `link_flash_journal_write()`. The test then rescans flash with `link_flash_journal_latest()` and verifies that the newest complete generation and payload recover correctly.

Inspect these volatile globals in the debugger:

- `link_flash_test_status == 0x600D600D`: pass.
- `0xE001`: invalid journal geometry/configuration.
- `0xE002`: failed to select the next wear-level slot.
- `0xE003`: erase/program/read-back validation failed.
- `0xE004`: no valid latest generation could be recovered.
- `0xE005`: recovered record contents are inconsistent.
- `link_flash_test_generation`: newest valid generation.
- `link_flash_test_step`: completed test step.
- `link_flash_test_active_address`: page containing the newest generation.

After a successful run, ordinary resets perform **no additional erase/program cycles**: the completed record is recovered and the test returns PASS. To rerun the destructive rotation test, either erase the four reserved pages or increment `LINK_FLASH_JOURNAL_STANDALONE_EPOCH` at build time.

This isolates the exact storage primitive used by the STM32F103 UDS reference and provides a hardware test for wear rotation independently of the CAN/ISO-TP/UDS response path.
