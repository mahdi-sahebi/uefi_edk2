# G4DEL graphics, splash, menu, and Shell status

**State as of 2026-10-25:** source has a linear framebuffer handoff, CBMEM
boot-logo path, and a G4DEL USB inventory page before the Boot Manager Menu.
The EDK2 logo width/height memory overwrite has been corrected. The current
hardware capture still stops during `CpuDxe`; GOP, visible screen, menu input,
USB enumeration, and Shell remain unverified on the board.

## What the 08-02 capture proves

The capture at `/home/uefi/workspace/log/1405-08-02/serial.log`
shows coreboot completing both FSP calls and handing off to EDK2. EDK2 completes
PEI dispatch and DXE IPL, enters DXE Core, and begins loading DXE drivers. The
last full driver checkpoint is entry into `CpuDxe`;
`1A1E4886-9517-440E-9FDE-3BE44CEE2136` has no corresponding return in that
capture. The log ends mid-line. It does not identify the failing instruction or
show that BDS, GOP, or the logo code ran.

There are two independently confirmed graphics/splash setup gaps in that
capture:

1. Coreboot logged `CBFS ERROR: 'logo.bmp' not found in 'BOOTSPLASH'`. The
   framebuffer choice in the captured config was VGA text. Coreboot only adds
   `LB_TAG_FRAMEBUFFER` for a linear framebuffer. Without that record,
   `ParseGfxInfo()` returns `RETURN_NOT_FOUND`, so Dasharo's `GraphicsOutputDxe`
   does not install its GOP driver.
2. The EDK2 `LogoDxe` had an out-of-bounds write in the CBMEM bitmap path. It
   passed pointers to 16-bit `EFI_IMAGE_INPUT.Width` and `.Height` fields as
   `UINTN *` output parameters. On x64 the BMP decoder writes 8 bytes through
   each pointer, overwriting adjacent structure fields, including `Bitmap`.
   The driver could therefore fail or corrupt state while loading the splash.

These graphics issues explain why the old image could not provide the expected
GOP splash/menu reliably. They do not explain or fix the earlier observed stop
inside `CpuDxe`; that stop occurs before the logo driver and BDS are reached.

## Coreboot graphics and logo handoff

### Framebuffer mode

The checked-in G4DEL defconfig explicitly selects
`GENERIC_LINEAR_FRAMEBUFFER`, and G4DEL Kconfig defaults to it when AST native
VGA initialization is available. A stale `.config` from older testing still
selected `VGA_TEXT_FRAMEBUFFER`; regenerate it with the G4DEL defconfig before
building. The dedicated defconfig is
[`configs/config.gooxi_g4del`](../../../../../../../configs/config.gooxi_g4del). The coreboot
source path is:

1. `src/device/Kconfig` selects a framebuffer mode; the G4DEL-specific default
   is near lines 451–456.
2. `src/drivers/aspeed/ast2050/ast2050.c` calls `ast_driver_load()` and then
   `ast_driver_framebuffer_init()` for the linear path. Failures are logged as
   `[GX-GFX]` and the returned status is checked.
3. `src/drivers/aspeed/common/ast_mode_corebootfb.c` reads EDID using the AST
   GPIO bit-banged I²C path. A failed read now starts from a zeroed EDID buffer,
   logs the status, and takes the existing headless/KVM 1024×768 fallback
   instead of decoding uninitialized stack bytes. The selected mode, mode-set
   result, framebuffer base, pitch, size, and bpp are logged.
4. The AST driver registers the physical framebuffer using
   `fb_new_framebuffer_info_from_edid()`. Coreboot emits the resulting
   `LB_TAG_FRAMEBUFFER` record in the coreboot tables.
5. `src/include/pc80/vga.h` makes legacy `vga_line_write()` a no-op when
   `VGA_TEXT_FRAMEBUFFER` is disabled. This avoids both a link failure and
   writes to the legacy `0xB8000` text aperture while AST is in linear mode.
   In this configuration coreboot diagnostics are on SOL; later EDK2 console
   output should use GOP.

The EDID read is deliberately best effort: headless operation is expected for
a server BMC/KVM path. If both EDID reads fail, the fallback mode is logged. A
successful coreboot mode set is not proof that EDK2 installed GOP; the next
check is the EDK2 HOB/GOP trace below.

### Logo file and CBMEM transport

`Documentation/coreboot_logo.bmp` is the current open-source coreboot bitmap
(250×200×32). It is a project fallback image, not the Gooxi factory artwork.
The Kconfig setting `BOOTSPLASH_REGION_LOGO_FILE` supplies the path. The
`branding_bootsplash_region` Make target puts it in the `BOOTSPLASH` FMAP
region as compressed CBFS file `logo.bmp`. The recipe first removes an existing
entry, so repeated builds do not fail with “already in ROM image.”

At CBMEM ready, `src/lib/coreboot_table.c`:

1. Finds `BOOTSPLASH/logo.bmp` and allocates a CBMEM entry identified by
   `CBMEM_ID_TIANOCORE_LOGO`.
2. Prepends an 8-byte `UINT64 size` header, matching
   `DasharoPayloadPkg/Include/Coreboot.h`'s `cb_bootlogo_header`.
3. Adds the CBMEM reference as `CB_TAG_LOGO` in the coreboot table. Allocation,
   missing-file, and successful-copy outcomes have `[GX-LOGO]` serial logs.

This is separate from coreboot's built-in `CONFIG_BOOTSPLASH_IMAGE`, which
would display an image in coreboot itself. The present design carries an image
for EDK2 and expects EDK2 to draw it after GOP is available.

## EDK2 GOP and splash path

`DasharoPayloadPkg/Library/CbParseLib/CbParseLib.c:ParseGfxInfo()` finds
`CB_TAG_FRAMEBUFFER`, validates the record size, nonzero base and geometry,
32-bpp pixel masks, pitch, and address-range arithmetic, then fills the
`EFI_PEI_GRAPHICS_INFO_HOB`. Invalid/missing records now produce explicit
`[GX-GOP]` serial messages. `BlSupportPei.c` checks the HOB allocation and
logs whether the graphics HOB was installed. The driver in
`DasharoPayloadPkg/GraphicsOutputDxe/GraphicsOutput.c` requires that HOB and
logs its driver-binding install status.

The logo code in `MdeModulePkg/Logo/Logo.c` asks `ParseBootLogo()` for the
CBMEM bitmap and converts it to a GOP BLT buffer. It now uses local `UINTN`
width/height outputs, bounds-checks them against `MAX_UINT16`, initializes
`EFI_IMAGE_INPUT.Flags`, and only then copies dimensions into the protocol's
16-bit fields. If the CBMEM logo is missing or invalid, the existing embedded
HII logo remains the fallback. `LogoDxe` emits `[GX-LOGO]` messages for CBMEM
lookup, bitmap decode, and fallback status.

Dasharo `PlatformBootManagerAfterConsole()` calls `BootLogoEnableLogo()`, logs
its status, runs `EfiBootManagerConnectAll()` when full connection is enabled,
and retries the logo after console/device connection if the first attempt
failed. This is the expected stage for the splash to be visible. It is not an
early bootblock or FSP splash.

### Source map

| Layer | Source location | Role |
| --- | --- | --- |
| Coreboot framebuffer choice | `src/device/Kconfig:451-456` | Selects generic linear AST framebuffer on G4DEL. |
| AST native VGA driver | `src/drivers/aspeed/ast2050/ast2050.c:22-59` | Runs AST driver setup and the linear framebuffer path. |
| AST EDID/mode setup | `src/drivers/aspeed/common/ast_mode_corebootfb.c:81-177, 190-249` | EDID/fallback, mode set, framebuffer registration, serial status. |
| No text aperture access in linear mode | `src/include/pc80/vga.h:30-40` | Replaces text breadcrumb writes with a no-op when text mode is disabled. |
| Logo Kconfig and CBFS insertion | `src/Kconfig:477-482`, `Makefile:336-353` | Chooses the file and installs it as `BOOTSPLASH/logo.bmp`. |
| CBFS-to-CBMEM copy | `src/lib/coreboot_table.c:430-475` | Adds the size header and exports the CBMEM logo reference. |
| Framebuffer parsing and validation | `DasharoPayloadPkg/Library/CbParseLib/CbParseLib.c:572-670` | Converts `LB_TAG_FRAMEBUFFER` to a validated EDK2 graphics-info structure. |
| PEI graphics HOB | `DasharoPayloadPkg/BlSupportPei/BlSupportPei.c:1106-1155` | Creates and reports the graphics HOB and optional PCI display HOB. |
| GOP driver entry | `DasharoPayloadPkg/GraphicsOutputDxe/GraphicsOutput.c:723-751` | Requires the graphics HOB and installs the GOP driver binding. |
| Logo decode and fixed dimension handling | `MdeModulePkg/Logo/Logo.c:52-132` | Reads the coreboot BMP, decodes to GOP BLT, and bounds-checks 16-bit image dimensions. |
| BDS logo and USB connection | `DasharoPayloadPkg/Library/PlatformBootManagerLib/PlatformBootManager.c:2077-2129` | Displays/retries logo and connects the device tree. |
| Shell boot option and menu key | `DasharoPayloadPkg/Library/PlatformBootManagerLib/PlatformBootManager.c:2241-2260` | Registers Shell and handles boot-menu configuration. |
| BDS and boot-option trace | `MdeModulePkg/Universal/BdsDxe/BdsEntry.c:438-441, 739, 963-1014, 1075, 1177` | Logs BDS entry, console connection, platform callbacks, menu return status, boot-option count, and boot-option launch/return. |
| Menu app entry | `MdeModulePkg/Application/BootManagerMenuApp/BootManagerMenu.c:1020` | Emits the `[GX-MENU] entry` serial marker. |
| Shell application entry | `ShellPkg/Application/Shell/Shell.c:362-372` | Emits `[GX-SHELL] entry` and the initial console-clear status. |
| Firmware module inclusion | `DasharoPayloadPkg/DasharoPayloadPkg.fdf:121, 158, 257-264, 392-395` | Includes Boot Manager Menu, LogoDxe, xHCI/USB drivers, and Shell. |

Useful markers in the next SOL capture:

```text
[GX-GFX] AST linear framebuffer initialization returned 0
[GX-GFX] framebuffer registered base=... size=... mode=... pitch=... bpp=32
[GX-LOGO] copied BOOTSPLASH/logo.bmp to CBMEM size=...
[GX-GOP] framebuffer record base=... mode=... pitch=... bpp=32 ...
[GX-GOP] graphics info HOB installed
[GX-GOP] driver binding installation status=Success
[GX-LOGO] CBMEM logo status=Success ...
[GX-LOGO] using CBMEM bitmap 250x200
[GX-LOGO] display returned status=Success
```

Each step must be observed. A logo in CBFS alone does not establish that the
coreboot framebuffer, EDK2 GOP, BDS console, or display path works.

## Menu, input, USB, and Shell

The selected G4DEL config builds the EDK2 payload with its boot manager,
`BootManagerMenuApp`, `LogoDxe`, `GraphicsOutputDxe`, `ConPlatformDxe`,
`ConSplitterDxe`, and `GraphicsConsoleDxe`. The FDF also includes the xHCI,
USB bus, USB keyboard, and USB mass-storage drivers. On the normal BDS path,
`PlatformBootManagerAfterConsole()` connects devices and registers the FV Shell
using `PcdShellFile`. The current config sets F12 (`EDK2_BOOT_MENU_KEY=0x0016`)
for `BootManagerMenuApp`, Escape (`EDK2_SETUP_MENU_KEY=0x0017`) for the Boot
Manager menu, and a 10-second boot prompt. The F12 option is registered when
the boot-menu variable is absent or enabled. The Shell is included in the
firmware volume and registered as a boot option; presence in the image does
not prove that it runs.

SOL output is useful even if the monitor stays black. For physical console
acceptance, require the GOP install marker, visible menu, and a key press that
changes the selection. Test both the BMC keyboard path and a directly attached
USB keyboard. Once Shell starts, run `map -r`, `devices -b`, `drivers -b`, and
`pci -b`; attach a USB storage device and confirm a filesystem mapping. UEFI
Shell does not provide the Unix `lsusb` command; use its device/driver and PCI
commands to inspect USB enumeration.

## Gooxi reference image and Dasharo research

The two supplied files
`/home/uefi/workspace/gooxie_bios/server.bin` and
`/home/uefi/workspace/gooxie_bios/uefi/server.bin` are byte-identical 64 MiB
images (SHA-256
`db303ef4cefe79372f508f061d4dafe79cb5d98cfcb84127a7297b4c88100465`). The
local vendor inspection identifies AMI Aptio V, Archer City/Eagle Stream, and
board code `G4DEL110`. The factory FSP-T/M/S firmware volumes are at
`0x03EC8000`, `0x03B90000`, and `0x03B30000`. Their FSP headers identify
`$SPR-SP$`, spec 2.3, header revision 6, and image revision 1.1.1 build 631;
the raw image-revision field is `0x01010177`. A report's title table says the
extracted blobs were wired into ASRock SPC741D8; that target label does not
change the raw Gooxi image inspection. Current coreboot uses the repository
Eagle Stream FSP package: `$SPR-SP$`, spec 2.3, header revision 6, image
revision 1.1.1 build 917 (raw field `0x01010195`). The 08-02 trace records
FSP-M and FSP-S success for the image tested then. The image revision and
hashes differ, but that alone does not show an incompatibility; the current
FSP input and component hashes are recorded in the coreboot inventory.

### G4DEL ROM layout and verified factory regions

The current `src/mainboard/gooxi/g4del/board.fmd` builds a 64 MiB image. The
layout and the just-built ROM's regions are:

| Region | Flash offset | Size | Use |
| --- | ---: | ---: | --- |
| `SI_DESC` | `0x00000000` | 4 KiB | Factory flash descriptor. |
| `SI_GBE` | `0x00001000` | 8 KiB | Factory GbE data. |
| `SI_ME` | `0x00003000` | `0x02fed000` | Factory ME region. |
| `SI_DEVICEEXT` | `0x02ff0000` | 64 KiB | Device Exp1/platform data. |
| `SI_BIOS` | `0x03000000` | 16 MiB | Rebuilt coreboot + EDK2 BIOS region. |
| `SMMSTORE` | `0x03000000` | 256 KiB | Separate SMMSTORE v2 variable-flash backing area; initially erased in this build. |
| `BOOTSPLASH` | `0x03040000` | 1 MiB | CBFS logo asset for EDK2. |
| `FMAP` | `0x03140000` | 2 KiB | Flash map metadata. |
| `COREBOOT` | `0x03140800` | remainder | coreboot stages, FSP and EDK2 payload. |

The current generated ROM was compared directly against the supplied
`server.bin` at the four factory ranges. Descriptor, GbE, ME, and Device Exp1
bytes all match byte for byte. The proprietary BIOS code and variable stores
from `server.bin` are not copied into the new `SI_BIOS` region. The SMMSTORE
area is a new, erased 256 KiB region for EDK2; it does not contain factory
variable values.

The factory image was parsed with UEFIExtract NE alpha 76. Its 3,691 BIOS
metadata rows contain no item named `Logo`, `Splash`, or `.bmp`; that does not
prove the vendor logo is absent because it could be stored in a raw or
compressed item. The report did not export proprietary payloads. The full
volume and nested-item tree is documented in the coreboot worktree at
`doc/g4del/factory_image_inventory.md` and `server_bios_inventory.csv`. The
project bitmap remains in use and is not factory artwork.

The public Dasharo platform version matrix reviewed for this task lists
supported Dasharo platforms but does not list Gooxi G4DEL. I found no
G4DEL-specific Dasharo release to apply directly. The reusable references are
the generic Dasharo [logo customization guide](https://docs.dasharo.com/guides/logo-customization/),
the [Dasharo coreboot source](https://github.com/Dasharo/coreboot), the
[Dasharo EDK2 source](https://github.com/Dasharo/edk2), and the current
[Dasharo platform version matrix](https://docs.dasharo.com/variants/versions/).
Coreboot's [architecture guide](https://doc.coreboot.org/getting_started/architecture.html)
also clarifies that graphics initialization is a ramstage responsibility and
EDK2 runs as the next payload. These references informed the logo region and
GOP handoff; they do not certify G4DEL hardware support.

## USB inventory, menu, and Shell

In the G4DEL diagnostic build, `PlatformBootManagerAfterConsole()` connects
the full driver tree, waits one second, logs USB2 host-controller capability
and state, and enumerates `EFI_USB_IO_PROTOCOL` handles. The serial record
contains device/config/interface descriptor status, VID/PID, class fields,
interface number, available manufacturer/product/serial strings, and device
path. The UEFI USB I/O protocol does not expose the negotiated speed of each
device; only each host controller's maximum supported speed is available.

The helper draws a ten-second inventory page using `gST->ConOut`; Up/Down and
Page Up/Page Down scroll, Enter/Esc continues, and timeout continues
automatically. BDS then requests the existing Boot Manager Menu, with the
built-in EDK2 Shell available as an option. The page appears on VGA only if
the graphics console is connected, and on SOL only if serial is an active
output console. Source presence and FDF module inclusion do not establish
runtime enumeration. The 08-02 log stops before BDS and contains no USB or
menu event.

UEFI Shell does not have Linux `lsusb`. Use `devices -b`, `drivers -b`,
`pci -b`, and `map -r`; with USB storage attached, confirm its filesystem
mapping and read a small file.

## Build evidence and remaining work

The 2026-10-25 static build produced `build/UEFIPAYLOAD.fd` (14,876,672
bytes, SHA-256
`87e3939fce12305e1a71bd4e945f544ca6b31939df2a9145cbe6514d196d9c76`) and
`build/coreboot.rom` (67,108,864 bytes, SHA-256
`7169a0d953b62239d50cf6f24aa185244876334b9c9d54f7becc1ed02a77fce3`).
`util/g4del_stock.py --compare` confirms byte-for-byte preservation of the
factory descriptor, GbE, ME, and Device Exp1 areas. `--verify-layout` and
`cbfstool layout -w` confirm all 64 MiB IFD/FMAP ranges, the four aligned
BIOS subregions, and the 256 KiB erased SMMSTORE.

`cbfstool print -r BOOTSPLASH` confirms `logo.bmp` is present as a 5,172-byte
LZMA entry with a 200,138-byte decompressed size. The payload manifest lists
BootManagerMenuApp, Shell, GraphicsOutputDxe, LogoDxe, xHCI, USB bus, USB
keyboard, USB mouse, and mass-storage modules. Payload strings also contain
the G4DEL inventory page and forced-menu diagnostics. These checks establish
packaging; they do not establish that the dispatcher starts the modules on
the board.

The source build uses coreboot commit
`6e4458e8660fd0d32d921f3cd95543f8afc75e05` and EDK2 commit
`d3f653673e6bc70177094b11c1ee1ecd353b0f2e`. The FSP input is
`3rdparty/fsp/EagleStreamFspBinPkg/Fsp.fd`, SHA-256
`776100021418dba4fb62446e7c420ce2bd161e849fc6d19c9f72b80a6b479e77`. Its
T/M/S components identify as `$SPR-SP$`, FSP spec 2.3, image revision
1.1.1 build 917, header revision 6. Component SHA-256 values are recorded in
the coreboot [factory image inventory](../../../../../../../doc/g4del/factory_image_inventory.md).

No flash/readback hash is associated with a board run. These are static build
results only.

The outstanding blocker is still the unpaired `CpuDxe` entry in the 08-02
hardware capture. A fresh reset-to-SOL capture must first show the new
`[GX-CPU]`/`[GX-MP]` markers and `CpuDxe` return. Continue from the next
unmatched DXE entry if it stops again. Only then can BDS, GOP, logo display,
USB input, menu input, and Shell execution be tested. Hardware acceptance
requires recording the flashed/readback ROM hash, visible splash/menu, a
working menu key, Shell launch, and a command response.

## External references

- [Gooxi SL201-G4 family manual](https://www.gooxi.us/uploadfiles/2024/01/20240122163352960.pdf) — family reference, not proof of every G4DEL detail.
- [UEFI USB protocols](https://uefi.org/specs/UEFI/2.11/17_Protocols_USB_Support.html)
- [UEFITool / UEFIExtract releases](https://github.com/LongSoft/UEFITool/releases)
- [Dasharo logo customization](https://docs.dasharo.com/guides/logo-customization/)
- [Dasharo platform version matrix](https://docs.dasharo.com/variants/versions/)
- [Dasharo coreboot](https://github.com/Dasharo/coreboot)
- [Dasharo EDK2](https://github.com/Dasharo/edk2)
- [coreboot architecture](https://doc.coreboot.org/getting_started/architecture.html)
