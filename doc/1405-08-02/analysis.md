# G4DEL 1405-08-02 serial log analysis

## Finding

The capture reaches DXE Core and begins dispatching DXE drivers. Its final complete
checkpoint is the entry into `CpuDxe`, whose file GUID is
`1A1E4886-9517-440E-9FDE-3BE44CEE2136`. The final bytes end in the middle of
`[GX-DXE] seq=13 DXE start 1A1E4886-9517-440E-9FDE-3BE44...`. There is no
`CpuDxe` return marker, exception report, reset reason, or status code after
that point.

**Explicit observed problem:** DXE driver dispatch does not show `CpuDxe`
returning. The stop is inside the interval from its dispatcher entry through
its `InitializeCpu` entry point, or the capture ended while the driver was
running. This log alone cannot identify the failing instruction or prove a CPU
fault. The VGA loss is not proof that GOP failed; the serial trace ends before
later graphics and BDS checkpoints.

## Timeline from `/home/uefi/workspace/log/1405-08-02/serial.log`

| Log lines | Observation | Interpretation |
| --- | --- | --- |
| 2–4, 129–136 | FSP-M and FSP-S report entry and return with status `0x00000000`; FSP-M returns HOB `0x63900000`. | Both FSP calls complete. |
| 131–172 | Missing `logo.bmp`, SPI probe says `JEDEC=00:0000`, probed size `0x8000000` versus configured `0x4000000`, IPMI/KCS and keyboard-controller errors, and repeated `uncore_acpi.c` assertions. | Coreboot reports these errors but continues. They are not the last operation in this trace. Investigate the flash-size mismatch separately before future flashing. |
| 173–193 | Configuration reports Gooxi G4DEL, two sockets, 64 MiB ROM, UART `0x2f8` at 115200; payload loads and coreboot jumps to EDK2 at `0x802ba0`. | Coreboot reaches payload handoff. The log identifies `origin/feature/dxe-log`, but does not include a ROM hash. |
| 194–229 | EDK2 SEC finds PEI entry `0x811F1C`; SEC/PEI checkpoints continue. | SEC completes and PEI starts. |
| 230–330 | PEI dispatcher initializes and returns the first modules. FTW reports working workspace `0xFF010000+0x10000` invalid, spare `0xFF020000+0x20000` invalid, then installs its completion PPI and returns `Success`. | FTW workspace contents are invalid, but this is not where this capture stops. Variable-store behavior later remains at risk. |
| 350–429 | `BlSupportPei` reports memory ranges, installs PEI memory at `0x5F510000` with size `0x4000000` and status `Success`, then reports `Error when parsing platform info, Status = Not Found`; the PEIM returns `Not Found`. | This is a separate PEI error. Dispatch continues; it is not the last operation. |
| 468–624 | Permanent-memory PEI dispatch runs; `End of Dispatching` and dispatcher return are logged. | PEI dispatch completes. |
| 637–665 | DXE IPL loads DXE Core and logs `DXE IPL -> DXE entry=0x634AA68A`. | DXE Core handoff completes. |
| 824–837 | DXE HOB scan completes; event services, memory protection, vector handoff, decompression and firmware-volume services report ready; DXE dispatcher initializes and starts. | DXE Core initialization reaches driver dispatch. |
| 839–1065 | Multiple DXE drivers load, enter and return successfully. At line 1063 the dispatcher begins loading `1A1E4886-9517-440E-9FDE-3BE44CEE2136`; line 1065 reports driver entry. | This GUID is `UefiCpuPkg/CpuDxe/CpuDxe.inf`. No return follows. |
| 1066 | The file ends mid-message during the `CpuDxe` start breadcrumb; it has no final newline. | Abrupt capture end; no explicit fatal error is recorded. |

## Confirmed errors and risks

- **DXE stop location:** `CpuDxe` entry with no return is the last complete driver checkpoint. The current source performs page-table setup, FPU setup, interrupt/GDT/IDT setup, CPU architectural protocol installation, MTRR/GCD refresh, local APIC setup, idle-event creation, and MP initialization in `InitializeCpu`.
- **Likely area to inspect first:** `MpInitLibInitialize()` wakes and enumerates application processors when there is no CPU handoff HOB. A wait that does not complete, or a CPU-count assertion, could occur there. The log does not currently show whether a handoff HOB exists or how many processors check in.
- **Processor-capacity mismatch risk:** the EDK2 payload sets `MAX_LOGICAL_PROCESSORS=64`; coreboot is configured with `CONFIG_MAX_CPUS=255`. The actual CPU models and logical processor count are absent from the capture. Do not call this the cause until runtime counts are recorded.
- **Invalid FTW workspace:** both the working and spare workspace validation paths fail, despite the completion PPI being installed successfully. This can affect variable services and boot options after the CPU driver issue is resolved. It is a distinct confirmed error, not the observed stop.
- **Platform information lookup:** `BlSupportPei` returns `Not Found` while PEI memory installation succeeds and dispatch continues. Determine which platform-information PPI/HOB was expected before deciding whether a code or configuration change is required.
- **Coreboot messages:** FSP-M/S return success, coreboot loads EDK2, and the payload handoff marker is present. The missing splash asset, SPI probe mismatch, IPMI/KCS errors, keyboard cleanup error, and ACPI assertions need their own investigation; this capture does not tie them to the later CPU DXE stop.

## New tracing added

`UefiCpuPkg/CpuDxe/CpuDxe.c` emits paired `[GX-CPU]` SOL messages around each major `InitializeCpu` operation. The markers include page tables, FPU, interrupts, GDT, IDT/exception handlers, CPU Arch protocol installation, MTRR and paging attribute refresh, local APIC, idle event, and MP support. Calls with an EFI status log that status. The CPU checkpoints do not write to `0xB8000`: once EDK2 changes page tables or switches to linear graphics, an unverified legacy VGA text-aperture write is unsafe. PEI's existing text breadcrumbs are separate and only useful while text mode remains active.

`UefiCpuPkg/CpuDxe/CpuMp.c` logs entry/return for MP library initialization, detected and enabled processor counts, the configured maximum, and MP Services protocol installation.

`UefiCpuPkg/Library/MpInitLib/MpLib.c` logs whether CPU handoff HOB data exists, selected processor capacity, AP state allocation, initial AP wake-up, AP check-in counts, and the configured timeout. In particular, the last marker distinguishes a stop inside AP wake-up from later CPU initialization.

These changes add observability and do not suppress assertions or change AP startup policy. The processor limit remains unchanged until a capture reports the installed count. The current serial log and VGA recording do not contain the new markers.

Source checkpoints: `UefiCpuPkg/CpuDxe/CpuDxe.c:1042-1119` brackets CPU
initialization; `UefiCpuPkg/CpuDxe/CpuMp.c:780-825` records the processor count
and MP Services installation; `UefiCpuPkg/Library/MpInitLib/MpLib.c:581-600,
1238-1466, 2100-2330` records processor-HOB selection, AP state allocation,
startup, waits, and enumeration.

## Next capture requirements

Record the flashed ROM SHA-256 and export the complete SOL output from reset. Preserve the raw log, including any final partial line. At the stop, report the last `[GX-CPU]` and `[GX-MP]` begin/return pair, VGA row 24, BMC reset/power status, and any BMC event/POST code. If MP initialization does not return, use the AP check-in counts and whether the CPU handoff HOB was present to distinguish absent AP startup data, missing AP check-ins, and capacity mismatch. Do not change processor limits or bypass a failed CPU initialization before that evidence is available.

Coreboot diagnostic messages should be reported separately from the EDK2 stopping point. Recheck the `JEDEC=00:0000` / 128 MiB probe versus 64 MiB configured ROM discrepancy before any flash operation.

## 2026-10-25: flash, graphics, and USB integration review

The factory image at `/home/uefi/workspace/gooxie_bios/server.bin` is a
64 MiB input with SHA-256
`db303ef4cefe79372f508f061d4dafe79cb5d98cfcb84127a7297b4c88100465`. The
current coreboot integration FMD now uses `SI_DEVICEEXT` for the 64 KiB IFD
Device Exp1 area and reserves the 256 KiB SMMSTORE, 1 MiB BOOTSPLASH, FMAP, and
COREBOOT areas inside the 16 MiB BIOS range. The full IFD/FMAP details and
UEFIExtract BIOS tree are documented in the local coreboot worktree at
[factory image inventory](../../../../../../../doc/g4del/factory_image_inventory.md)
and `server_bios_inventory.csv`. The extracted factory data remains a local
build input; the factory BIOS and variable values are not copied into the
open-source payload.

The historical offline inspection reports in
`/home/uefi/workspace/gooxie_bios/` are ASRock SPC741D8 reports. One included
64 MiB `coreboot.rom` has SHA-256
`1b1b93183a61ae2391c8c1000aa6ba5c6417c8db7688e78073be0230685c6870`, while
the paired recipe claims a different MD5. They are not evidence about
`server.bin` or the current Gooxi build. The exact factory image attribution,
region map, and report mismatch are recorded in the coreboot inventory guide.

### Graphics and USB instrumentation now in source

Coreboot's G4DEL defconfig now explicitly selects
`GENERIC_LINEAR_FRAMEBUFFER`; its prior saved `.config` still selected VGA
text mode until regenerated. The checked-in defconfig also pins EDK2 to
integration revision `d3f653673e6bc70177094b11c1ee1ecd353b0f2e`. Coreboot's
ramstage diagnostics report each ASPEED PCI function's vendor/device ID so
the VGA ID can be compared with the current driver match (`0x1a03:0x2000`).
The 08-02 log does not show the actual board ID, so that driver match remains
unverified and has not been changed on speculation.

`PlatformBootManagerAfterConsole()` now performs `EfiBootManagerConnectAll()`
for the G4DEL diagnostic build, waits one second for enumeration, queries each
USB2 host-controller protocol, and inventories USB I/O handles. Serial records
include descriptor statuses, VID/PID, device and interface classes, available
strings, and device paths. The UEFI USB I/O protocol does not report negotiated
per-device link speed; host-controller maximum speed is logged instead.
Descriptor failures are logged and leave partial inventory rows rather than
discarding the rest of enumeration.

The helper then displays a ten-second scrollable inventory with EDK2
`ConOut`: Up/Down and Page Up/Page Down scroll; Enter/Esc continues; timeout
continues automatically. The page is visible on VGA only if the VGA console is
connected, and mirrored on SOL only if serial is an active `ConOut`. BDS then
requests the existing Boot Manager Menu. Shell remains a selectable menu
option; no Shell auto-launch or new public UEFI API was added. The
`BootManagerMenuApp`, Shell, GOP/console, xHCI, USB bus, keyboard, mouse, and
mass-storage modules are present in the payload file list, but driver binding
and attached-device enumeration still require the next board capture.

These changes do not alter the current observed stopping point: the latest
hardware trace still ends during `CpuDxe` with no return marker. It contains no
USB, GOP, BDS, menu, or Shell evidence. The updated path and screenshots/log
requirements are in `graphics_splash_shell.md` and
`remaining_boot_path.md`. Source/build checks must be recorded separately from
hardware success.

## Earlier artifact, superseded on 2026-10-25

The old ROM hash `f9bda3c1a9376d51c811f1e4657094a76a8ca627f7a4e83cae5651925868c46d`
used the obsolete `SI_PT` FMAP name. It is preserved losslessly at
`build/coreboot.rom.pre-g4del-final.gz`; its uncompressed hash is recorded
above. The active `test/uefi_coreboot/build/coreboot.rom` and
`build/UEFIPAYLOAD.fd` now contain the verified integration artifacts below.

## Intermediate implementation and static verification (2026-10-24)

The EDK2 integration worktree now contains CpuDxe and MpInitLib checkpoints described above. The CpuDxe INF also declares `PcdCpuMaxLogicalProcessorNumber` as a consumed PCD; adding the source reference without that INF declaration failed compilation, so the declaration is included. This change is instrumentation and build metadata only: it does not increase the processor limit or alter AP startup policy.

The current coreboot configuration built successfully with the instrumented EDK2 payload (`Built gooxi/g4del (G4DEL)`). The host lacks `uuid-dev`, so the build command skipped coreboot's `checktools` prerequisite check; the actual EDK2 and coreboot compilation and ROM assembly completed. Configuration evidence: `CONFIG_ROM_SIZE=0x04000000` (64 MiB), `CONFIG_MAX_CPUS=255`, `CONFIG_EDK2_HAVE_EFI_SHELL=y`, generic AST linear framebuffer enabled, and payload ref `origin/feature/dxe-log`. The build also packages `BOOTSPLASH/logo.bmp` and its CBMEM handoff; see [graphics, splash, menu, and Shell status](graphics_splash_shell.md).

Build artifacts and checks:

- Intermediate `build/UEFIPAYLOAD.fd`: 14,876,672 bytes; SHA-256 `adf766c880ef4dfacf02e36618c26aa1d38859dc322e1b19faba46df3daf5a97`.
- Intermediate `build/coreboot.rom`: 67,108,864 bytes; SHA-256 `f9bda3c1a9376d51c811f1e4657094a76a8ca627f7a4e83cae5651925868c46d`. Its FMAP used the old `SI_PT` name, so this is not the map-verified image.
- The rebuilt DXE firmware-volume manifest lists `BootManagerMenuApp`, `LogoDxe`, `GraphicsOutputDxe`, `XhciDxe`, `UsbKbDxe`, `UsbMassStorageDxe`, and `Shell.inf`. The payload contains `[GX-GOP]`, `[GX-LOGO]`, `[GX-CPU]`, `[GX-MP]`, `[GX-BDS]`, and `[GX-USB]` strings.
- The final ROM has a 1 MiB `BOOTSPLASH` CBFS region at flash offset `0x03040000`; `logo.bmp` is 5,172 bytes compressed with LZMA (200,138 bytes decompressed). Extracting it reproduces `Documentation/coreboot_logo.bmp` byte for byte.
- The DXE firmware-volume manifest contains CpuDxe GUID `1A1E4886-9517-440E-9FDE-3BE44CEE2136`, BootManagerMenuApp GUID `EEC25BDC-67F2-4D95-B1D5-F81B2039D11D`, and Shell GUID `7C04A583-9E3E-4F1C-AD65-E05268D0B4D1`.
- `git -c core.whitespace=cr-at-eol diff --check` passes for the EDK2 changes, preserving the source files' CRLF line endings. The enclosing coreboot and nested EDK2 source checkouts have no tracked modifications from the build overlay; the implementation changes are in the `feature/g4del/integration` EDK2 worktree.

These are build and packaging checks only. The ROM was not flashed, its hash is not associated with a board attempt, and there is no new SOL, VGA, BMC, or interactive menu/Shell result. The original 08-02 capture remains the latest hardware evidence and does not contain these new checkpoints.


## 2026-10-25 final build and USB instrumentation audit

The final EDK2 payload and coreboot image were rebuilt after the USB inventory
review. Coreboot HEAD is `6e4458e8660fd0d32d921f3cd95543f8afc75e05`; EDK2 HEAD is
`d3f653673e6bc70177094b11c1ee1ecd353b0f2e`. The generated EDK2 workspace is
local build output, not a separate source revision.

A payload binary check exposed a real integration defect: EDK2's `-D` build
argument set a DSC macro but did not define the C preprocessor symbol used by
the G4DEL `#if` blocks. The earlier payload therefore contained only the
existing generic USB logger; it did not contain the inventory page or forced
menu path. `DasharoPayloadPkg.dsc` now turns the DSC macro into the compiler
flag `-DGOOXI_G4DEL_USB_DIAGNOSTICS=1` when coreboot selects the G4DEL board.
The final compiler response file contains that flag, and the rebuilt payload
contains the inventory page, descriptor-field, no-input fallback, and forced
menu diagnostic strings. This is build evidence; the board has not executed
these paths.

The inventory now logs USB device/config/interface descriptor statuses,
VID/PID, USB and device revision, endpoint-zero packet size, configuration
attributes/power/interface count, interface class fields, strings, device
path, and controller capability/state. UEFI's USB I/O protocol does not expose
negotiated per-device speed, so the implementation logs each host controller's
maximum supported speed and labels the per-device value unavailable. It still
collects and logs devices if `ConIn` is absent; the VGA inventory page waits
for its bounded timeout if key input is unavailable. Returned USB strings are
freed on both success and error paths.

Build results:

- `build/UEFIPAYLOAD.fd`: 14,876,672 bytes, SHA-256
  `87e3939fce12305e1a71bd4e945f544ca6b31939df2a9145cbe6514d196d9c76`.
- `build/coreboot.rom`: 67,108,864 bytes, SHA-256
  `7169a0d953b62239d50cf6f24aa185244876334b9c9d54f7becc1ed02a77fce3`.
- Those verified artifacts were copied from the successful integration build
  into the active `test/uefi_coreboot/build` directory. The coreboot and EDK2
  source files copied into the active checkouts compare byte-for-byte with the
  built integration sources, and the active `.config` now matches the build
  configuration. The active checkout was not rebuilt a second time because
  the filesystem had only 64 MiB free. The former payload is preserved as
  `build/UEFIPAYLOAD.fd.pre-g4del-final`.
- `util/g4del_stock.py --compare` confirms descriptor, GbE, ME, and Device
  Exp1 bytes match `server.bin`. `--verify-layout` and `cbfstool layout -w`
  pass. All 256 KiB of new SMMSTORE read as `0xff`.
- `cbfstool print -r BOOTSPLASH` shows `logo.bmp` (5,172 stored bytes,
  200,138 bytes decompressed). The payload manifest includes BootManagerMenuApp,
  Shell, GraphicsOutputDxe, LogoDxe, XhciDxe, UsbBusDxe, UsbKbDxe,
  UsbMouseDxe, and UsbMassStorageDxe.
- The source FSP is `$SPR-SP$`, FSP spec 2.3, image revision 1.1.1 build 917,
  header revision 6. The source and component hashes are in the coreboot
  [`factory image inventory`](../../../../../../../doc/g4del/factory_image_inventory.md).
- EDK2 `git -c core.whitespace=cr-at-eol diff --check`, coreboot `git diff
  --check`, the stock-region comparison, and XML parsing of both SVG diagrams
  pass.

The ROM was assembled by building `build/coreboot.rom` and adding the bundled
logo to the `BOOTSPLASH` CBFS region with `cbfstool`. The final ROM was not
flashed or read back. No new BMC, SOL, VGA, menu, USB-device, or Shell
interaction evidence exists. `CpuDxe` remains the last stage confirmed by
hardware, with no return marker in the 08-02 capture.
