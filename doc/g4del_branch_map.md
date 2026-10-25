# G4DEL EDK2 topic history

The local topic stack is now based on `feature/dxe-log` at
`5ccd003e38`. The original `gooxie-g4del` branch is unchanged. The
serial/menu topic changes were already present in the base, so an empty marker
commit preserves that topic position. Later PEI, FTW, dispatcher,
graphics, and documentation commits retain their order. The USB/menu
diagnostics branch follows the integration tip.

| Branch | Rebased commit | Purpose |
| --- | --- | --- |
| `feature/g4del/01-serial-menu` | `21157d5f57` | DXE serial routing and menu/Shell entry checkpoints |
| `feature/g4del/02-pei-memory-smmstore` | `172c307942` | PEI memory validation, SMMSTORE geometry, and HOB checks |
| `feature/g4del/03-ftw-dispatch` | `3f5a35dc24` | FTW validation and removal of its debug stall; empty delayed-dispatch guard |
| `feature/g4del/04-dxe-tracing` | `590871895f` | SEC, PEI, DXE IPL, and DXE Core/dispatcher diagnostics |
| `feature/g4del/05-usb-graphics` | `e639a0bae5` | USB, console, graphics, logo, and menu interaction diagnostics |
| `feature/g4del/06-documentation` | `5bb35f3166` | Historical capture and bring-up documentation |
| `feature/g4del/integration` | `5bb35f3166` | Final documentation/integration tip |
| `feature/g4del/usb-graphics-menu` | this branch | USB inventory, graphics and Shell diagnostics |

Most earlier topic source changes are supplied by `feature/dxe-log`; the
rebased stack uses the serial/menu marker and retains its staged review points.
The EDK2 build uses CRLF in several source files; run whitespace checks with
`git -c core.whitespace=cr-at-eol diff --check` to avoid false positives.

The current 08-02 coreboot image fails before host power. Its descriptor is
only a suspected cause; no newer EDK2 boot result exists. Source-level checks
cannot establish DXE, menu, USB, logo, or Shell operation on the G4DEL board.
