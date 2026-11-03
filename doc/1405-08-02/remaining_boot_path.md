# Remaining G4DEL boot path to the EDK2 menu and Shell

## Current measured stage

The 08-02 SOL capture completed PEI dispatch, DXE IPL, DXE Core initialization,
and several DXE drivers. Its final complete checkpoint is entry to
`CpuDxe` (`1A1E4886-9517-440E-9FDE-3BE44CEE2136`). It ends before a driver
return marker. The next acceptance checkpoint is a `CpuDxe` return, including
its CPU Arch and MP Services setup. The internal `[GX-CPU]` / `[GX-MP]`
checkpoints added after this capture should show which operation returns last.

The source now selects AST linear graphics, packages the coreboot bitmap for
EDK2, validates the framebuffer HOB, and fixes LogoDxe's width/height output
buffer overwrite. The rebuilt payload and 64 MiB ROM contain the graphics,
logo, CPU, menu, USB, and Shell modules/checkpoints. This confirms packaging;
the ROM has not been tested on the board. Hardware progress therefore remains
at the last stage actually seen in the 08-02 capture.

The current coreboot defconfig pins the EDK2 integration commit and explicitly
selects `GENERIC_LINEAR_FRAMEBUFFER`; older saved `.config` files still selected
VGA text mode until regenerated. `G4delUsbDiagnostics.c` connects the USB
driver tree and adds serial/VGA inventory source code. These changes do not
show that USB, GOP, or VGA has initialized on the board.

## Remaining states and evidence to collect

| Order | State to reach | Evidence required |
| --- | --- | --- |
| 1 | Finish `CpuDxe` initialization | `[GX-CPU]` return from MP support, processor counts within configured capacity, and `[EDK2-DIAG] CHECKPOINT DXE driver returned 1A1E4886-9517-440E-9FDE-3BE44CEE2136`. |
| 2 | Dispatch remaining DXE drivers | Complete entry/return markers for dependencies including variable services, SMMSTORE FVB, PCI, serial, graphics, console and USB drivers. Resolve the invalid FTW working/spare workspace separately and confirm variable services can read/write the intended store. |
| 3 | Complete DXE dispatch and establish consoles | Require DXE dispatcher completion and BDS entry, plus GOP install, connected ConOut/ConIn, and logo decode/display return. A framebuffer record or BMP in CBFS alone does not prove visible VGA output. |
| 4 | Connect USB and show inventory | Current G4DEL source calls `EfiBootManagerConnectAll()`, waits one second, logs host-controller state and each USB I/O device/interface, then draws a ten-second page through `gST->ConOut`. Verify device descriptors in SOL and visible text on VGA. `EFI_USB_IO_PROTOCOL` does not expose negotiated per-device link speed. |
| 5 | Enter the Boot Manager menu | After the USB page exits or times out, the G4DEL diagnostic build requests the existing Boot Manager Menu. Require `[GX-MENU] entry`, visible menu output, and a key press that changes the selection. F12 (`0x0016`) remains the BootManagerMenuApp hotkey where enabled. |
| 6 | Launch the built-in Shell | Select Shell from the menu and observe Shell entry plus a usable prompt. Run `map -r`, `devices -b`, `drivers -b`, and `pci -b`; verify USB storage mapping when a USB stick is attached. UEFI Shell has no Linux `lsusb` command. |

## Build evidence versus board evidence

The final build hashes and module checks are recorded in
[`analysis.md`](analysis.md) after rebuilding the corrected LogoDxe. They
identify build artifacts only, not a flashed/readback hash. No new board
capture or input test has been recorded.

## Reading the next failure

- If the log ends after a `[GX-CPU] ... begin` marker, inspect that operation first. For MP initialization, compare the reported CPU handoff HOB, configured maximum, detected processors, AP check-ins and timeout. EDK2 currently configures 64 logical processors while coreboot permits 255 CPUs; actual board population must decide whether that limit is wrong.
- If `CpuDxe` returns but later driver entry has no return, inspect that driver's entry path and its dependencies. The log has not yet established whether USB, GOP, console, or a variable driver is the next failure.
- If DXE dispatch completes but BDS or console markers are absent, debug the last BDS/platform boot-manager operation. Test display, serial input, and USB keyboard independently.
- If the menu displays but does not react, verify that ConIn has a connected serial or keyboard input protocol. If Shell starts but USB devices are absent, inspect xHCI/USB enumeration and the Shell's `devices`, `drivers`, `pci`, and `map` output.

## Acceptance criteria

The hardware goal is met only when a fresh reset-to-SOL capture, tied to a recorded ROM hash, shows CPU DXE return, DXE dispatch completion and BDS/menu entry; the monitor shows the menu; user input changes the selected item; Shell launches; and a Shell command returns output. Build success, module inclusion, and a successful CPU driver return alone do not establish interactive menu or Shell operation.
