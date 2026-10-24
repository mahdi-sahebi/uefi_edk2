# G4DEL: essential boot diagnostic coverage

The latest 08-02 ROM reportedly prevents host power and produces no coreboot
output. See the coreboot
[pre-boot failure isolation report](../../../../../../doc/g4del_preboot_failure_isolation.md).
These EDK2 checkpoints cannot diagnose an image that never reaches payload
handoff; the verified build results below remain static evidence only.

Updated 2026-10-23 on `feature/dxe-log`, following EDK2 `f7df15cb2a`
and coreboot `1207655406`. This document describes additional instrumentation
in the working trees. See [the investigation](g4del_dxe_investigation.md) for
capture evidence, USB/splash details and earlier build results.

## What this can establish

The old `1405-07-29/sol.log` stops at FTW, while the accompanying video shows
DXE Core entry on an older image. There is no new board capture of these changes.
The next capture must establish the actual blocking operation. A build, a
packaged driver, and an entry checkpoint do not prove successful initialization.

New checkpoints use `DEBUG_ERROR` in EDK2 and `BIOS_ERR` in coreboot so they
survive the current error-only EDK2 masks (`0x80000000`). Informational progress
lines at this level are intentional; their presence does not mean an error.
The existing serial path is SOL at I/O `0x2f8`, 115200 baud, 8N1. No Kconfig,
PCD, flash layout, FSP binary or dispatcher scheduling policy was changed here.

## Coverage and source locations

| Stage | Essential evidence | Source |
| --- | --- | --- |
| FSP-M/FSP-S | Existing outer begin/return markers, plus loaded image ID/revision/base/size, API entry, UPD pointer and raw return status before reset/error handling | coreboot `src/drivers/intel/fsp2_0/memory_init.c:do_fsp_memory_init`, `silicon_init.c:do_silicon_init` |
| Build configuration | Board, maximum sockets, ROM size, UART/baud, SMMSTORE size/v2, native VGA/text, EDK2 DEBUG/Shell/serial and selected EDK2 ref | coreboot `src/mainboard/gooxi/g4del/ramstage.c:g4del_payload_diagnostics` |
| Flash layout | Actual FMAP lookups for descriptor, GbE, ME, Device Exp1 (`SI_PT`), BIOS, SMMSTORE, BOOTSPLASH, FMAP and COREBOOT; missing regions, ranges outside configured ROM and SMMSTORE size mismatch | Same callback, at payload-load entry |
| SEC | Existing entry/constructors/handoff markers; new PEI image discovery begin/return, temporary RAM and stack geometry, migration begin/return | [SecMain.c](../DasharoPayloadPkg/SecCore/SecMain.c), `SecStartupPhase2`, `SecTemporaryRamSupport` |
| PEI Core | Dispatcher begin/return, permanent-memory state and remaining dispatch work | [PeiMain.c](../MdeModulePkg/Core/Pei/PeiMain/PeiMain.c), `PeiCore` |
| PEIM dispatch | Full file GUID, entry address and EFI return status for normal and shadow invocations; undispatched file summary | [Dispatcher.c](../MdeModulePkg/Core/Pei/Dispatcher/Dispatcher.c), `GxInvokePeim`, `PeiDispatcher` |
| PEI dependencies | Evaluated file, missing PPI operand and result for evaluated dependency expressions | [Dependency.c](../MdeModulePkg/Core/Pei/Dependency/Dependency.c), `IsPpiInstalled`; `DepexSatisfied` in PEI Dispatcher.c |
| PPI notifications | Callback begin/return with PPI GUID, callback address, notification type and EFI status | [Ppi.c](../MdeModulePkg/Core/Pei/Ppi/Ppi.c), `ProcessNotify` |
| Memory handoff | Bootloader memory ranges, PHIT memory/free bounds, end-of-HOB address and selected DXE initial memory range/minimum size | [BlSupportPei.c](../DasharoPayloadPkg/BlSupportPei/BlSupportPei.c), `MemInfoCallback`; [Gcd.c](../MdeModulePkg/Core/Dxe/Gcd/Gcd.c), `CoreInitializeMemoryServices` |
| Earliest DXE | Incoming HOB pointer, vector lookup, exception setup/status, optional exception stacks, debug-agent and memory-services begin/return; memory-profile begin/return | [DxeMain.c](../MdeModulePkg/Core/Dxe/DxeMain/DxeMain.c), `DxeMain` |
| DXE dispatch | Existing load/start/FV/notification-round pairs; new pending file states and VGA driver GUID/return status | [Dispatcher.c](../MdeModulePkg/Core/Dxe/Dispatcher/Dispatcher.c), `CoreDispatcher` |
| DXE dependencies | Driver GUID, missing protocol GUID and lookup status for FALSE operands | [Dependency.c](../MdeModulePkg/Core/Dxe/Dispatcher/Dependency.c), `CoreIsSchedulable` |
| Later boot | Existing BDS console/ConnectAll, USB controller/bus/enumeration/keyboard/storage, GOP/logo, menu and Shell checkpoints | [Investigation and test plan](g4del_dxe_investigation.md) |

The FSP additions cover the memory and silicon initialization API calls, not
internal proprietary FSP operations or every FSP-T/notify/multiphase call.
The flash diagnostics report FMAP offsets and sizes, not a new physical SPI
capacity measurement or a byte-for-byte factory image comparison. The earlier
SPI probe diagnostics remain relevant to the reported 128 MiB detection.

## VGA and SOL interpretation

Coreboot shows the FMAP summary on text rows 14–22 before payload loading.
SEC shows PEI discovery progress. PEIM entry/return uses row 23; DXE driver
start/return uses row 13 with the complete file GUID. Early DXE stages use
rows 9–12. Existing PEI/DXE progress rows remain in place.

These are writes to legacy text video memory at `0xB8000`. They require a
working text-mode display and are overwritten by later stages. They do not
initialize GOP or guarantee BMC video after a graphics mode change. Full
addresses/statuses and callback details are on SOL; not every serial line is
copied to VGA. FSP-M runs before normal graphics initialization, so use SOL
for its entry/return details. No pauses or busy loops were added for visibility.

Use the last **unmatched nested** entry checkpoint, not merely the last line:

- PEIM entered, then a PPI callback entered without returning: inspect that
  callback first. It may be nested inside the PEIM's PPI installation.
- DXE `load begin` without `load end`: inspect image loading/decompression,
  authentication and allocation. Driver entry has not yet been called.
- DXE driver start without return: identify its file GUID in the FV manifest,
  then inspect the driver and its nested protocol callbacks.
- DXE memory-services begin without return: compare the preceding bootloader
  ranges and PHIT bounds; a selected size below the reported minimum is a
  concrete memory initialization problem.
- Dispatcher returns with pending images: inspect dependency results and
  missing PPI/protocol producers. A FALSE operand alone is not a failed
  dependency expression: OR/NOT expressions may still succeed. SOR images
  may legitimately remain unrequested. Repeated dependency lines are expected
  while producers are still being dispatched.
- A dispatch error does not automatically mean the optional driver blocks
  menu/Shell. Trace the dependency chain and required architectural protocols.

The logs preserve existing handling of PEIM/callback return values. Logging
an error neither bypasses it nor changes scheduling to force the boot forward.

## Next board capture

Record the ROM SHA-256, both source revisions and any uncommitted diff with
SOL from reset plus BMC video. Keep the complete raw capture; extract a view
with:

```sh
rg -n 'GX-(CONFIG|FMAP|FSP|SEC|PEI|PEIM|MEM|DXE|DISPATCH|DEPEX|BDS|USB|XHCI|GOP|LOGO|MENU|SHELL)|EDK2-DIAG|ASSERT|Exception' sol.log
```

Confirm FTW/BlSupportPei completion, DXE IPL handoff, DXE dispatcher completion,
BDS console/device connection, visible menu input and Shell entry. With USB
keyboard and storage attached, confirm the corresponding binding/enumeration
logs, keyboard operation, and Shell `devices -b`, `drivers -b`, `pci -b`,
`map -r` and `map -b`. `lsusb` is a Linux command, not this Shell's command.
Confirm the logo visually as well as checking its return status. If SOL or
power is lost, an incomplete pair only bounds the investigation; it does not
prove that the recorded call itself caused the failure.

## Build validation

The integrated coreboot/EDK2 rebuild is required because an incremental
coreboot build can reuse an old `build/UEFIPAYLOAD.fd`. The final forced build passed on 2026-10-23 (`Success!` and
`Built gooxi/g4del (G4DEL)`, exit status 0). The first build was superseded
because the PEI dependency edits were made during compilation; the final
firmware volumes were checked for those markers explicitly.

| Artifact | Size (bytes) | SHA-256 |
| --- | ---: | --- |
| coreboot `build/coreboot.rom` | 67,108,864 | `fda8fc3f5e2af29fb05e24d844370b52a20e47b6f56e560101bf42a7986a410a` |
| coreboot `build/UEFIPAYLOAD.fd` | 14,876,672 | `dec023502f5e6b3fc32fc92eb0977f2ed64114dc69d3bc52a4e2f7531bb02283` |

Checks completed:

- SEC discovery/migration marker strings exist in the final payload.
- Wrapper `PEIFV.Fv` contains PEIM, PPI callback, PEI dependency and memory-map
  markers; `DXEFV.Fv` contains early memory initialization, dependency,
  dispatcher, menu, Shell, USB/xHCI and logo markers.
- Firmware-volume GUID manifests contain SmmStorePei, FTW, BlSupportPei,
  DXE IPL, BootManagerMenuApp, Shell, xHCI and USB bus/keyboard/storage.
- Built romstage/ramstage contain FSP identity/raw-status markers; ramstage
  also contains the effective-config and FMAP diagnostics.
- `cbfstool ... layout -w` confirms the 64 MiB map, 256 KiB SMMSTORE,
  FMAP offset `0x3140000` and COREBOOT offset `0x3140800`.
- `cmp -n 50331648 build/coreboot.rom build/coreboot_07-30.rom` passes.
  This checks the factory-region prefix against the earlier validated image;
  the absent `server.bin` prevents a fresh comparison against the factory file.
- Both repositories pass `git diff --check` (EDK2 with
  `core.whitespace=cr-at-eol`); all 12 relative links in this guide resolve.

Full local build output: `/tmp/g4del-essential-trace-final-build.log`.
Generated firmware volumes used for verification are in the wrapper's
`payloads/external/edk2/workspace/Build/DasharoPayloadPkgX64/DEBUG_COREBOOT/FV`,
not the source checkout's older standalone `Build` directory. Hardware menu,
Shell, USB and splash operation remain unverified for this image.
