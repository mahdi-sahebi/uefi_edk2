# G4DEL USB inventory pool ownership

The 2026-11-02 SOL capture in `log/1405-08-11` shows repeated
`Pool.c(721)` and `MemoryAllocationLib.c(821)` assertions while BDS
enumerates USB interfaces. The same capture reaches the UEFI Shell,
launches Kali GRUB from USB, and ends at the Linux EFI stub message
`Loaded initrd from LINUX_EFI_INITRD_MEDIA_GUID device path`.

`UsbGetSupportedLanguages()` returns a language table maintained by the
USB bus driver. The G4DEL inventory code freed that table after reading
USB strings, so every device with a string descriptor could pass a
non-owned pointer to `FreePool()`. The fix retains the table for the bus
driver and frees only the string returned by `UsbGetStringDescriptor()`.

For the next hardware test, capture SOL from power-on through OS launch.
Confirm the pool assertions are absent, then retry both Kali boot paths.
The Kali stall may have another cause; this fix addresses the observed
firmware memory error without claiming to resolve OS booting.
