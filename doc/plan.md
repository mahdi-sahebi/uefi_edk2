# Port Working VGA and DXE Path from Coreboot EDK2

## Summary

Use `/home/uefi/workspace/coreboot/payloads/external/edk2/` as the reference and port its working VGA/framebuffer and DXE handoff behavior into this checkout.

The reference build uses a DEBUG EDK2 build, coreboot framebuffer information passed through PEI HOBs, early VGA output from `BlSupportPei`, `GraphicsOutputDxe` with `FrameBufferBltLib`, and the normal graphics-console/DXE/BDS stack.

## Implementation

- Port the reference `BlSupportPei` framebuffer/VGA initialization and ordered checkpoints.
- Preserve memory-map fixes only when validated; do not copy debug patches that suppress assertions or ignore errors.
- Ensure the graphics HOB contains a valid framebuffer base, size, resolution, scanline, pixel format, and HOB size.
- Port or reconcile `DasharoPayloadPkg/GraphicsOutputDxe` and validate framebuffer ranges before installing GOP.
- Include `DxeIpl`, `GraphicsOutputDxe`, `ConPlatformDxe`, `ConSplitterDxe`, `GraphicsConsoleDxe`, and BDS in the payload DSC/FDF.
- Match the reference DEBUG configuration and bootsplash conversion; add an external GOP driver only when its VBT and driver are available.
- Add stable `G4DELDBG` markers proving PEI graphics setup, DXE IPL, DXE Core handoff, GOP installation, graphics-console dispatch, and BDS entry.

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
