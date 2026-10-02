# Port Working VGA and DXE Path from Coreboot EDK2

## Summary

Use `/home/uefi/workspace/coreboot/payloads/external/edk2/` as the reference and port its working VGA/framebuffer and DXE handoff behavior into this checkout.

The reference build uses a DEBUG EDK2 build, coreboot framebuffer information passed through PEI HOBs, early VGA output from `BlSupportPei`, `GraphicsOutputDxe` with `FrameBufferBltLib`, and the normal graphics-console/DXE/BDS stack.

The coreboot build invokes `payloads/external/edk2/`, whose build target is
`DasharoPayloadPkg`. At runtime, execution enters the DasharoPayloadPkg
SEC/PEI path first and then proceeds through the shared `MdeModulePkg` PEI,
DxeIpl, DXE Core, dispatcher, graphics, and BDS modules. Coreboot's
`payloads/external/Makefile.mk` is the component that invokes the EDK2
`DasharoPayloadPkg` build; DasharoPayloadPkg is the payload package and
MdeModulePkg supplies the subsequent shared implementation modules.

## Implementation

- Port the reference `BlSupportPei` framebuffer/VGA initialization and ordered checkpoints.
- Preserve memory-map fixes only when validated; do not copy debug patches that suppress assertions or ignore errors.
- Ensure the graphics HOB contains a valid framebuffer base, size, resolution, scanline, pixel format, and HOB size.
- Port or reconcile `DasharoPayloadPkg/GraphicsOutputDxe` and validate framebuffer ranges before installing GOP.
- Include `DxeIpl`, `GraphicsOutputDxe`, `ConPlatformDxe`, `ConSplitterDxe`, `GraphicsConsoleDxe`, and BDS in the payload DSC/FDF.
- Match the reference DEBUG configuration and bootsplash conversion; add an external GOP driver only when its VBT and driver are available.
- Add stable `G4DELDBG` markers proving PEI graphics setup, DXE IPL, DXE Core handoff, GOP installation, graphics-console dispatch, and BDS entry.

## Completed in this implementation

- Added the exact `[GX]` prefix to the customized serial breadcrumbs and VGA breadcrumbs.
- Reworked customized VGA writers to validate row, column, string length, and framebuffer offset before writing.
- Removed destructive VGA row clearing so earlier phase breadcrumbs remain visible.
- Corrected the IA32 DXE IPL formatter buffer mismatch.
- Added/retained tagged checkpoints covering SEC/PEI, FSP, DXE IPL/Core, dispatcher, graphics, PCI, SMM/FTW, and BDS paths.
- Added paired coreboot checkpoints for FSP completion, CBFS payload lookup/load, HOB validation, and EDK2 payload handoff.
- Added a shared bounded EDK2 early-VGA checkpoint interface for the core execution phases.

The VGA writer currently uses the validated legacy `0xB8000` fallback. The framebuffer/GOP path remains the preferred runtime graphics path; a subsequent hardware pass should bind early breadcrumbs to the active framebuffer once its handoff contract is confirmed on the target.

Coreboot emits its `[GX]` markers before the original romstage/payload messages. This separates a failure before payload handoff from a failure inside EDK2.

## Validation status

- `git diff --check`: passes.
- Static source inspection: customized VGA writes are bounded and clear helpers no longer erase prior breadcrumbs.
- BaseTools build and test suite: passed after initializing the Brotli submodule.
- EDK2 DEBUG IA32/X64 payload build: passed with GCC5.
- Coreboot build: passed; the generated payload was copied into the coreboot build and embedded as `fallback/payload`.
- Artifact verification: the EDK2 FV, extracted CBFS payload, and coreboot build payload contain the expected `[GX]` module markers.
- Hardware SOL/VGA capture: still required; source and artifact verification cannot prove the physical console path.

## G4DEL payload execution and diagnostics update

- SEC now uses the serial DebugLib and the payload print mask is limited to `DEBUG_ERROR | DEBUG_WARN` (`0x80000002`).
- The classic UefiPayload path mirrors legacy I/O-UART diagnostics between COM1 and COM2 with bounded polling.
- VGA breadcrumbs emit matching warning-level serial messages and retain the B8000 text output.
- The x64 DXE handoff uses the validated stack, HOB, page-table, EndOfPei, and `SwitchStack()` sequence without artificial delays or raw HOB/entrypoint tracing.
- Coreboot invokes `payloads/external/edk2/`, builds `DasharoPayloadPkg`, and the runtime then enters DasharoPayloadPkg SEC/PEI before proceeding through shared `MdeModulePkg` PEI, DxeIpl, DXE Core, dispatcher, graphics, and BDS modules.

## Acceptance and Tests

- Build the local payload in DEBUG mode and verify the generated FV contains DxeIpl, GraphicsOutputDxe, GraphicsConsoleDxe, and BDS.
- Verify the generated image contains the configured logo/bootsplash.
- Flash and capture SOL from power-on.
- Require ordered markers for graphics HOB publication, DXE Core handoff, GOP installation, graphics-console dispatch, and BDS, plus visible VGA output and the existing SMM-store geometry marker.
- If VGA remains blank, validate the framebuffer base/size against coreboot, graphics HOB contents, GOP installation status, and GOP/VBT policy.
- Separately resolve the ROM-size mismatch and invalid firmware-volume warnings.

## Assumptions

- The external tree is behavioral reference material; its uncommitted patches are ported selectively.
- The target is the Gooxi G4DEL coreboot payload.
- Existing local user changes are preserved.
