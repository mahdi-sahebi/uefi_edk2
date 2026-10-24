# G4DEL EDK2 topic history

This local branch stack reconstructs the changes after `gooxie-g4del`
(`db93215cf1`). The source was the local `feature/dxe-log` tip
`5ccd003e38`; the original branch was not rewritten.

| Branch | Commit | Purpose |
| --- | --- | --- |
| `feature/g4del/01-serial-menu` | `4c629e7df6` | DXE serial routing and menu/Shell entry checkpoints |
| `feature/g4del/02-pei-memory-smmstore` | `f88af1f822` | PEI memory validation, SMMSTORE geometry, and HOB checks |
| `feature/g4del/03-ftw-dispatch` | `266be4336d` | FTW validation and removal of its debug stall; empty delayed-dispatch guard |
| `feature/g4del/04-dxe-tracing` | `eeeac7b00c` | SEC, PEI, DXE IPL, and DXE Core/dispatcher diagnostics |
| `feature/g4del/05-usb-graphics` | `8dc9b8e10d` | USB, console, graphics, logo, and menu interaction diagnostics |
| `feature/g4del/06-documentation` | `b2e41bb3eb` | Historical capture and bring-up documentation |

The tree at the documentation commit matches `5ccd003e38` byte for byte.
Files touched in more than one original commit were assigned to the earliest
applicable topic and then advanced to their final contents in a later topic.
The EDK2 build uses CRLF in several source files; run whitespace checks with
`git -c core.whitespace=cr-at-eol diff --check` to avoid false positives.

The current 08-02 coreboot image fails before host power. Its descriptor is
only a suspected cause; no newer EDK2 boot result exists. Source-level checks
cannot establish DXE, menu, USB, logo, or Shell operation on the G4DEL board.
