/** @file Shared bounded early-boot [GX] VGA breadcrumb writer. */
#ifndef GX_VGA_LIB_H_
#define GX_VGA_LIB_H_

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>

#define GX_VGA_TEXT_BASE     0xB8000
#define GX_VGA_TEXT_COLUMNS  80U
#define GX_VGA_TEXT_ROWS     25U

STATIC
VOID
GxVgaCheckpoint (
  IN UINTN       Row,
  IN CONST CHAR8 *Message
  )
{
  UINTN  Length;
  UINTN  Index;
  UINT16 *Text;

  if ((Message == NULL) || (Row >= GX_VGA_TEXT_ROWS)) {
    return;
  }
  Length = AsciiStrLen (Message);
  if (Length > GX_VGA_TEXT_COLUMNS) {
    Length = GX_VGA_TEXT_COLUMNS;
  }
  Text = (UINT16 *)(UINTN)GX_VGA_TEXT_BASE;
  for (Index = 0; Index < Length; Index++) {
    Text[(Row * GX_VGA_TEXT_COLUMNS) + Index] =
      (UINT16)(0x0F00U | (UINT8)Message[Index]);
  }

  DEBUG ((DEBUG_WARN, "%a\n", Message));
}

#endif
