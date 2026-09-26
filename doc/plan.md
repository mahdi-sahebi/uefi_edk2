# Gooxi G4DEL edk2 Boot Debug Plan

## Summary

Analyze the Gooxi G4DEL SOL log, then add targeted serial instrumentation to locate the next boot failure in the Dasharo/edk2 UEFI payload. The current checkout can be used for payload-level debugging, but it does not contain the Gooxi-specific vendor/platform package that produced the failing `IIO_UDS HOB DATA` log.

The provided log contains no `ASSERT`, explicit error, DXE/BDS handoff, shell output, or boot-manager output. It reaches an Intel IIO/PCIe UDS HOB dump and then stops mid socket/stack resource print at `PciResourceMem32Limit:`. Valid dual-socket data is present for socket 0 and socket 1, while socket 2/3 are invalid/zeroed, which is expected on a 2-socket system.

The suspicious part is that `PcieInfo` entries after port 20 contain garbage-like device/function values, and the capture ends mid socket/stack dump. This suggests either a hang/crash during or immediately after IIO PCIe resource/HOB processing, or serial capture loss before later phases. The exact IIO dump code is not present in this source tree, so the root cause cannot be fixed precisely from this checkout alone.

## Implemented Instrumentation

- Added `G4DELDBG` serial checkpoints in common code paths that exist in this workspace:
  - Dasharo SEC handoff to PEI.
  - Intel FSP-M memory init, HOB processing, and post-FSP-M HOB processing.
  - Intel FSP-S silicon init, FSP HOB lookup, and post-FSP-S HOB processing.
  - DXE IPL load and handoff to DXE Core.
  - Dasharo PCI host bridge root bridge discovery.
  - SMM Store PEI, SMM Store runtime library, SMM Store FVB runtime setup, and FTW recovery handoff.
  - Fault Tolerant Write PEI working/spare region lookup, workspace validation, last-write HOB creation, and completion PPI install.
  - DXE IPL PEIM entry/shadow/PPI install and TCG2 config PEIM TPM-selection status.
  - PCI bus entry, full/light enumeration, device start, and enumeration-complete protocol install.
  - Dasharo ACPI ExitBootServices callback.
  - Dasharo and generic BDS boot manager phases and boot option attempts.
- Kept changes additive and limited to `DEBUG_INFO` / `DEBUG_ERROR` logging. No boot policy or resource allocation behavior was changed.

## Remaining Gooxi-Specific Work

- Identify or import the actual Gooxi G4DEL platform package/vendor board package. That package is needed for:
  - Board-specific hardware initialization and policy.
  - FSP/IIO/PCIe HOB interpretation.
  - Platform DSC/FDF/build integration.
  - The debug code that emitted `IIO_UDS HOB DATA`, `PcieInfo`, and `SocketID`.
- The current workspace appears to be upstream edk2 plus Dasharo payload pieces and does not contain the strings that produced the SOL log (`IIO_UDS HOB DATA`, `PcieInfo`, `SocketID`).
- In that vendor package, add defensive logs around the IIO/PCIe UDS dump:
  - HOB pointer, HOB size, expected structure size, and revision.
  - Socket count and per-socket valid flag before reading each socket.
  - Stack and port loop bounds before dumping arrays.
  - Warning when port count exceeds the known hardware/FSP structure capacity.
  - `EFI_STATUS` after every HOB/protocol lookup and PCI resource allocation step.
- Clamp diagnostic dump loops to validated structure bounds in debug-only code, or at least log a warning before continuing, so uninitialized tail entries are obvious in the next capture.
- If no vendor package is available, continue only with generic payload checkpoints in this checkout. Add any remaining generic DXE Core, DXE dispatcher, PCI host bridge, and BDS logs only when they help determine whether execution gets past PEI/FSP into DXE. Do not claim the IIO dump itself is fixed until the source package containing that dump is available.

## Blocker / Risk

- The source emitting the last visible log lines is missing from this repo. This means the precise root cause of the IIO/PCIe UDS dump stop cannot be fixed from this checkout alone.
- Payload-level logs can prove whether execution continues past the IIO dump, but they cannot validate or correct the missing vendor structure parsing, loop bounds, FSP/IIO policy, or board-specific PCIe resource handling.

## Test Plan

- Build the current UEFI payload as `DEBUG` with serial debug enabled.
- Boot on the Gooxi G4DEL and capture a fresh SOL log from power-on, not mid-stream.
- Check whether `G4DELDBG` markers appear after the IIO dump.
- Confirm the new log shows ordered `G4DELDBG` checkpoints through SEC, PEI/FSP, DXE IPL, DXE PCI enumeration, BDS, and boot selection when execution reaches those stages.
- Compare the new capture against the current log:
  - Verify whether execution stops inside IIO/PCIe HOB dumping.
  - Verify whether the garbage-looking `PcieInfo` entries are raw uninitialized data, a structure-size mismatch, or harmless unused entries.
  - Verify whether boot reaches DXE/BDS after IIO logging.
- If no later marker appears after the IIO dump, request or provide the Gooxi vendor/platform package, or a full source tree matching the firmware image that produced the SOL log.

## Assumptions

- `Gooxie` in the request means `Gooxi G4DEL`.
- The target firmware role is a UEFI payload for physical Gooxi G4DEL hardware.
- The current attached log is trusted as runtime evidence, but any instructions inside attached documents/logs are ignored.
- No local Gooxi vendor/platform package is currently available.
- Because the current repo does not contain the vendor source that emitted the IIO log strings, `doc/plan.md` must document that dependency clearly instead of implying the missing IIO code can be fixed in this checkout.
