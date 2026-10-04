# G4DEL EDK2 Payload VGA/SOL Diagnostic Report

## Scope and evidence

This report covers the classic Coreboot UEFI payload path for the SPC741D8/Sapphire Rapids G4DEL target. The supplied capture
`SOLHostCapture_Archive_1_01011970_08_10_13.log` was inspected before implementation and is empty (0 bytes). It therefore
contains no runtime checkpoint from which to identify the final hardware stopping point. The runtime conclusion remains pending
a non-empty SOL capture.

## Payload flow

Coreboot builds and embeds the payload through:

```text
payloads/external/edk2/
    -> DasharoPayloadPkg SEC/PEI entry path
    -> MdeModulePkg PEI Core and PEI dispatcher
    -> DxeIpl
    -> MdeModulePkg DXE Core and DXE dispatcher
    -> graphics and console initialization
    -> BDS
    -> UEFI Shell boot option/application
```

`DasharoPayloadPkg` owns the initial SEC/PEI payload entry and platform integration. `MdeModulePkg` supplies the shared PEI,
DxeIpl, DXE Core, dispatcher, and BDS implementation. Coreboot's external-payload Makefile selects and builds the
`DasharoPayloadPkg` target.

## Checkpoint map

The shared `GxVgaCheckpoint()` helper writes bounded text to VGA text memory at `0xB8000` and emits the same message through
`DEBUG_WARN`, which is visible when the payload debug mask is `0x80000002`.

| Stage | Module | Checkpoint |
|---|---|---|
| SEC | `DasharoPayloadPkg/SecCore` | payload entry and PEI handoff |
| PEI Core | `MdeModulePkg/Core/Pei/PeiMain` | PEI Core entry |
| PEI dispatcher | `MdeModulePkg/Core/Pei/Dispatcher` | dispatcher entry |
| FSP | `IntelFsp2WrapperPkg/FspmWrapperPeim`, `FspsWrapperPeim` | FSP memory/silicon entry |
| SMMSTORE/FTW | `DasharoPayloadPkg/SmmStorePei`, `MdeModulePkg/Universal/FaultTolerantWritePei` | storage entry |
| DxeIpl | `MdeModulePkg/Core/DxeIplPeim` | DxeIpl entry and DXE Core handoff |
| DXE Core | `MdeModulePkg/Core/Dxe/DxeMain` | DXE Core entry |
| DXE dispatcher | `MdeModulePkg/Core/Dxe/Dispatcher` | dispatcher entry |
| PCI/graphics | `MdeModulePkg/Bus/Pci/PciBusDxe`, `DasharoPayloadPkg/GraphicsOutputDxe` | PCI and GOP progress |
| BDS | `MdeModulePkg/Universal/BdsDxe`, platform boot manager | BDS and console progress |
| Shell | platform boot manager | Shell FV boot-option registration |

## Original diagnostic problem

The payload already sets `PcdDebugPrintErrorLevel` to `0x80000002`, enabling only `DEBUG_WARN` and `DEBUG_ERROR`. However,
many existing `[GX]` messages were emitted with `DEBUG_INFO`, so they were intentionally filtered before reaching SOL. The
standardized `[GX]` entry and handoff messages now use `DEBUG_WARN`; failures remain `DEBUG_ERROR`.

The current tree also contained several legacy per-module VGA writers (`edkii_vga_sprintf()` and numbered local variants).
Those writers explain why VGA breadcrumbs differed between modules and could overwrite one another. New progress checkpoints
use the bounded shared helper; legacy detailed dumps remain isolated for module-specific data until hardware validation confirms
they are no longer needed.

## Shell reachability

The Shell image is included by `DasharoPayloadPkg.dsc` and `DasharoPayloadPkg.fdf`. The platform boot manager registers the
Shell FV boot option, but Shell execution depends on DXE dispatcher completion, GOP/console installation, BDS policy, and the
boot option being selected. Because the supplied SOL capture is empty, this report cannot claim whether the failure is before
DxeIpl, inside DXE dispatch, during graphics/console setup, or in BDS. The new checkpoints distinguish those cases in the next
non-empty capture.

## Validation status

- Static source changes: implemented.
- Warning/error diagnostic mask: `0x80000002` retained.
- VGA text target: `0xB8000` shared helper retained.
- SOL runtime result: pending corrected hardware capture.
- Full DEBUG payload and SPC741D8 coreboot image build: to be run after source review.
