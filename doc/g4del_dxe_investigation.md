# G4DEL DXE investigation (2026-10-23)

Goal: boot the Gooxi G4DEL to an interactive EDK2 boot menu and launch the
built-in Shell. This note records what can be established from the available
captures and current `feature/dxe-log` source. The porting history and ROM
map are in [g4del_porting_and_debugging.md](g4del_porting_and_debugging.md);
the original text capture is analyzed in [1405_07_29.md](1405_07_29.md).

## Evidence boundary

Raw captures remain under `/home/uefi/workspace/log/`; this report records
their locations and observations rather than copying large videos or serial
files into Git. The archive filenames containing `01011970` reflect the BMC's
clock and should not be treated as boot dates. Use the containing folder and
recorded video timestamp, plus a ROM hash, to associate each future run.

| Evidence | Last useful observation | Meaning |
| --- | --- | --- |
| `log/1405-07-29/sol.log` | `FTWa-3` at line 231 | The text capture ends in the old FTW artificial delay. It does not show a fault. |
| `log/1405-07-29/Screencast from 21-10-26 12_32_27.m4v` | At about 110 seconds, `[GX-DXE] serial entry`, then `seq=0 DxeCore entry` | The old image reaches DXE Core. The recording ends during subsequent output. |
| `log/1405-07-28/Screencast from 20-10-26 15_40_30.m4v` | VGA shows DXE Core GCD initialization, then loses video signal | Video loss alone does not establish a CPU crash; the serial output must be captured after that point. |
| `log/1405-07-25/Untitled.jpeg` | DXE dispatcher initialization is visible | This older, unidentified build got farther, but it is not evidence for the current image. |

Other archives are the SOL logs in `1405-06-28`, `1405-07-01`, `1405-07-07`,
`1405-07-11`, and `1405-07-28`, and screencasts in `1405-07-11`,
`1405-07-13`, `1405-07-25`, and `1405-07-29`. The older text logs are
truncated during coreboot/FSP or the old PEI path; the 2026-10-20 text log
ends at `FTWa-2`. The `1405-07-27` folder contains historical diagnostic
patches, a review and a build log, not a post-fix reset-to-Shell capture.
Some old serial files contain escape sequences or NUL bytes; retain the raw
files but use filtered text only for searching markers. The two 2026-10-22
PNG files in `1405-07-29` are IDE screenshots, not boot frames.

The recordings do not identify their flashed ROM hashes. There is no reset to
menu SOL capture of the current G4DEL build. A post-fix board test is required
to find the actual DXE stopping point and to verify keyboard/SOL input.

| Goal stage | Current evidence | Status |
| --- | --- | --- |
| Coreboot/FSP and EDK2 SEC/PEI | Old SOL captures and current image build | Historical board progress; current image untested |
| DXE Core entry | 2026-10-21 SOL screencast | Observed on an unidentified old image |
| DXE dispatcher completion and BDS | Current modules and checkpoints built | Hardware unverified |
| Interactive menu and Shell | Both applications in the current DXE FV | Hardware unverified |
| xHCI, keyboard and USB storage | Drivers and runtime markers in the current DXE FV | Binding and device detection unverified |
| EDK2 built-in logo | `LogoDxe`, draw and retry paths in the current DXE FV | Display on BMC VGA unverified |

## Static checks

The current coreboot `.config` selects `gooxi/g4del`, two sockets, 64 MiB
ROM, Eagle Stream FSP-T/M/S, SMMSTORE v2 (256 KiB), SOL at I/O `0x2f8` and
115200 baud, EDK2 menu and built-in Shell, and no optional TPM. The FSP
components are present in CBFS. `board.fmd` reserves the factory descriptor,
GbE, ME, and Device Exp1 regions below the 16 MiB BIOS region; SMMSTORE occupies
the first 256 KiB of BIOS. The existing 64 MiB ROM's FMAP and CBFS agree with
these addresses. The factory `server.bin` is absent now, so the earlier
byte-for-byte comparison of stock regions cannot be repeated here.

The EDK2 FDF places `SmmStorePei`, `FaultTolerantWritePei`, `BlSupportPei`,
then DXE IPL in the PEI firmware volume. The DXE firmware volume includes DXE
Core, variable storage, FTW, SMMSTORE FVB, PCI/serial and console drivers,
`BootManagerMenuApp`, and Shell. This proves packaging, not successful driver
dispatch or console connection on the board. The current G4DEL IIO table is
provisional; PCIe routing and graphics controller BARs need runtime evidence.

The integrated `make -j8` build completed on 2026-10-23. An initial
diagnostic ROM had SHA-256
`85a1d0c7afb069ff66c4bb8d9dee70d9486c3d159ed09ee276aa0a808f6abf9f`.
After adding USB and logo logs, an incremental `make` left that payload
unchanged. The payload artifact was moved aside and the wrapper was forced to
rebuild it, producing ROM SHA-256
`22f3f55c17c530dcf9289e3bdf8c5d2c04e96212b14af54ac3260ad68b38efdc`.
A final forced build added the logo retry after device connection. The
**current** `build/coreboot.rom` is 67,108,864 bytes, SHA-256
`5ce4c49d71076f7bae3c0f16dd910a22d89c1ed6a683361ee40650fd63e8af26`.
The current wrapper-produced `build/UEFIPAYLOAD.fd` is 14,876,672 bytes,
SHA-256
`e1d680421a048263a80fd6cac89c4ba6c52b1e1276d7d1c5ac4af0edb0c75d6e`.
The first `0x3000000` bytes of this ROM match the previously validated
`build/coreboot_07-30.rom` byte for byte. `cbfstool layout` shows descriptor
at `0x0`, GbE at `0x1000`, ME at `0x3000`, Device Exp1 at `0x2ff0000`,
SMMSTORE at `0x3000000`, and COREBOOT at `0x3140000`. The generated FV
manifests include the four PEI modules above and BootManagerMenuApp/Shell.
Both repositories pass `git diff --check` (EDK2 with CRLF treated as line
endings). The wrapper's generated DXE FV is under
`payloads/external/edk2/workspace/Build`, separate from the EDK2 checkout's
own `Build`. The wrapper FV contains the USB, logo, menu, Shell and DXE
dispatcher marker strings. This is static verification only.

The old VGA loss could occur during a switch from text mode to graphics output.
The EDK2 graphics driver now logs its PEI graphics HOB, selected BAR and final
start status to SOL. It returns `EFI_NOT_FOUND` if the HOB is missing or short,
instead of asserting and dereferencing it. SOL remains the primary witness
while video changes mode.

## Diagnostic changes in this working tree

- PEI Core's legacy VGA delay helper no longer spins through millions of
  arithmetic operations at every checkpoint. The X64 DXE IPL equivalent and
  the coreboot payload VGA helper also no longer impose artificial delays.
- DXE Core logs the dispatcher return status and writes a VGA breadcrumb
  before dispatch. Its VGA checkpoint no longer sleeps for 500 ms.
- The DXE dispatcher pairs begin/end SOL markers around driver image load,
  firmware-volume image processing, driver entry/return and dispatch event
  signaling. It reports its final status and clears the running flag if event
  creation fails.
- `BlSupportPei` checks memory-map parsing and the minimum low-memory bound
  before calculating the PEI allocation base. It reports the PEI memory
  installation status and returns on failure.
- The graphics driver reports framebuffer geometry, BAR match and final
  status. The marker prefix is `[GX-GOP]`; dispatcher messages use
  `[GX-DISPATCH]`.
- The coreboot Gooxi path emits BIOS error-level `[GX-FSP]` begin/return
  markers around FSP-M and FSP-S. This level is visible with the board's
  current `BIOS_ERR` console threshold. The coreboot payload VGA helper no
  longer adds a half-second pause at every payload checkpoint.

These changes improve the next capture and remove real diagnostic stalls.
They are not proof that the DXE menu works on hardware. No FSP or flash-layout
change is justified by the available DXE evidence. FSP-M did initialize enough
memory for the old image to enter DXE; that does not validate the provisional
G4DEL PCIe IIO map.

## Dasharo review

The local EDK2 tree already contains Dasharo's framebuffer-within-BAR change
([Dasharo/edk2 commit 40247f645759](https://github.com/Dasharo/edk2/commit/40247f645759)).
It is relevant if the BMC framebuffer does not begin at the PCI BAR base.
An upstream coreboot Sapphire Rapids change avoids a long FSP TempRamExit
([Dasharo/coreboot commit 65917a3f1329](https://github.com/Dasharo/coreboot/commit/65917a3f1329));
the current recordings already pass into DXE, so it is not the observed menu
blocker. The upstream serialized SMM relocation fix
([Dasharo/coreboot commit 0dbec00f670c](https://github.com/Dasharo/coreboot/commit/0dbec00f670c))
may matter for Emerald Rapids CPU stepping, which is unknown here. Neither
change should be copied without matching platform evidence.

## Next board capture

Record the exact flashed image hash, capture BMC SOL from reset through at
least two minutes after the last checkpoint, and record VGA simultaneously.
Export the complete BMC SOL text without truncating it. Search a copy with:

```sh
rg -n '\[GX-FSP\]|\[GX-DXE\]|\[GX-DISPATCH\]|\[GX-BDS\]|\[GX-XHCI\]|\[GX-USB\]|\[GX-GOP\]|\[GX-LOGO\]|\[GX-MENU\]|\[GX-SHELL\]|\[EDK2-DIAG\]' new-sol.log
```

Find the last paired `begin`/`end` or `enter`/`returned` marker. The sequence
should pass FTW PPI installation, `BlSupportPei`, DXE IPL, DXE Core services,
DXE dispatch, BDS console connection, menu readiness, menu input, Shell start,
and a Shell command such as `map`. If a pair is incomplete, inspect that call
and its inputs before changing unrelated FSP, flash or IIO settings. Menu and
Shell hardware operation remain unverified.

## USB, input, and splash follow-up

The current FDF and generated DXE FV contain `XhciDxe`, `UsbBusDxe`,
`UsbKbDxe`, and `UsbMassStorageDxe`. This verifies image inclusion only.
The DXE debug library sends `DEBUG_ERROR` messages through the status-code
router; `PcdStatusCodeUseSerial=TRUE` and `StatusCodeHandlerRuntimeDxe` send
them to the 16550 serial port configured at `0x2f8`. The runtime handler must
itself dispatch before USB messages can appear, so check its
`[GX-DISPATCH]` entry/return if later USB markers are unexpectedly absent.
`PlatformBootManagerAfterConsole()` calls `EfiBootManagerConnectAll()` when
Fast Boot is off, then the USB drivers bind to controllers and devices. The
default BDS `mFastBoot` value is false, but an existing firmware variable can
enable it. The SOL trace now says when ConnectAll starts and returns, when
Fast Boot skips it, how many USB host and device interfaces are present, and
each discovered device's vendor/product IDs. Additional `[GX-XHCI]` and
`[GX-USB]` markers bracket xHCI reset/start, USB bus start, port enumeration,
keyboard input installation, and mass-storage binding. Pair these with
`[GX-BDS] console connection complete` and `[GX-MENU] entry` to tell whether
USB keyboard discovery fails before the menu or whether the menu cannot read
an available input protocol. A serial console may still permit menu input
when USB keyboard input fails.

The built-in UEFI Shell includes `devices`, `drivers`, `pci`, and `map` commands.
`lsusb` is a Linux userspace command and is not included in this Shell build.
After reaching Shell, use `pci -b` to inspect the xHCI PCI function, `drivers
-b` and `devices -b` to see bound drivers and devices, and `map -r` followed
by `map -b` to check USB mass-storage file-system mappings. Compare their
output with the `[GX-USB]` VID/PID lines in SOL. A keyboard or USB stick
must be attached during the test; zero USB device interfaces without one is
not itself a failure.

The coreboot configuration currently has `CONFIG_BOOTSPLASH_IMAGE` off and
`CONFIG_EDK2_BOOTSPLASH_FILE` empty. The separate coreboot `BOOTSPLASH` FMAP
slot is therefore empty; the old coreboot missing-bootsplash warning is
expected. EDK2 nevertheless packages a 600×308 built-in BMP in `LogoDxe`.
`LogoDxe` first tries a CBMEM logo and falls back to that BMP, while
`PlatformBootManagerAfterConsole()` calls `BootLogoEnableLogo()` after console
connection. If that first draw fails, BDS retries it after its full device
connection pass, allowing a newly bound GOP to be used. The new `[GX-LOGO]`
lines report logo protocol installation,
CBMEM lookup, fallback selection, console GOP/UGA availability, and the final
display status. A successful status proves the draw path returned; the BMC
video recording must confirm pixels actually appear. A custom Gooxi splash
asset is not present in the workspace and is not yet configured. Showing the
built-in logo requires working GOP, HII and video output; adding a bitmap to
the coreboot FMAP alone would not establish that path.

The next image should be tested in this order:

1. Save the image hash and capture reset-to-menu SOL plus BMC video. Confirm
   PEI, DXE dispatcher, BDS, and console markers before interpreting USB or
   splash failures.
2. With Fast Boot disabled and a USB keyboard attached, require an xHCI
   `start returned`, USB bus `start returned`, enumeration VID/PID, keyboard
   `ready`, then menu input that changes the selected item.
3. Select Shell; require `[GX-SHELL] entry`, a prompt, `pci -b`, `drivers -b`,
   `devices -b`, and `map -r`. With a USB storage device attached, require a
   mass-storage return status and a corresponding mapping if the medium has a
   supported file system.
4. Require `[GX-LOGO] display returned status=Success` and visually confirm
   the built-in logo. If the status is `Unsupported`, use the preceding
   logo/GOP markers to determine which protocol or framebuffer step failed.
5. Save the complete trace and screen recording with the ROM hash. Fix the
   first failed or incomplete checkpoint pair, rebuild and repeat. Do not
   mark menu, Shell, USB, or splash hardware operation verified until observed.
