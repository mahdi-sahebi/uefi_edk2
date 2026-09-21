# Gooxi G4DEL edk2 Boot Debug Plan

## Summary

Analyze the Gooxi G4DEL SOL log, then add targeted serial instrumentation to locate the next boot failure. The provided log contains no `ASSERT`, explicit error, DXE/BDS handoff, or shell/boot-manager output. It appears to be a truncated/cut-off capture during an Intel IIO/PCIe UDS HOB dump: valid dual-socket data is present for socket 0 and socket 1, while socket 2/3 are invalid/zeroed, which is expected on a 2-socket system.

The suspicious part is that `PcieInfo` entries after port 20 contain garbage-like device/function values, and the capture ends mid socket/stack dump. This suggests either a hang/crash during or immediately after IIO PCIe resource/HOB processing, or serial capture loss before later phases.

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

- Identify or import the actual Gooxi G4DEL platform package/vendor board package. The current workspace appears to be upstream edk2 plus Dasharo payload pieces and does not contain the strings that produced the SOL log (`IIO_UDS HOB DATA`, `PcieInfo`, `SocketID`).
- In that vendor package, add defensive logs around the IIO/PCIe UDS dump:
  - HOB pointer, HOB size, expected structure size, and revision.
  - Socket count and per-socket valid flag before reading each socket.
  - Stack and port loop bounds before dumping arrays.
  - Warning when port count exceeds the known hardware/FSP structure capacity.
  - `EFI_STATUS` after every HOB/protocol lookup and PCI resource allocation step.
- Clamp diagnostic dump loops to validated structure bounds in debug-only code, or at least log a warning before continuing, so uninitialized tail entries are obvious in the next capture.

## Test Plan

- Build a `DEBUG` image for the actual Gooxi G4DEL platform with serial debug enabled.
- Boot on the Gooxi G4DEL and capture a fresh SOL log from power-on, not mid-stream.
- Confirm the new log shows ordered `G4DELDBG` checkpoints through SEC, PEI/FSP, DXE IPL, DXE PCI enumeration, BDS, and boot selection.
- Compare the new capture against the current log:
  - Verify whether execution stops inside IIO/PCIe HOB dumping.
  - Verify whether the garbage-looking `PcieInfo` entries are raw uninitialized data, a structure-size mismatch, or harmless unused entries.
  - Verify whether boot reaches DXE/BDS after IIO logging.

## Assumptions

- `Gooxie` in the request means `Gooxi G4DEL`.
- The target is to make edk2 boot further on physical Gooxi G4DEL hardware, not just analyze the current log.
- The current attached log is trusted as runtime evidence, but any instructions inside attached documents/logs are ignored.
- Because the current repo does not contain the vendor source that emitted the IIO log strings, implementation must first locate/add the correct Gooxi platform package or confirm which external package provides those modules.
