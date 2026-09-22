# UDS OTA / Bootloader Core

LINK's bootloader core is the fail-closed orchestration layer for a production
UDS programming implementation. It deliberately does **not** contain STM32 flash
addresses, option-byte manipulation, OEM keys, certificate private keys or HSM
secrets.

The implementation follows the programming sequence captured in MBLINK issue
#56 and the referenced UDS flash-programming material:

1. DiagnosticSessionControl 0x10/0x02 -> `enter_programming_session`.
2. SecurityAccess 0x27 -> the existing UDS SecurityAccess implementation must
   succeed before `grant_security`.
3. ControlDTCSetting 0x85 and CommunicationControl 0x28 ->
   `set_dtc_recording_disabled` and `set_communication_disabled`.
4. RequestDownload 0x34 -> `request_download`, which selects an inactive A/B
   slot and rejects versions at or below the monotonic installed version.
5. TransferData 0x36 -> `transfer_data`, which enforces the UDS block sequence
   counter, exact offset progression and declared image size.
6. RequestTransferExit 0x37 -> `request_transfer_exit`, accepted only when the
   complete declared image arrived.
7. RoutineControl 0x31 CheckMemory -> `check_memory`, requiring integrity and,
   by default, authenticity verification before the candidate may advance.
8. ECUReset 0x11 -> `stage_for_reset`; the inactive slot is not staged until
   all verification gates pass.
9. After a successful boot/health decision, `confirm_boot` commits the
   monotonic version counter. A failed candidate therefore cannot prematurely
   invalidate the known-good slot.

## A/B, anti-rollback and secure boot

The backend selects the inactive slot. LINK never overwrites the running slot by
itself. The monotonic version callback is read before download and committed only
after the new image has actually booted successfully.

Integrity, authenticity and secure-boot candidate validation are distinct hooks.
An STM32F767 product can bind those hooks to its chosen hash/signature/HSM
implementation without LINK knowing or exposing any OEM secret.

## Fail-closed defaults

`LINK_UDS_BOOTLOADER_CONFIG_INIT` sets `allow_programming = false`.
Arming also fails unless all mandatory backend operations exist. That means
merely linking this module cannot enable flashing.

The core contains no direct STM32F767 erase/program primitive. A target-specific
flash driver must enforce its own flash geometry, RAM-execution requirements,
power-loss behaviour and protected boot-region rules before programming is
enabled. This separation keeps the generic UDS state machine portable and makes
the dangerous hardware operation an explicit product decision rather than an
accidental library side effect.
