# Gooxi G4DEL: coreboot, EDK2, SOL and VGA bring-up

This document describes the current `feature/g4del-dxe-sol-menu` port, why its
diagnostics are arranged as they are, and what the available evidence actually
shows. It covers two repositories: the coreboot checkout at
`/home/uefi/workspace/test/uefi_coreboot` and this EDK2 checkout at
`payloads/external/edk2/workspace/mahdi-sahebi` beneath it. Source links below
point into those checkouts; line numbers refer to coreboot `79815f8497` and
EDK2 `7074d98411`. The [dated SOL investigation](1405_07_29.md) preserves the
original 231-line capture and its detailed timeline.

## Goal, platform and evidence boundary

The goal is to boot the Gooxi G4DEL with open-source coreboot and EDK2 far
enough to display an **interactive UEFI boot menu** and start the built-in UEFI
Shell from that menu. A successful build or a text breadcrumb on VGA does not
establish that goal. It requires a reset-to-menu SOL capture plus working menu
input and an interactive Shell prompt on the physical server.

The board owner reports 64 GiB RAM, four fans, CPUs, a BMC and SOL. The factory
`server.bin` supplied during the earlier investigation is 64 MiB. The G4DEL
stock image SHA-256 recorded then was
`db303ef4cefe79372f508f061d4dafe79cb5d98cfcb84127a7297b4c88100465`.
The G4DEL target is configured for **up to two Sapphire Rapids sockets**
([board Kconfig](../../../../../../src/mainboard/gooxi/g4del/Kconfig#L5),
[current configuration](../../../../../../.config#L149)); the number of
populated CPUs, the physical DIMM slot count, fan control details and board
specific PCIe routing have not been established. `CONFIG_DIMM_MAX=32` is a
configuration ceiling, not evidence that 32 slots are populated. Installed
DIMMs are reported from FSP information; empty slot reporting is left unset
([board README](../../../../../../src/mainboard/gooxi/g4del/README.md#L3)).
The BMC's SOL path is the debug transport; it is not the same thing as EDK2
console input being ready.

**Observed on hardware:** the only available capture,
`/home/uefi/workspace/log/1405-07-29/sol.log`, was recorded on 2026-10-21
using the earlier ASRock-target image. Coreboot loaded and entered EDK2; EDK2
entered SEC and PEI. The final line is `[GX-VGA] row=0 FTWa-3` inside
`FaultTolerantWritePei`. It contains no FTW PEIM return, DXE, BDS, menu or
Shell evidence. This capture predates removal of the FTW busy loops. The last
marker alone cannot establish a flash fault, because it preceded an artificial
delay. The two early ACPI timer assertions and several coreboot warnings did
not stop the observed payload handoff
([timeline and analysis](1405_07_29.md#source-and-observed-sequence)).

**Verified statically:** the corrected G4DEL EDK2 and integrated coreboot
images were built in the preceding investigation, with BootManagerMenuApp and
Shell packaged. The factory non-BIOS regions were then compared byte for byte
against `server.bin`. The corrected EDK2 payload SHA-256 was
`75be5343d16e644f09ca7fde185735fcad61af569ccd4733f1d52fed64e8cd73`
([build report](1405_07_29.md#verification-and-next-capture)). No post-fix
board boot is available. In particular, **DXE entry, menu input and Shell
launch remain unverified on hardware**. `server.bin` and the built ROM are not
present in this checkout now, so their previous byte comparison is a recorded
result, not a check repeated while writing this guide.

## Boot chain and source map

```text
Factory flash descriptor / ME / GbE / Device Exp1
                          |
                  coreboot bootblock
                          |  PCH decode + ASPEED SUART1, 0x2f8
                  coreboot romstage / FSP-M
                          |  memory and IIO setup, serial diagnostics
                  coreboot ramstage
                          |  CBFS payload load and coreboot table pointer
                  EDK2 SEC -> PEI dispatcher
                          |  SMMSTORE flash HOB, FTW PPI, platform/memory HOBs
                  DXE IPL -> DXE Core
                          |  DXE drivers and protocols
                  BDS -> console connection -> BootManagerMenuApp
                          |  keyboard / serial input selects UEFI Shell
                  Shell prompt
```

Coreboot's payload lookup and paired load markers are in
[`payload_load()`](../../../../../../src/lib/prog_loaders.c#L663). Segment
validation, loading and the coreboot-table argument are in
[`selfboot.c`](../../../../../../src/lib/selfboot.c#L245); the x86 handoff uses
[`protected_mode_call_1arg()`](../../../../../../src/arch/x86/boot.c#L23).
The old SOL log proves this handoff because it contains `payload loaded`,
`edk2_payload`, then EDK2's `SEC entry` and `SEC -> PEI` markers. The EDK2
entry and handoff prints are in
[`SecMain.c`](../DasharoPayloadPkg/SecCore/SecMain.c#L77).

### What changed from Dasharo

The two feature branches have many historical diagnostic commits. These are
the project-specific changes that explain the current boot path; they are not
claims that a particular commit has already passed a new board boot.

| Repository / commit | Change and source | What it addresses |
| --- | --- | --- |
| coreboot `9e3270ba26` | Introduced a BIOS/SMMSTORE flash layout; current version is [G4DEL `board.fmd`](../../../../../../src/mainboard/gooxi/g4del/board.fmd#L1). | Gave EDK2 persistent variable storage in a dedicated flash region. |
| coreboot `4040b2b5a9`, `757663494d` | Added payload load and handoff markers in [`prog_loaders.c`](../../../../../../src/lib/prog_loaders.c#L663) and [`selfboot.c`](../../../../../../src/lib/selfboot.c#L245). | Makes the coreboot-to-EDK2 boundary visible on SOL and VGA. |
| coreboot `493c958021`, `79815f8497` | Reused the captured SUART1 route and added the separate [Gooxi board target](../../../../../../src/mainboard/gooxi/g4del/bootblock.c#L17). | Replaces the ASRock board selection and one-socket assumption with a G4DEL target while preserving the known SOL port. |
| EDK2 `6c77766915` | Changed [`MemInfoCallback()`](../DasharoPayloadPkg/BlSupportPei/BlSupportPei.c#L503) to track the highest low-RAM end even when each fragment is smaller than PEI's requested 64 MiB. | Addresses the previously identified `UsableLowMemTop=0` arithmetic underflow on fragmented Xeon SP low memory. It does not independently prove that every available range is large enough for [`PeiServicesInstallPeiMemory()`](../DasharoPayloadPkg/BlSupportPei/BlSupportPei.c#L953). |
| EDK2 `d8efa42cd0`, `d05d3facf9` | Mirrored early VGA breadcrumbs to SOL, routed SEC and DXE Core diagnostics to the configured UART, and traced [DXE Core](../MdeModulePkg/Core/Dxe/DxeMain/DxeMain.c#L297), [BDS](../MdeModulePkg/Universal/BdsDxe/BdsEntry.c#L739), menu and Shell. | Gives each stage a distinct serial checkpoint, including the place a future DXE handoff could fail. |
| EDK2 `e71ebc7bf0` | Removed FTW's artificial busy loops, added validated lookups and paired status prints in [`FaultTolerantWritePei.c`](../MdeModulePkg/Universal/FaultTolerantWritePei/FaultTolerantWritePei.c#L231), and skipped the empty delayed-dispatch timer lookup in [`Dispatcher.c`](../MdeModulePkg/Core/Pei/Dispatcher/Dispatcher.c#L399). | Removes the delay immediately after the old capture's last marker and prevents the observed premature timer HOB assertions. |
| EDK2 `7074d98411` | Hardened [SMMSTORE geometry/HOB](../DasharoPayloadPkg/SmmStorePei/SmmStorePei.c#L249) and [BlSupport HOB allocations](../DasharoPayloadPkg/BlSupportPei/BlSupportPei.c#L1107), and guarded optional TPM calls in [the boot manager](../DasharoPayloadPkg/Library/PlatformBootManagerLib/PlatformBootManager.c#L1257). | Rejects malformed or missing metadata before use and permits the configured TPM-disabled payload build. |

The original Dasharo branches did not contain the combined board-specific
configuration and diagnostics above. The earlier capture already reached PEI,
so “Dasharo only reaches SEC” describes an earlier baseline, not the final
observed stopping point. The source changes explain how this branch is prepared
to progress toward DXE; only the next board log can show whether it does.

### Which changes can unblock DXE?

The earlier SEC-only work found a concrete PEI memory arithmetic failure mode:
skipping every small low-RAM fragment could leave `UsableLowMemTop` at zero,
and subtracting the PEI memory size underflowed. Commit `6c77766915` changed
the [memory callback](../DasharoPayloadPkg/BlSupportPei/BlSupportPei.c#L537)
that supplies the value used for
[`PeiMemBase`](../DasharoPayloadPkg/BlSupportPei/BlSupportPei.c#L953). The
2026-10-21 log proves the then-current firmware progressed beyond SEC into
PEI, but it does not isolate that commit as the sole reason.

The **last observed** obstacle is the old FTW delay after `FTWa-3`. Commit
`e71ebc7bf0` removes that loop and adds checked flash HOB/range operations
([current FTW](../MdeModulePkg/Universal/FaultTolerantWritePei/FaultTolerantWritePei.c#L231)).
Its empty-queue guard also eliminates a premature timer call
([dispatcher](../MdeModulePkg/Core/Pei/Dispatcher/Dispatcher.c#L399)); the
timer assertions in the old log were nonfatal. Commit `7074d98411` further
checks SMMSTORE geometry and allocations before dereference
([`SmmStorePei.c`](../DasharoPayloadPkg/SmmStorePei/SmmStorePei.c#L268),
[`BlSupportPei.c`](../DasharoPayloadPkg/BlSupportPei/BlSupportPei.c#L1171)).
The DXE serial changes make a successful handoff *observable*; they do not
cause PEI to complete. **No available log proves that the corrected firmware
has reached DXE.** The decisive new pair is FTW's completion-PPI status and
PEIM return, followed by `BlSupportPei complete`, `DXE IPL -> DXE`, and
`[GX-DXE] serial entry`. If any is absent, the preceding stage still needs
investigation.

### PEI to DXE: the critical sequence

The [PEI firmware volume](../DasharoPayloadPkg/DasharoPayloadPkg.fdf#L50)
lists `SmmStorePei`, `FaultTolerantWritePei` and `BlSupportPei` in that
dependency order. The old capture confirms the first two were entered in
that order. The following steps identify the signals needed to establish
the rest of the chain on a new boot.

1. SEC configures its serial backend and announces entry and PEI handoff in
   [`SecMain.c`](../DasharoPayloadPkg/SecCore/SecMain.c#L77). The PEI dispatcher
   brackets PEIM calls with `CHECKPOINT PEIM enter` and `returned` in
   [`Dispatcher.c`](../MdeModulePkg/Core/Pei/Dispatcher/Dispatcher.c#L2264).
2. [`SmmStorePei`](../DasharoPayloadPkg/SmmStorePei/SmmStorePei.c#L249)
   parses the coreboot SMMSTORE record, validates its address and block count,
   divides the 256 KiB into variable, FTW working and FTW spare areas, and
   publishes `gVariableFlashInfoHobGuid` with a `HOB ready` marker
   ([source](../DasharoPayloadPkg/SmmStorePei/SmmStorePei.c#L320)). With four
   64 KiB blocks this yields 64 KiB variables, 64 KiB working and 128 KiB
   spare; the code derives these values from the advertised geometry.
3. [`FaultTolerantWritePei`](../MdeModulePkg/Universal/FaultTolerantWritePei/FaultTolerantWritePei.c#L231)
   checks the flash HOB and version, retrieves working/spare ranges, checks
   bounds, validates the workspace or scans the spare area, then installs its
   completion PPI with a logged EFI status
   ([source](../MdeModulePkg/Universal/FaultTolerantWritePei/FaultTolerantWritePei.c#L363)).
   A missing HOB or failed lookup now returns an error before its data is used.
   The old `FTWa-3` marker preceded a long loop in the previous revision; the
   current code no longer has that FTW loop. The PEIM must still return
   successfully before later progress can be inferred.
4. When there are no pending PEI delayed-dispatch entries,
   [`DelayedDispatchDispatcher()`](../MdeModulePkg/Core/Pei/Dispatcher/Dispatcher.c#L399)
   returns before calling the timer library. This is why the two early
   `AcpiTimerLib.c(57)` assertions in the old capture should disappear. Their
   disappearance alone is not a DXE success signal.
5. [`BlSupportPei`](../DasharoPayloadPkg/BlSupportPei/BlSupportPei.c#L953)
   parses coreboot memory information and installs PEI memory. It later
   publishes graphics, SMMSTORE and ACPI information HOBs and prints its
   completion addresses
   ([`BlSupportPei.c`](../DasharoPayloadPkg/BlSupportPei/BlSupportPei.c#L1157),
   [completion](../DasharoPayloadPkg/BlSupportPei/BlSupportPei.c#L1522)).
   The completion marker proves the function finished only when it appears in
   a new capture. In the old capture, SMMSTORE and FTW were dispatched before
   this PEIM; the absence of its completion is expected there.
6. [`DxeIplEntry()`](../MdeModulePkg/Core/DxeIplPeim/DxeLoad.c#L299)
   installs the DXE IPL PPI; its load routine locates and loads DXE Core and
   prints `CHECKPOINT DXE IPL -> DXE` immediately before
   [`HandOffToDxeCore()`](../MdeModulePkg/Core/DxeIplPeim/DxeLoad.c#L949).
   DXE Core prints a raw serial entry and then service and dispatcher status
   markers in [`DxeMain.c`](../MdeModulePkg/Core/Dxe/DxeMain/DxeMain.c#L297).
   It must eventually print its BDS handoff
   ([source](../MdeModulePkg/Core/Dxe/DxeMain/DxeMain.c#L677)).
7. [`BdsEntry.c`](../MdeModulePkg/Universal/BdsDxe/BdsEntry.c#L963)
   brackets console connection and reports `ConIn`/`ConOut`. The platform boot
   manager [registers the Shell option](../DasharoPayloadPkg/Library/PlatformBootManagerLib/PlatformBootManager.c#L2182).
   [`BootManagerMenuApp`](../MdeModulePkg/Application/BootManagerMenuApp/BootManagerMenu.c#L1020)
   prints menu entry and `Boot menu ready options=N` after drawing, then reads
   keys through `ConIn`
   ([source](../MdeModulePkg/Application/BootManagerMenuApp/BootManagerMenu.c#L1117)).
   [`Shell.c`](../ShellPkg/Application/Shell/Shell.c#L362) prints `[GX-SHELL]
   entry` if the Shell image actually starts. The FDF includes
   [menu](../DasharoPayloadPkg/DasharoPayloadPkg.fdf#L119) and
   [built Shell](../DasharoPayloadPkg/DasharoPayloadPkg.fdf#L390); packaging
   alone does not prove either can accept input.

## SOL: wiring, build settings and runtime stages

**Coreboot side.** The G4DEL bootblock opens PCH LPC/eSPI I/O decode windows
for COM1 `0x3f8`, COM2 `0x2f8`, and the ASPEED Super I/O configuration ports
`0x2e/0x2f`; it enables ASPEED **SUART1** at `CONFIG_TTYS0_BASE`
([`bootblock.c`](../../../../../../src/mainboard/gooxi/g4del/bootblock.c#L17)).
The device tree maps that PnP UART at `0x2f8` with IRQ 3 and also declares
the BMC IPMI KCS device
([`devicetree.cb`](../../../../../../src/mainboard/gooxi/g4del/devicetree.cb#L3)).
The current coreboot configuration selects `CONFIG_TTYS0_BASE=0x2f8`,
`CONFIG_TTYS0_BAUD=115200`, serial console, bootblock and postcar console and
`CONFIG_CONSOLE_OVERRIDE_LOGLEVEL`
([UART configuration](../../../../../../.config#L651),
[console configuration](../../../../../../.config#L644)). The board's
[`get_console_loglevel()`](../../../../../../src/mainboard/gooxi/g4del/console.c#L5)
returns `BIOS_ERR`, so the `[GX]`/`[CB-DIAG]` error-level breadcrumbs remain
visible without depending on verbose messages. In romstage, FSP-M serial
diagnostics are enabled and pointed at the same base before memory training
([`romstage.c`](../../../../../../src/mainboard/gooxi/g4del/romstage.c#L7)).

**Build-time crossing.** Coreboot's external-payload makefile forwards the
board flag and UART base
([`Makefile.mk`](../../../../../../payloads/external/Makefile.mk#L200)).
The EDK2 wrapper sets `SOL_UART_BASE` and
`PcdSerialRegisterBase` from that coreboot base and enables serial terminal
settings when configured
([EDK2 wrapper Makefile](../../../Makefile#L46)). The debug build,
serial support, default serial redirection and built-in Shell are selected in
the [current `.config`](../../../../../../.config#L705). The EDK2 DSC default
is `0x3f8` and is **overridden** by the wrapper for G4DEL; both the SEC
compile-time base and PCD serial base therefore become `0x2f8`
([DSC definitions](../DasharoPayloadPkg/DasharoPayloadPkg.dsc#L42),
[compile flags](../DasharoPayloadPkg/DasharoPayloadPkg.dsc#L150),
[PCDs](../DasharoPayloadPkg/DasharoPayloadPkg.dsc#L598)).
The configuration also enables the second-UART, dual-serial-debug and SOL
string options ([`.config`](../../../../../../.config#L755)); the wrapper
passes their build flags and PCDs
([Makefile](../../../Makefile#L219),
[SOL string PCD](../../../Makefile#L372)). Those settings do not establish
which physical BMC channel will accept menu input.

**EDK2 runtime.** SEC calls
[`SecSerialPortConfigure()`](../DasharoPayloadPkg/SecCore/SecMain.c#L77)
before its first debug print. That library starts from the build-selected UART
base, then reads the coreboot table's `CB_TAG_SERIAL` record when valid, using
its base, stride and I/O-versus-MMIO type
([`SecSerialPortLib.c`](../DasharoPayloadPkg/Library/SecSerialPortLib/SecSerialPortLib.c#L12),
[parser](../DasharoPayloadPkg/Library/SecSerialPortLib/SecSerialPortLib.c#L74)).
Ordinary EDK2 modules use the 16550 serial library and the platform hook,
which reads coreboot serial information into PCDs
([DSC](../DasharoPayloadPkg/DasharoPayloadPkg.dsc#L288),
[`PlatformHookLib.c`](../DasharoPayloadPkg/Library/PlatformHookLib/PlatformHookLib.c#L41)).
Although coreboot enables EDK2 CBMEM logging, the DSC selects its CBMEM
serial library only for a release target; this debug build follows the 16550
branch ([DSC condition](../DasharoPayloadPkg/DasharoPayloadPkg.dsc#L288)).
DXE Core is special: it binds the direct serial debug and serial-port
libraries in the [DSC](../DasharoPayloadPkg/DasharoPayloadPkg.dsc#L443),
then emits `[GX-DXE] serial entry` directly before its normal debug prints
([`DxeMain.c`](../MdeModulePkg/Core/Dxe/DxeMain/DxeMain.c#L297)).
The BDS, menu and Shell modules also have direct serial debug bindings
([DSC](../DasharoPayloadPkg/DasharoPayloadPkg.dsc#L797)). SOL output from
these modules is a progress trace; the menu needs a successfully connected
`ConIn` to receive keystrokes.

For a new capture, start logging BMC SOL **before reset**, record the firmware
image revision/hash and keep logging through a menu selection and several
Shell commands such as `ver` and `map`. Search the capture for
`[CB-DIAG]`, `[GX]`, `[EDK2-DIAG]`, `[GX-VGA]`, `[GX-DXE]`, `[GX-BDS]`,
`[GX-MENU]` and `[GX-SHELL]`. Pair each `begin`/`entry` with its `complete`,
`returned`, or status line; if the trace stops, inspect the source for the
operation between the last pair. A marker emitted before a call proves entry,
not completion. The capture's first missing return should drive the next fix.

## VGA: text breadcrumbs and the eventual UEFI display

**Coreboot.** The G4DEL configuration enables VGA text framebuffer and native
VGA initialization, selects the ASPEED VGA driver, and does not run a VGA
option ROM ([`.config`](../../../../../../.config#L477)). The selected ASPEED
driver runs its device initialization, initializes VGA text mode when that
option is enabled, and writes a sample line
([`ast2050.c`](../../../../../../src/drivers/aspeed/ast2050/ast2050.c#L22)).
The Gooxi device tree does not itself describe the ASPEED PCI VGA device;
the driver binds by PCI ID when the device is enumerated. The generic payload
loader writes `[GX]` breadcrumbs as 16-bit character/attribute cells to
`0xB8000` and emits matching `printk(BIOS_ERR)` lines
([`prog_loaders.c`](../../../../../../src/lib/prog_loaders.c#L113),
[payload path](../../../../../../src/lib/prog_loaders.c#L663)). Its
`vga_cb_print()` also delays for 500 ms per call, so these debug writes can
slow payload loading. This delay is distinct from the removed EDK2 FTW loop.

**EDK2.** SEC's
[`edkii_vga.c`](../DasharoPayloadPkg/SecCore/edkii_vga.c#L20), PEI's
[`BlSupportPei.c`](../DasharoPayloadPkg/BlSupportPei/BlSupportPei.c#L40),
the PEI dispatcher, SMMSTORE, FTW and
[`DxeLoad.c`](../MdeModulePkg/Core/DxeIplPeim/DxeLoad.c#L34) use direct
`0xB8000` text rows for early checkpoints. Their mirror helpers read the row
back and print `[GX-VGA] row=...` at `DEBUG_ERROR` to SOL
([SEC helper](../DasharoPayloadPkg/SecCore/edkii_vga.c#L42),
[DXE IPL helper](../MdeModulePkg/Core/DxeIplPeim/DxeLoad.c#L78)). DXE Core
has its own text-cell checkpoint and `[GX-DXE]` serial print
([`DxeMain.c`](../MdeModulePkg/Core/Dxe/DxeMain/DxeMain.c#L13)).
The old log's `FTWa-*` lines are such VGA rows mirrored over SOL; they do not
prove that a monitor displayed the rows. Direct text writes assume a usable
legacy VGA text aperture. The interactive menu instead needs DXE graphics or
text console drivers, connected `ConOut`/`ConIn`, and input devices. The FDF
includes console platform/splitter, graphics console and terminal drivers
([source](../DasharoPayloadPkg/DasharoPayloadPkg.fdf#L190)). The current DSC
defaults `USE_PLATFORM_GOP=FALSE`, so it packages
[`GraphicsOutputDxe`](../DasharoPayloadPkg/DasharoPayloadPkg.fdf#L197)
instead of the conditional Intel GOP/VBT path
([DSC default](../DasharoPayloadPkg/DasharoPayloadPkg.dsc#L114)). These are
build selections, not evidence of a working graphics protocol on this board.
BDS logs console connection status
([`BdsEntry.c`](../MdeModulePkg/Universal/BdsDxe/BdsEntry.c#L984)). A new
board test must check both visible output and usable keyboard or redirected
serial input. If `[GX-MENU]` appears on SOL without a usable menu, inspect
`ConIn`/`ConOut`, graphics protocol availability and menu key handling first.

## 64 MiB flash map and factory-region preservation

The current [G4DEL `board.fmd`](../../../../../../src/mainboard/gooxi/g4del/board.fmd#L1)
describes `FLASH 64M`. Offsets below are byte offsets in the complete image;
the end column is exclusive. The stock-region boundaries are also encoded by
the [read-only inspection utility](../../../../../../util/g4del_stock.py#L13).

| Region | Start | End exclusive | Size | Input / role |
| --- | ---: | ---: | ---: | --- |
| Intel descriptor (`SI_DESC`) | `0x0000000` | `0x0001000` | 4 KiB | Factory `descriptor.bin` |
| GbE (`SI_GBE`) | `0x0001000` | `0x0003000` | 8 KiB | Factory `gbe.bin` |
| ME/CSE (`SI_ME`) | `0x0003000` | `0x2ff0000` | `0x2fed000` | Factory `me.bin` |
| Device Exp1 (`SI_PT`) | `0x2ff0000` | `0x3000000` | 64 KiB | Factory `device_exp1.bin` |
| BIOS (`SI_BIOS`) | `0x3000000` | `0x4000000` | 16 MiB | New coreboot CBFS, EDK2 payload and reserved regions |

Inside `SI_BIOS`, `board.fmd` reserves `SMMSTORE(PRESERVE) 256K`, a 1 MiB
`BOOTSPLASH(CBFS)` area, FMAP and coreboot CBFS
([source](../../../../../../src/mainboard/gooxi/g4del/board.fmd#L8)). The
SMMSTORE reservation is part of the BIOS address range, **not** a sixth Intel
descriptor region; inspect the assembled ROM's FMAP for its resolved offset.
`PRESERVE` describes how that area is handled by image update workflows; it
does not import the factory AMI variable store into the new EDK2 build.
SMMSTORE v2 and `0x40000` size are selected in
[`.config`](../../../../../../.config#L529). The old log's reported SPI
`SF size 0x8000000` (128 MiB) conflicts with the 64 MiB supplied image. The
[SPI probe log](../../../../../../src/drivers/spi/spi_flash.c#L559) now includes
bus, chip select, JEDEC ID and sector size; physical chip capacity still needs
checking on the board before changing the image size.

The older ASRock-target image preserved the ME bytes but changed descriptor
bytes and left GbE and Device Exp1 erased. The G4DEL port adds the missing
[Device Exp1 build input](../../../../../../src/southbridge/intel/common/firmware/Makefile.mk#L97)
alongside the descriptor, ME and GbE inputs. The current configuration keeps
the descriptor untouched and leaves descriptor unlock and ME-disable options
off ([`.config`](../../../../../../.config#L408)). The factory AMI executable
BIOS region is deliberately replaced by coreboot and EDK2. This corrects an
image-assembly gap; it does **not** establish that the previous FTW stop was
caused by the missing regions.

From the coreboot root, inspect a locally available stock image, extract only
the four non-BIOS inputs when preparing a build, and compare the resulting
image as follows:

```sh
util/ifdtool/ifdtool -p lbg -d /media/sf_shared/server.bin
python3 util/g4del_stock.py /media/sf_shared/server.bin
python3 util/g4del_stock.py /media/sf_shared/server.bin \
  --extract deps/blobs/mainboard/gooxi/g4del
python3 util/g4del_stock.py /media/sf_shared/server.bin \
  --compare build/coreboot.rom
build/cbfstool build/coreboot.rom layout
build/cbfstool build/coreboot.rom print
```

The first two commands only read the stock image. `--extract` writes local
factory blobs; `--compare` reads both images and fails if any non-BIOS region
differs ([utility](../../../../../../util/g4del_stock.py#L27)). Keep
`server.bin` and extracted factory blobs out of Git. The
[board README](../../../../../../src/mainboard/gooxi/g4del/README.md#L3)
documents the same build inputs.

## Board choices still to validate

The Gooxi board target selects 64 MiB ROM, two-socket support, serial console,
EDK2 debug/menu/Shell, IPMI KCS and Sapphire Rapids FSP
([Kconfig](../../../../../../src/mainboard/gooxi/g4del/Kconfig#L5)). Its
[`romstage.c`](../../../../../../src/mainboard/gooxi/g4del/romstage.c#L7)
uses the **Archer City two-socket IIO table as a provisional PCIe map**. It
does not copy the ASRock-specific GPIO or forced CXL settings. The target
uses Gooxi SMBIOS names and pins the exact EDK2 payload commit
([`.config`](../../../../../../.config#L139),
[payload revision](../../../../../../.config#L714)). Optional TPM support is
disabled for this build; the wrapper passes `TPM_ENABLE=FALSE`
([Makefile](../../../Makefile#L164)). That removes a missing optional
TPM from the menu path but does not prove TPM behavior on this board. The
factory flash map, IIO routing, video and console input must all be checked
with a new hardware run.

## Next reset-to-Shell acceptance test

1. Record the exact flashed ROM hash and preserve an unmodified factory ROM.
   Capture BMC SOL from reset at 115200 baud; record any local VGA or BMC
   remote-console output in parallel.
2. Confirm coreboot reaches the paired payload load/handoff markers, then SEC
   entry and SEC-to-PEI. Check that the EDK2 serial base used by SEC and DXE
   remains `0x2f8` and the output is readable.
3. Require `SMMSTORE flash-info HOB ready`, FTW completion-PPI success and the
   matching `PEIM returned` marker. Require `BlSupportPei complete` with valid
   HOB pointers, DXE IPL handoff, `[GX-DXE] serial entry`, DXE dispatcher
   completion and BDS handoff. The old `FTWa-*` busy-loop markers and early
   empty-queue timer assertions should be absent.
4. Require BDS `console connection complete` with useful `ConIn` and `ConOut`,
   `[GX-MENU] entry` and `Boot menu ready options=N`. Verify that input changes
   the selected option, that selecting **UEFI Shell** produces `[GX-SHELL]
   entry`, and that the Shell accepts commands. Check both SOL and display
   behavior where those outputs are expected.
5. If any stage fails, save the entire new log and update the dated report with
   the first missing return/status, the exact new image hash and the observed
   screen/input behavior. Do not infer a later stage from an earlier entry
   marker. The menu/Shell goal is met only after the interactive test passes.
