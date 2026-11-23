/** @file
  DXE Core Main Entry Point

Copyright (c) 2006 - 2022, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "DxeMain.h"
#include <Library/SerialPortLib.h>
#include <Library/PrintLib.h>
#include <Protocol/GxDxeDiagnostics.h>
#include <Guid/GraphicsInfoHob.h>
#include <Guid/HobList.h>
#include <Guid/Acpi.h>
#include <Guid/GlobalVariable.h>
#include <Pi/PiHob.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/BlockIo.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/MpService.h>
#include <Library/G4delDiagnosticChecks.h>

#define GX_DXE_FB_COLUMNS       72
#define GX_DXE_FB_LOG_ROWS      24
#define GX_DXE_FB_PINNED_ROWS   4
#define GX_DXE_FB_CELL_WIDTH    12
#define GX_DXE_FB_CELL_HEIGHT   16
#define GX_DXE_LOG_BUFFER_SIZE  512

STATIC UINTN  mGxDxeScreenRow;
STATIC UINT32 mGxDxeSequence;
STATIC UINT32 *mGxDxeScreenBackup;
STATIC UINTN  mGxDxeScreenBackupWidth;
STATIC UINTN  mGxDxeScreenBackupRows;
STATIC UINTN  mGxDxeScreenBackupPitch;
STATIC UINT64 mGxDxeScreenBackupBase;
STATIC EFI_HANDLE mGxDxeImagePathHandle;
STATIC CHAR16 *mGxDxeImagePathText;
STATIC UINT64 mGxDxeElapsedMilliseconds;
STATIC UINT64 mGxDxeLastClockMilliseconds;
STATIC BOOLEAN mGxDxeClockStarted;
STATIC BOOLEAN mGxDxeClockFresh;
STATIC BOOLEAN mGxDxeShellLoadTraceActive;
STATIC EFI_HANDLE mGxDxeDiagnosticsHandle;
STATIC GX_DXE_DIAGNOSTICS_PROTOCOL mGxDxeDiagnosticsProtocol;
STATIC GX_DIAG_CHECK mGxDiagChecks[GX_DIAG_MAX_CHECKS];
STATIC UINTN mGxDiagCheckCount;
STATIC EFI_STATUS EFIAPI GxDxeDiagnosticsRunChecks (IN CONST CHAR8 *Category);
STATIC EFI_STATUS EFIAPI GxDxeDiagnosticsGetCheck (IN UINTN Index, OUT GX_DIAG_CHECK *Check);
STATIC EFI_STATUS EFIAPI GxDxeDiagnosticsGetSummary (OUT UINTN *Count, OUT UINTN *Failures, OUT UINTN *Warnings);
STATIC CONST EFI_GUID  mGxGraphicsInfoHobGuid = EFI_PEI_GRAPHICS_INFO_HOB_GUID;

STATIC
BOOLEAN
GxDxeGetClockMilliseconds (
  OUT UINT64  *ClockMilliseconds
  )
{
  EFI_TIME  Time;
  UINTN     Month;
  UINT64    Year;
  UINT64    Days;
  UINT64    YearMinusOne;
  BOOLEAN   LeapYear;
  UINT8     MaximumDay;
  STATIC CONST UINT8  DaysPerMonth[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

  if ((gDxeCoreST == NULL) || (gDxeCoreST->RuntimeServices == NULL) ||
      EFI_ERROR (gDxeCoreST->RuntimeServices->GetTime (&Time, NULL)) ||
      (Time.Year < 1900) || (Time.Month < 1) || (Time.Month > 12) ||
      (Time.Day < 1) || (Time.Day > 31) || (Time.Hour > 23) ||
      (Time.Minute > 59) || (Time.Second > 59))
  {
    return FALSE;
  }

  Year        = Time.Year;
  YearMinusOne = Year - 1;
  LeapYear    = (BOOLEAN)(((Year % 4) == 0) && (((Year % 100) != 0) || ((Year % 400) == 0)));
  MaximumDay  = DaysPerMonth[Time.Month - 1];
  if (LeapYear && (Time.Month == 2)) {
    MaximumDay++;
  }

  if ((Time.Day > MaximumDay) || (Time.Nanosecond >= 1000000000)) {
    return FALSE;
  }

  Days        = (YearMinusOne * 365) + (YearMinusOne / 4) - (YearMinusOne / 100) + (YearMinusOne / 400);
  for (Month = 1; Month < Time.Month; Month++) {
    Days += DaysPerMonth[Month - 1];
  }

  if (LeapYear && (Time.Month > 2)) {
    Days++;
  }

  Days += Time.Day - 1;
  *ClockMilliseconds = (((((Days * 24) + Time.Hour) * 60 + Time.Minute) * 60) + Time.Second) * 1000;
  *ClockMilliseconds += Time.Nanosecond / 1000000;
  return TRUE;
}

STATIC
UINT64
GxDxeUpdateElapsedMilliseconds (
  VOID
  )
{
  UINT64  ClockMilliseconds;

  if (gEfiCurrentTpl != TPL_APPLICATION) {
    mGxDxeClockFresh = FALSE;
    return mGxDxeElapsedMilliseconds;
  }

  if (GxDxeGetClockMilliseconds (&ClockMilliseconds)) {
    mGxDxeClockFresh = TRUE;
    if (!mGxDxeClockStarted) {
      mGxDxeClockStarted = TRUE;
    } else if (ClockMilliseconds >= mGxDxeLastClockMilliseconds) {
      mGxDxeElapsedMilliseconds += ClockMilliseconds - mGxDxeLastClockMilliseconds;
    }

    mGxDxeLastClockMilliseconds = ClockMilliseconds;
  } else {
    mGxDxeClockFresh = FALSE;
  }

  return mGxDxeElapsedMilliseconds;
}

STATIC
UINT8
GxDxeGlyphColumn (
  IN CHAR8  Character,
  IN UINTN  Column
  )
{
  // 5x7 ASCII font, stored as one 7-bit column per entry.
  STATIC CONST UINT8  Font[][5] = {
    { 0x3E, 0x51, 0x49, 0x45, 0x3E }, // 0
    { 0x00, 0x42, 0x7F, 0x40, 0x00 }, // 1
    { 0x42, 0x61, 0x51, 0x49, 0x46 }, // 2
    { 0x21, 0x41, 0x45, 0x4B, 0x31 }, // 3
    { 0x18, 0x14, 0x12, 0x7F, 0x10 }, // 4
    { 0x27, 0x45, 0x45, 0x45, 0x39 }, // 5
    { 0x3C, 0x4A, 0x49, 0x49, 0x30 }, // 6
    { 0x01, 0x71, 0x09, 0x05, 0x03 }, // 7
    { 0x36, 0x49, 0x49, 0x49, 0x36 }, // 8
    { 0x06, 0x49, 0x49, 0x29, 0x1E }, // 9
    { 0x7E, 0x11, 0x11, 0x11, 0x7E }, // A
    { 0x7F, 0x49, 0x49, 0x49, 0x36 }, // B
    { 0x3E, 0x41, 0x41, 0x41, 0x22 }, // C
    { 0x7F, 0x41, 0x41, 0x22, 0x1C }, // D
    { 0x7F, 0x49, 0x49, 0x49, 0x41 }, // E
    { 0x7F, 0x09, 0x09, 0x09, 0x01 }, // F
    { 0x3E, 0x41, 0x49, 0x49, 0x7A }, // G
    { 0x7F, 0x08, 0x08, 0x08, 0x7F }, // H
    { 0x00, 0x41, 0x7F, 0x41, 0x00 }, // I
    { 0x20, 0x40, 0x41, 0x3F, 0x01 }, // J
    { 0x7F, 0x08, 0x14, 0x22, 0x41 }, // K
    { 0x7F, 0x40, 0x40, 0x40, 0x40 }, // L
    { 0x7F, 0x02, 0x0C, 0x02, 0x7F }, // M
    { 0x7F, 0x04, 0x08, 0x10, 0x7F }, // N
    { 0x3E, 0x41, 0x41, 0x41, 0x3E }, // O
    { 0x7F, 0x09, 0x09, 0x09, 0x06 }, // P
    { 0x3E, 0x41, 0x51, 0x21, 0x5E }, // Q
    { 0x7F, 0x09, 0x19, 0x29, 0x46 }, // R
    { 0x46, 0x49, 0x49, 0x49, 0x31 }, // S
    { 0x01, 0x01, 0x7F, 0x01, 0x01 }, // T
    { 0x3F, 0x40, 0x40, 0x40, 0x3F }, // U
    { 0x1F, 0x20, 0x40, 0x20, 0x1F }, // V
    { 0x3F, 0x40, 0x38, 0x40, 0x3F }, // W
    { 0x63, 0x14, 0x08, 0x14, 0x63 }, // X
    { 0x07, 0x08, 0x70, 0x08, 0x07 }, // Y
    { 0x61, 0x51, 0x49, 0x45, 0x43 }, // Z
    { 0x08, 0x08, 0x08, 0x08, 0x08 }, // -
    { 0x00, 0x36, 0x36, 0x00, 0x00 }, // :
    { 0x00, 0x60, 0x60, 0x00, 0x00 }, // .
    { 0x00, 0x41, 0x22, 0x14, 0x08 }, // /
    { 0x00, 0x00, 0x00, 0x00, 0x00 }, // space
      { 0x02, 0x01, 0x51, 0x09, 0x06 }, // ?
      { 0x00, 0x08, 0x08, 0x08, 0x00 }, // _
      { 0x14, 0x14, 0x14, 0x14, 0x14 }, // =
      { 0x7F, 0x41, 0x41, 0x41, 0x00 }, // [
      { 0x00, 0x41, 0x41, 0x41, 0x7F }, // ]
      { 0x00, 0x60, 0x30, 0x00, 0x00 }, // ,
      { 0x08, 0x08, 0x3E, 0x08, 0x08 }, // +
      { 0x62, 0x64, 0x08, 0x13, 0x23 }, // %
      { 0x3E, 0x41, 0x5D, 0x55, 0x1E }, // @
      { 0x00, 0x1C, 0x22, 0x41, 0x00 }, // (
      { 0x00, 0x41, 0x22, 0x1C, 0x00 }, // )
      { 0x08, 0x08, 0x08, 0x08, 0x08 }, // |
      { 0x00, 0x03, 0x0C, 0x30, 0x40 }  // backslash
  };
  UINTN  Index;

  if (Column >= 5) {
    return 0;
  }

  if ((Character >= '0') && (Character <= '9')) {
    Index = Character - '0';
  } else if ((Character >= 'A') && (Character <= 'Z')) {
    Index = 10 + Character - 'A';
  } else {
    switch (Character) {
      case '-': Index = 36; break;
      case ':': Index = 37; break;
      case '.': Index = 38; break;
      case '/': Index = 39; break;
      case ' ': Index = 40; break;
      case '_': Index = 42; break;
      case '=': Index = 43; break;
      case '[': Index = 44; break;
      case ']': Index = 45; break;
      case ',': Index = 46; break;
      case '+': Index = 47; break;
      case '%': Index = 48; break;
      case '@': Index = 49; break;
      case '(': Index = 50; break;
      case ')': Index = 51; break;
      case '|': Index = 52; break;
      case '\\': Index = 53; break;
      default:  Index = 41; break;
    }
  }

  return Font[Index][Column];
}

STATIC
BOOLEAN
GxDxeSaveScreenLogBackground (
  IN EFI_PEI_GRAPHICS_INFO_HOB  *GraphicsInfo,
  IN UINTN                      ScreenRows
  )
{
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  *Mode;
  UINTN                                  RowBytes;
  UINTN                                  ScanLineBytes;
  UINTN                                  AvailableRows;
  UINTN                                  BackupBytes;
  UINTN                                  Row;
  UINTN                                  Column;
  UINT32                                 *Backup;
  volatile UINT32                        *FrameBuffer;

  if (mGxDxeScreenBackup != NULL) {
    return TRUE;
  }

  if (GraphicsInfo == NULL) {
    return FALSE;
  }

  Mode = &GraphicsInfo->GraphicsMode;
  if ((GraphicsInfo->FrameBufferBase == 0) || (Mode->HorizontalResolution == 0) ||
      (Mode->PixelsPerScanLine < Mode->HorizontalResolution) ||
      (Mode->HorizontalResolution > MAX_UINTN / sizeof (UINT32)) ||
      (Mode->PixelsPerScanLine > MAX_UINTN / sizeof (UINT32)))
  {
    return FALSE;
  }

  RowBytes       = Mode->HorizontalResolution * sizeof (UINT32);
  ScanLineBytes  = Mode->PixelsPerScanLine * sizeof (UINT32);
  AvailableRows  = GraphicsInfo->FrameBufferSize / ScanLineBytes;
  if (ScreenRows > AvailableRows) {
    ScreenRows = AvailableRows;
  }

  if ((ScreenRows == 0) || (ScreenRows > MAX_UINTN / RowBytes)) {
    return FALSE;
  }

  BackupBytes = ScreenRows * RowBytes;
  Backup      = NULL;
  if (EFI_ERROR (CoreAllocatePool (EfiBootServicesData, BackupBytes, (VOID **)&Backup))) {
    return FALSE;
  }

  FrameBuffer = (volatile UINT32 *)(UINTN)GraphicsInfo->FrameBufferBase;
  for (Row = 0; Row < ScreenRows; Row++) {
    for (Column = 0; Column < Mode->HorizontalResolution; Column++) {
      Backup[Row * Mode->HorizontalResolution + Column] =
        FrameBuffer[Row * Mode->PixelsPerScanLine + Column];
    }
  }

  mGxDxeScreenBackup      = Backup;
  mGxDxeScreenBackupWidth = Mode->HorizontalResolution;
  mGxDxeScreenBackupRows  = ScreenRows;
  mGxDxeScreenBackupPitch = Mode->PixelsPerScanLine;
  mGxDxeScreenBackupBase  = GraphicsInfo->FrameBufferBase;
  return TRUE;
}

STATIC
VOID
GxDxeRestoreScreenLogBackground (
  VOID
  )
{
  UINTN                   Row;
  UINTN                   Column;
  volatile UINT32         *FrameBuffer;

  if (mGxDxeScreenBackup == NULL) {
    return;
  }

  FrameBuffer = (volatile UINT32 *)(UINTN)mGxDxeScreenBackupBase;
  for (Row = 0; Row < mGxDxeScreenBackupRows; Row++) {
    for (Column = 0; Column < mGxDxeScreenBackupWidth; Column++) {
      FrameBuffer[Row * mGxDxeScreenBackupPitch + Column] =
        mGxDxeScreenBackup[Row * mGxDxeScreenBackupWidth + Column];
    }
  }
}

STATIC
VOID
GxDxeScreenLog (
  IN CONST CHAR8  *Message
  )
{
  VOID                              *Hob;
  EFI_PEI_GRAPHICS_INFO_HOB         *GraphicsInfo;
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  *Mode;
  volatile UINT32                   *FrameBuffer;
  UINTN                             Row;
  UINTN                             CharacterIndex;
  UINTN                             X;
  UINTN                             Y;
  UINTN                             GlyphX;
  UINTN                             GlyphY;
  UINTN                             Scale;
  UINTN                             MessageOffset;
  UINTN                             ScreenRows;
  UINTN                             PinnedRows;
  UINTN                             RingRows;
  UINTN                             PinnedRow;
  BOOLEAN                           Pinned;
  UINT32                            Pixel;
  UINT32                            Black;
  UINT32                            White;

  if (Message == NULL) {
    return;
  }

  Hob = GetFirstGuidHob (&mGxGraphicsInfoHobGuid);
  if ((Hob == NULL) || (GET_GUID_HOB_DATA_SIZE (Hob) < sizeof (*GraphicsInfo))) {
    return;
  }

  GraphicsInfo = (EFI_PEI_GRAPHICS_INFO_HOB *)GET_GUID_HOB_DATA (Hob);
  Mode = &GraphicsInfo->GraphicsMode;
  if ((GraphicsInfo->FrameBufferBase == 0) || (Mode->HorizontalResolution < 640) ||
      (Mode->VerticalResolution < GX_DXE_FB_CELL_HEIGHT) || (Mode->PixelsPerScanLine < Mode->HorizontalResolution))
  {
    return;
  }

  Scale = 2;
  ScreenRows = Mode->VerticalResolution / GX_DXE_FB_CELL_HEIGHT;
  if (ScreenRows > GX_DXE_FB_LOG_ROWS) {
    ScreenRows = GX_DXE_FB_LOG_ROWS;
  }

  Pinned = FALSE;
  PinnedRow = 0;
  if (AsciiStrStr (Message, "SHELL EXEC BEGIN") != NULL) {
    Pinned = TRUE;
    PinnedRow = 0;
  } else if (AsciiStrStr (Message, "SHELL EXEC RETURN") != NULL) {
    Pinned = TRUE;
    PinnedRow = 0;
  } else if ((AsciiStrStr (Message, "SHELL LOAD_IMAGE") != NULL) ||
             (AsciiStrStr (Message, "LOAD BEGIN") != NULL) ||
             (AsciiStrStr (Message, "LOAD FILEPATH") != NULL) ||
             (AsciiStrStr (Message, "LOAD PARENT_PATH") != NULL) ||
             (AsciiStrStr (Message, "LOAD RESOLVE") != NULL) ||
             (AsciiStrStr (Message, "LOAD FILE_READ") != NULL) ||
             (AsciiStrStr (Message, "LOAD AUTH") != NULL) ||
             (AsciiStrStr (Message, "LOAD PE") != NULL) ||
             (AsciiStrStr (Message, "LOAD DONE") != NULL))
  {
    Pinned = TRUE;
    PinnedRow = 1;
  } else if ((AsciiStrStr (Message, "OS START BEGIN") != NULL) ||
             (AsciiStrStr (Message, "OS START PARENT_PATH") != NULL)) {
    Pinned = TRUE;
    PinnedRow = 2;
  } else if (AsciiStrStr (Message, "OS EBS ") != NULL) {
    Pinned = TRUE;
    PinnedRow = 3;
  }

  PinnedRows = (ScreenRows >= (GX_DXE_FB_PINNED_ROWS + 4)) ? GX_DXE_FB_PINNED_ROWS : 0;
  RingRows = ScreenRows - PinnedRows;
  if (Pinned && (PinnedRows == 0)) {
    Pinned = FALSE;
  }

  FrameBuffer = (volatile UINT32 *)(UINTN)GraphicsInfo->FrameBufferBase;
  Black = 0;
  White = 0x00FFFFFF;
  if (Mode->PixelFormat == PixelRedGreenBlueReserved8BitPerColor) {
    White = 0x00FFFFFF;
  } else if (Mode->PixelFormat != PixelBlueGreenRedReserved8BitPerColor) {
    return;
  }

  // Keep the framebuffer untouched until its original pixels are saved.
  // DXE diagnostics run before the DXE memory services are ready, so the
  // backup allocation can fail on early calls. Drawing anyway would make a
  // later successful backup preserve our own diagnostic overlay.
  if (!GxDxeSaveScreenLogBackground (GraphicsInfo, ScreenRows)) {
    return;
  }

  for (MessageOffset = 0; Message[MessageOffset] != '\0'; MessageOffset += GX_DXE_FB_COLUMNS) {
    if (Pinned) {
      Row = PinnedRow;
    } else {
      Row = PinnedRows + (mGxDxeScreenRow++ % RingRows);
    }

    Y = Row * GX_DXE_FB_CELL_HEIGHT;
    for (GlyphY = 0; GlyphY < GX_DXE_FB_CELL_HEIGHT; GlyphY++) {
      for (X = 0; X < Mode->HorizontalResolution; X++) {
        if ((Y + GlyphY < Mode->VerticalResolution) &&
            (((Y + GlyphY) * Mode->PixelsPerScanLine + X) * sizeof (UINT32) < GraphicsInfo->FrameBufferSize))
        {
          FrameBuffer[(Y + GlyphY) * Mode->PixelsPerScanLine + X] = Black;
        }
      }
    }

    for (CharacterIndex = 0; (Message[MessageOffset + CharacterIndex] != '\0') &&
         (CharacterIndex < GX_DXE_FB_COLUMNS); CharacterIndex++)
    {
      UINT8  Character;

      Character = Message[MessageOffset + CharacterIndex];
      if ((Character >= 'a') && (Character <= 'z')) {
        Character = (CHAR8)(Character - 'a' + 'A');
      }
      X = CharacterIndex * GX_DXE_FB_CELL_WIDTH;
      for (GlyphY = 0; GlyphY < 14; GlyphY++) {
        for (GlyphX = 0; GlyphX < GX_DXE_FB_CELL_WIDTH; GlyphX++) {
          Pixel = Black;
          if ((GlyphX < 10) && (GlyphY < 14) &&
              ((GxDxeGlyphColumn (Character, GlyphX / Scale) & (1U << (GlyphY / Scale))) != 0))
          {
            Pixel = White;
          }
          if ((X + GlyphX < Mode->HorizontalResolution) && (Y + GlyphY < Mode->VerticalResolution) &&
              (((Y + GlyphY) * Mode->PixelsPerScanLine + X + GlyphX) * sizeof (UINT32) < GraphicsInfo->FrameBufferSize))
          {
            FrameBuffer[(Y + GlyphY) * Mode->PixelsPerScanLine + X + GlyphX] = Pixel;
          }
        }
      }
    }

    if (Pinned) {
      break;
    }
  }
}

STATIC
VOID
GxDxeLogInternal (
  IN CONST CHAR8  *Message,
  IN BOOLEAN      ShowOnScreen
  )
{
  CHAR8   NumberedMessage[GX_DXE_LOG_BUFFER_SIZE];
  CHAR8   ElapsedText[32];
  UINT64  ElapsedMilliseconds;

  if (Message == NULL) {
    return;
  }

  mGxDxeSequence++;
  ElapsedMilliseconds = GxDxeUpdateElapsedMilliseconds ();
  if (!mGxDxeClockStarted) {
    AsciiSPrint (ElapsedText, sizeof (ElapsedText), "elapsed=unknown");
  } else if (mGxDxeClockFresh) {
    AsciiSPrint (ElapsedText, sizeof (ElapsedText), "elapsed=%lu ms", ElapsedMilliseconds);
  } else {
    AsciiSPrint (ElapsedText, sizeof (ElapsedText), "elapsed=%lu ms stale", ElapsedMilliseconds);
  }

  AsciiSPrint (NumberedMessage, sizeof (NumberedMessage), "[%04u %a] %a", mGxDxeSequence, ElapsedText, Message);
  DEBUG ((DEBUG_ERROR, "[GX-DXE][%04u][%a] %a\n", mGxDxeSequence, ElapsedText, Message));
  if (ShowOnScreen) {
    GxDxeScreenLog (NumberedMessage);
  }
}

VOID
GxDxeLog (
  IN CONST CHAR8  *Message
  )
{
  GxDxeLogInternal (Message, TRUE);
}

VOID
GxDxeLogSerialOnly (
  IN CONST CHAR8  *Message
  )
{
  GxDxeLogInternal (Message, FALSE);
}

// ExitBootServices tears down the memory map and later clears Boot Services.
// Keep its final checkpoints independent of DebugLib, RuntimeServices, and
// memory allocation so the diagnostic path remains usable at that boundary.
STATIC
VOID
GxDxeEbsCheckpoint (
  IN CONST CHAR8  *Message
  )
{
  if (Message == NULL) {
    return;
  }

  SerialPortWrite ((UINT8 *)"[GX-EBS] ", sizeof ("[GX-EBS] ") - 1);
  SerialPortWrite ((UINT8 *)Message, AsciiStrLen (Message));
  SerialPortWrite ((UINT8 *)"\r\n", sizeof ("\r\n") - 1);

  // The framebuffer logger has an existing, preallocated background buffer.
  // Do not allocate after CoreTerminateMemoryMap has accepted the map key.
  if (mGxDxeScreenBackup != NULL) {
    GxDxeScreenLog (Message);
  }
}

STATIC
VOID
GxDxeLogHobSerialOnly (
  IN CONST CHAR8  *Message
  )
{
  if (Message == NULL) {
    return;
  }

  SerialPortWrite ((UINT8 *)"[GX-HOB] ", sizeof ("[GX-HOB] ") - 1);
  SerialPortWrite ((UINT8 *)Message, AsciiStrLen (Message));
  SerialPortWrite ((UINT8 *)"\r\n", sizeof ("\r\n") - 1);
}

VOID
GxDxeLogUnicodeData (
  IN CONST CHAR8   *Label,
  IN CONST CHAR16  *Text,
  IN UINTN         TextSizeInBytes
  )
{
  CHAR8    Message[160];
  CHAR8    Summary[64];
  CHAR16   Character;
  UINTN    CharacterCount;
  UINTN    Offset;
  UINTN    Position;
  UINTN    SummaryPosition;
  BOOLEAN  Truncated;

  if ((Label == NULL) || (Text == NULL)) {
    AsciiSPrint (Summary, sizeof (Summary), "%a <none>", (Label != NULL) ? Label : "unicode");
    GxDxeLog (Summary);
    return;
  }

  CharacterCount = TextSizeInBytes / sizeof (CHAR16);
  AsciiSPrint (Summary, sizeof (Summary), "%a=", Label);
  SummaryPosition = AsciiStrLen (Summary);
  Truncated       = FALSE;
  for (Offset = 0; (Offset < CharacterCount) && (SummaryPosition < 42); Offset++) {
    CopyMem (&Character, &Text[Offset], sizeof (Character));
    if ((Character >= 0x20) && (Character <= 0x7e) && (Character != '\\')) {
      Summary[SummaryPosition++] = (CHAR8)Character;
      Summary[SummaryPosition]   = '\0';
    } else {
      Truncated = TRUE;
      break;
    }
  }

  if ((Offset < CharacterCount) || (TextSizeInBytes & 1) != 0) {
    Truncated = TRUE;
  }

  if (Truncated && (SummaryPosition < sizeof (Summary) - 4)) {
    CopyMem (&Summary[SummaryPosition], "...", sizeof ("...") );
  }

  GxDxeLog (Summary);

  for (Offset = 0; Offset < CharacterCount; ) {
    AsciiSPrint (Message, sizeof (Message), "%a offset=%lu: ", Label, (UINT64)Offset);
    Position = AsciiStrLen (Message);
    while (Offset < CharacterCount) {
      CopyMem (&Character, &Text[Offset], sizeof (Character));
      if ((Character >= 0x20) && (Character <= 0x7e) && (Character != '\\')) {
        if (Position + 2 >= sizeof (Message)) {
          break;
        }

        Message[Position++] = (CHAR8)Character;
        Message[Position]   = '\0';
        Offset++;
      } else if (Character == '\\') {
        if (Position + 3 >= sizeof (Message)) {
          break;
        }

        Message[Position++] = '\\';
        Message[Position++] = '\\';
        Message[Position]   = '\0';
        Offset++;
      } else {
        if (Position + 7 >= sizeof (Message)) {
          break;
        }

        AsciiSPrint (&Message[Position], sizeof (Message) - Position, "\\u%04x", Character);
        Position += 6;
        Offset++;
      }
    }

    GxDxeLogSerialOnly (Message);
  }

  if ((TextSizeInBytes & 1) != 0) {
    GxDxeLogSerialOnly ("unicode data has trailing byte");
  }
}

VOID
GxDxeSetCurrentImagePath (
  IN EFI_HANDLE  ImageHandle,
  IN CHAR16      *DevicePathText
  )
{
  if (mGxDxeImagePathText != NULL) {
    CoreFreePool (mGxDxeImagePathText);
  }

  mGxDxeImagePathHandle = ImageHandle;
  mGxDxeImagePathText   = DevicePathText;
}

VOID
GxDxeLogCurrentImagePath (
  IN EFI_HANDLE  ImageHandle
  )
{
  if ((mGxDxeImagePathHandle == ImageHandle) && (mGxDxeImagePathText != NULL)) {
    GxDxeLogUnicodeData ("OS EBS PATH", mGxDxeImagePathText, StrSize (mGxDxeImagePathText));
  } else {
    GxDxeLog ("OS EBS PATH <unavailable>");
  }
}

STATIC
VOID
EFIAPI
GxDxeDiagnosticsProtocolLog (
  IN CONST CHAR8  *Message
  )
{
  GxDxeLog (Message);
}

STATIC
EFI_STATUS
EFIAPI
GxDxeDiagnosticsProtocolSetShellLoadTrace (
  IN BOOLEAN  Active
  )
{
  mGxDxeShellLoadTraceActive = Active;
  return EFI_SUCCESS;
}

BOOLEAN
GxDxeIsShellLoadTraceActive (
  VOID
  )
{
  return mGxDxeShellLoadTraceActive;
}

STATIC
EFI_STATUS
EFIAPI
GxDxeDiagnosticsProtocolGetDriverStatus (
  IN  CONST EFI_GUID          *FileName,
  OUT GX_DXE_DRIVER_STATUS    *DriverStatus
  )
{
  return CoreGetDriverStatus (FileName, DriverStatus);
}

VOID
GxDxeCheckpoint (
  IN UINTN       Sequence,
  IN CONST CHAR8 *Message
  )
{
  (VOID)Sequence;
  GxDxeLog (Message);
}

STATIC
VOID
GxDxeTraceStatus (
  IN CONST CHAR8  *Stage,
  IN EFI_STATUS   Status
  )
{
  CHAR8  Message[GX_DXE_FB_COLUMNS + 1];

  AsciiSPrint (Message, sizeof (Message), "DXE %a STATUS=0x%lx", Stage, Status);
  GxDxeLog (Message);
}

STATIC
VOID
GxDxeTrace (
  IN CONST CHAR8  *Stage
  )
{
  CHAR8  Message[GX_DXE_FB_COLUMNS + 1];

  AsciiSPrint (Message, sizeof (Message), "DXE %a", Stage);
  GxDxeLog (Message);
}

//
// DXE Core Global Variables for Protocols from PEI
//
EFI_HANDLE  mDecompressHandle = NULL;

//
// DXE Core globals for Architecture Protocols
//
EFI_SECURITY_ARCH_PROTOCOL        *gSecurity      = NULL;
EFI_SECURITY2_ARCH_PROTOCOL       *gSecurity2     = NULL;
EFI_CPU_ARCH_PROTOCOL             *gCpu           = NULL;
EFI_METRONOME_ARCH_PROTOCOL       *gMetronome     = NULL;
EFI_TIMER_ARCH_PROTOCOL           *gTimer         = NULL;
EFI_BDS_ARCH_PROTOCOL             *gBds           = NULL;
EFI_WATCHDOG_TIMER_ARCH_PROTOCOL  *gWatchdogTimer = NULL;

//
// DXE Core globals for optional protocol dependencies
//
EFI_SMM_BASE2_PROTOCOL  *gSmmBase2 = NULL;

//
// DXE Core Global used to update core loaded image protocol handle
//
EFI_GUID                   *gDxeCoreFileName;
EFI_LOADED_IMAGE_PROTOCOL  *gDxeCoreLoadedImage;

//
// DXE Core Module Variables
//
EFI_BOOT_SERVICES  mBootServices = {
  {
    EFI_BOOT_SERVICES_SIGNATURE,                                                          // Signature
    EFI_BOOT_SERVICES_REVISION,                                                           // Revision
    sizeof (EFI_BOOT_SERVICES),                                                           // HeaderSize
    0,                                                                                    // CRC32
    0                                                                                     // Reserved
  },
  (EFI_RAISE_TPL)CoreRaiseTpl,                                                            // RaiseTPL
  (EFI_RESTORE_TPL)CoreRestoreTpl,                                                        // RestoreTPL
  (EFI_ALLOCATE_PAGES)CoreAllocatePages,                                                  // AllocatePages
  (EFI_FREE_PAGES)CoreFreePages,                                                          // FreePages
  (EFI_GET_MEMORY_MAP)CoreGetMemoryMap,                                                   // GetMemoryMap
  (EFI_ALLOCATE_POOL)CoreAllocatePool,                                                    // AllocatePool
  (EFI_FREE_POOL)CoreFreePool,                                                            // FreePool
  (EFI_CREATE_EVENT)CoreCreateEvent,                                                      // CreateEvent
  (EFI_SET_TIMER)CoreSetTimer,                                                            // SetTimer
  (EFI_WAIT_FOR_EVENT)CoreWaitForEvent,                                                   // WaitForEvent
  (EFI_SIGNAL_EVENT)CoreSignalEvent,                                                      // SignalEvent
  (EFI_CLOSE_EVENT)CoreCloseEvent,                                                        // CloseEvent
  (EFI_CHECK_EVENT)CoreCheckEvent,                                                        // CheckEvent
  (EFI_INSTALL_PROTOCOL_INTERFACE)CoreInstallProtocolInterface,                           // InstallProtocolInterface
  (EFI_REINSTALL_PROTOCOL_INTERFACE)CoreReinstallProtocolInterface,                       // ReinstallProtocolInterface
  (EFI_UNINSTALL_PROTOCOL_INTERFACE)CoreUninstallProtocolInterface,                       // UninstallProtocolInterface
  (EFI_HANDLE_PROTOCOL)CoreHandleProtocol,                                                // HandleProtocol
  (VOID *)NULL,                                                                           // Reserved
  (EFI_REGISTER_PROTOCOL_NOTIFY)CoreRegisterProtocolNotify,                               // RegisterProtocolNotify
  (EFI_LOCATE_HANDLE)CoreLocateHandle,                                                    // LocateHandle
  (EFI_LOCATE_DEVICE_PATH)CoreLocateDevicePath,                                           // LocateDevicePath
  (EFI_INSTALL_CONFIGURATION_TABLE)CoreInstallConfigurationTable,                         // InstallConfigurationTable
  (EFI_IMAGE_LOAD)CoreLoadImage,                                                          // LoadImage
  (EFI_IMAGE_START)CoreStartImage,                                                        // StartImage
  (EFI_EXIT)CoreExit,                                                                     // Exit
  (EFI_IMAGE_UNLOAD)CoreUnloadImage,                                                      // UnloadImage
  (EFI_EXIT_BOOT_SERVICES)CoreExitBootServices,                                           // ExitBootServices
  (EFI_GET_NEXT_MONOTONIC_COUNT)CoreEfiNotAvailableYetArg1,                               // GetNextMonotonicCount
  (EFI_STALL)CoreStall,                                                                   // Stall
  (EFI_SET_WATCHDOG_TIMER)CoreSetWatchdogTimer,                                           // SetWatchdogTimer
  (EFI_CONNECT_CONTROLLER)CoreConnectController,                                          // ConnectController
  (EFI_DISCONNECT_CONTROLLER)CoreDisconnectController,                                    // DisconnectController
  (EFI_OPEN_PROTOCOL)CoreOpenProtocol,                                                    // OpenProtocol
  (EFI_CLOSE_PROTOCOL)CoreCloseProtocol,                                                  // CloseProtocol
  (EFI_OPEN_PROTOCOL_INFORMATION)CoreOpenProtocolInformation,                             // OpenProtocolInformation
  (EFI_PROTOCOLS_PER_HANDLE)CoreProtocolsPerHandle,                                       // ProtocolsPerHandle
  (EFI_LOCATE_HANDLE_BUFFER)CoreLocateHandleBuffer,                                       // LocateHandleBuffer
  (EFI_LOCATE_PROTOCOL)CoreLocateProtocol,                                                // LocateProtocol
  (EFI_INSTALL_MULTIPLE_PROTOCOL_INTERFACES)CoreInstallMultipleProtocolInterfaces,        // InstallMultipleProtocolInterfaces
  (EFI_UNINSTALL_MULTIPLE_PROTOCOL_INTERFACES)CoreUninstallMultipleProtocolInterfaces,    // UninstallMultipleProtocolInterfaces
  (EFI_CALCULATE_CRC32)CoreEfiNotAvailableYetArg3,                                        // CalculateCrc32
  (EFI_COPY_MEM)CopyMem,                                                                  // CopyMem
  (EFI_SET_MEM)SetMem,                                                                    // SetMem
  (EFI_CREATE_EVENT_EX)CoreCreateEventEx                                                  // CreateEventEx
};

EFI_DXE_SERVICES  mDxeServices = {
  {
    DXE_SERVICES_SIGNATURE,                                           // Signature
    DXE_SERVICES_REVISION,                                            // Revision
    sizeof (DXE_SERVICES),                                            // HeaderSize
    0,                                                                // CRC32
    0                                                                 // Reserved
  },
  (EFI_ADD_MEMORY_SPACE)CoreAddMemorySpace,                               // AddMemorySpace
  (EFI_ALLOCATE_MEMORY_SPACE)CoreAllocateMemorySpace,                     // AllocateMemorySpace
  (EFI_FREE_MEMORY_SPACE)CoreFreeMemorySpace,                             // FreeMemorySpace
  (EFI_REMOVE_MEMORY_SPACE)CoreRemoveMemorySpace,                         // RemoveMemorySpace
  (EFI_GET_MEMORY_SPACE_DESCRIPTOR)CoreGetMemorySpaceDescriptor,          // GetMemorySpaceDescriptor
  (EFI_SET_MEMORY_SPACE_ATTRIBUTES)CoreSetMemorySpaceAttributes,          // SetMemorySpaceAttributes
  (EFI_GET_MEMORY_SPACE_MAP)CoreGetMemorySpaceMap,                        // GetMemorySpaceMap
  (EFI_ADD_IO_SPACE)CoreAddIoSpace,                                       // AddIoSpace
  (EFI_ALLOCATE_IO_SPACE)CoreAllocateIoSpace,                             // AllocateIoSpace
  (EFI_FREE_IO_SPACE)CoreFreeIoSpace,                                     // FreeIoSpace
  (EFI_REMOVE_IO_SPACE)CoreRemoveIoSpace,                                 // RemoveIoSpace
  (EFI_GET_IO_SPACE_DESCRIPTOR)CoreGetIoSpaceDescriptor,                  // GetIoSpaceDescriptor
  (EFI_GET_IO_SPACE_MAP)CoreGetIoSpaceMap,                                // GetIoSpaceMap
  (EFI_DISPATCH)CoreDispatcher,                                           // Dispatch
  (EFI_SCHEDULE)CoreSchedule,                                             // Schedule
  (EFI_TRUST)CoreTrust,                                                   // Trust
  (EFI_PROCESS_FIRMWARE_VOLUME)CoreProcessFirmwareVolume,                 // ProcessFirmwareVolume
  (EFI_SET_MEMORY_SPACE_CAPABILITIES)CoreSetMemorySpaceCapabilities,      // SetMemorySpaceCapabilities
};

EFI_SYSTEM_TABLE  mEfiSystemTableTemplate = {
  {
    EFI_SYSTEM_TABLE_SIGNATURE,                                           // Signature
    EFI_SYSTEM_TABLE_REVISION,                                            // Revision
    sizeof (EFI_SYSTEM_TABLE),                                            // HeaderSize
    0,                                                                    // CRC32
    0                                                                     // Reserved
  },
  NULL,                                                                   // FirmwareVendor
  0,                                                                      // FirmwareRevision
  NULL,                                                                   // ConsoleInHandle
  NULL,                                                                   // ConIn
  NULL,                                                                   // ConsoleOutHandle
  NULL,                                                                   // ConOut
  NULL,                                                                   // StandardErrorHandle
  NULL,                                                                   // StdErr
  NULL,                                                                   // RuntimeServices
  &mBootServices,                                                         // BootServices
  0,                                                                      // NumberOfConfigurationTableEntries
  NULL                                                                    // ConfigurationTable
};

EFI_RUNTIME_SERVICES  mEfiRuntimeServicesTableTemplate = {
  {
    EFI_RUNTIME_SERVICES_SIGNATURE,                               // Signature
    EFI_RUNTIME_SERVICES_REVISION,                                // Revision
    sizeof (EFI_RUNTIME_SERVICES),                                // HeaderSize
    0,                                                            // CRC32
    0                                                             // Reserved
  },
  (EFI_GET_TIME)CoreEfiNotAvailableYetArg2,                       // GetTime
  (EFI_SET_TIME)CoreEfiNotAvailableYetArg1,                       // SetTime
  (EFI_GET_WAKEUP_TIME)CoreEfiNotAvailableYetArg3,                // GetWakeupTime
  (EFI_SET_WAKEUP_TIME)CoreEfiNotAvailableYetArg2,                // SetWakeupTime
  (EFI_SET_VIRTUAL_ADDRESS_MAP)CoreEfiNotAvailableYetArg4,        // SetVirtualAddressMap
  (EFI_CONVERT_POINTER)CoreEfiNotAvailableYetArg2,                // ConvertPointer
  (EFI_GET_VARIABLE)CoreEfiNotAvailableYetArg5,                   // GetVariable
  (EFI_GET_NEXT_VARIABLE_NAME)CoreEfiNotAvailableYetArg3,         // GetNextVariableName
  (EFI_SET_VARIABLE)CoreEfiNotAvailableYetArg5,                   // SetVariable
  (EFI_GET_NEXT_HIGH_MONO_COUNT)CoreEfiNotAvailableYetArg1,       // GetNextHighMonotonicCount
  (EFI_RESET_SYSTEM)CoreEfiNotAvailableYetArg4,                   // ResetSystem
  (EFI_UPDATE_CAPSULE)CoreEfiNotAvailableYetArg3,                 // UpdateCapsule
  (EFI_QUERY_CAPSULE_CAPABILITIES)CoreEfiNotAvailableYetArg4,     // QueryCapsuleCapabilities
  (EFI_QUERY_VARIABLE_INFO)CoreEfiNotAvailableYetArg4             // QueryVariableInfo
};

EFI_RUNTIME_ARCH_PROTOCOL  gRuntimeTemplate = {
  INITIALIZE_LIST_HEAD_VARIABLE (gRuntimeTemplate.ImageHead),
  INITIALIZE_LIST_HEAD_VARIABLE (gRuntimeTemplate.EventHead),

  //
  // Make sure Size != sizeof (EFI_MEMORY_DESCRIPTOR). This will
  // prevent people from having pointer math bugs in their code.
  // now you have to use *DescriptorSize to make things work.
  //
  sizeof (EFI_MEMORY_DESCRIPTOR) + sizeof (UINT64) - (sizeof (EFI_MEMORY_DESCRIPTOR) % sizeof (UINT64)),
  EFI_MEMORY_DESCRIPTOR_VERSION,
  0,
  NULL,
  NULL,
  FALSE,
  FALSE
};

EFI_RUNTIME_ARCH_PROTOCOL  *gRuntime = &gRuntimeTemplate;

//
// DXE Core Global Variables for the EFI System Table, Boot Services Table,
// DXE Services Table, and Runtime Services Table
//
EFI_DXE_SERVICES  *gDxeCoreDS = &mDxeServices;
EFI_SYSTEM_TABLE  *gDxeCoreST = NULL;

//
// For debug initialize gDxeCoreRT to template. gDxeCoreRT must be allocated from RT memory
//  but gDxeCoreRT is used for ASSERT () and DEBUG () type macros so lets give it
//  a value that will not cause debug infrastructure to crash early on.
//
EFI_RUNTIME_SERVICES  *gDxeCoreRT         = &mEfiRuntimeServicesTableTemplate;
EFI_HANDLE            gDxeCoreImageHandle = NULL;

BOOLEAN  gMemoryMapTerminated = FALSE;

static BOOLEAN  mExitBootServicesCalled = FALSE;

//
// EFI Decompress Protocol
//
EFI_DECOMPRESS_PROTOCOL  gEfiDecompress = {
  DxeMainUefiDecompressGetInfo,
  DxeMainUefiDecompress
};

//
// For Loading modules at fixed address feature, the configuration table is to cache the top address below which to load
// Runtime code&boot time code
//
GLOBAL_REMOVE_IF_UNREFERENCED EFI_LOAD_FIXED_ADDRESS_CONFIGURATION_TABLE  gLoadModuleAtFixAddressConfigurationTable = { 0, 0 };

// Main entry point to the DXE Core
//

/**
  Main entry point to DXE Core.

  @param  HobStart               Pointer to the beginning of the HOB List from PEI.

  @return This function should never return.

**/
VOID
EFIAPI
DxeMain (
  IN  VOID  *HobStart
  )
{
  EFI_STATUS                    Status;
  EFI_PHYSICAL_ADDRESS          MemoryBaseAddress;
  UINT64                        MemoryLength;
  PE_COFF_LOADER_IMAGE_CONTEXT  ImageContext;
  UINTN                         Index;
  EFI_HOB_GUID_TYPE             *GuidHob;
  EFI_VECTOR_HANDOFF_INFO       *VectorInfoList;
  EFI_VECTOR_HANDOFF_INFO       *VectorInfo;
  VOID                          *EntryPoint;

  // Emit this marker before raw UART access.  The checkpoint below is SOL-only
  // because the platform is using a linear framebuffer.
  DEBUG ((DEBUG_ERROR, "[GX-DXE] DXE entry before raw UART\n"));
  DEBUG ((DEBUG_ERROR, "[GX-DXE] raw UART write begin\n"));
  SerialPortWrite ((UINT8 *)"[GX-DXE] serial entry\r\n", sizeof ("[GX-DXE] serial entry\r\n") - 1);
  DEBUG ((DEBUG_ERROR, "[GX-DXE] raw UART write returned\n"));
  GxDxeCheckpoint (0, "DxeCore entry");

  //
  // Setup the default exception handlers
  //
  GxDxeCheckpoint (9, "DXE vector HOB lookup begin");
  DEBUG ((DEBUG_ERROR, "[GX-DXE] incoming HOB list=%p\n", HobStart));
  VectorInfoList = NULL;
  GuidHob        = GetNextGuidHob (&gEfiVectorHandoffInfoPpiGuid, HobStart);
  if (GuidHob != NULL) {
    VectorInfoList = (EFI_VECTOR_HANDOFF_INFO *)(GET_GUID_HOB_DATA (GuidHob));
  }

  GxDxeCheckpoint (10, "DXE exception handlers begin");
  Status = InitializeCpuExceptionHandlers (VectorInfoList);
  GxDxeTraceStatus ("exception handlers returned", Status);
  ASSERT_EFI_ERROR (Status);

  //
  // Setup Stack Guard
  //
  if (PcdGetBool (PcdCpuStackGuard)) {
    GxDxeTrace ("exception stacks begin");
    Status = InitializeSeparateExceptionStacks (NULL, NULL);
    GxDxeTraceStatus ("exception stacks returned", Status);
    ASSERT_EFI_ERROR (Status);
  }

  //
  // Initialize Debug Agent to support source level debug in DXE phase
  //
  GxDxeCheckpoint (11, "DXE debug agent begin");
  InitializeDebugAgent (DEBUG_AGENT_INIT_DXE_CORE, HobStart, NULL);
  GxDxeTrace ("debug agent returned");

  //
  // Initialize Memory Services
  //
  GxDxeCheckpoint (12, "DXE memory services begin");
  CoreInitializeMemoryServices (&HobStart, &MemoryBaseAddress, &MemoryLength);
  DEBUG ((DEBUG_ERROR, "[GX-DXE] memory services returned hob=%p base=0x%lx size=0x%lx\n",
    HobStart, MemoryBaseAddress, MemoryLength));

  GxDxeTrace ("memory profile begin");
  MemoryProfileInit (HobStart);
  GxDxeTrace ("memory profile returned");

  //
  // Start the Handle Services.
  //
  Status = CoreInitializeHandleServices ();
  GxDxeTraceStatus ("handle services", Status);
  GxDxeCheckpoint (1, EFI_ERROR (Status) ? "Handle services FAILED" : "Handle services initialized");
  ASSERT_EFI_ERROR (Status);

  //
  // Start the Image Services.
  //
  Status = CoreInitializeImageServices (HobStart);
  GxDxeTraceStatus ("image services", Status);
  GxDxeCheckpoint (2, EFI_ERROR (Status) ? "Image services FAILED" : "Image services initialized");
  ASSERT_EFI_ERROR (Status);

  //
  // Initialize the Global Coherency Domain Services
  //
  Status = CoreInitializeGcdServices (&HobStart, MemoryBaseAddress, MemoryLength);
  GxDxeTraceStatus ("GCD services", Status);
  GxDxeCheckpoint (3, EFI_ERROR (Status) ? "GCD services FAILED" : "GCD services initialized");
  ASSERT_EFI_ERROR (Status);

  //
  // Allocate the EFI System Table and EFI Runtime Service Table from EfiRuntimeServicesData
  // Use the templates to initialize the contents of the EFI System Table and EFI Runtime Services Table
  //
  GxDxeTrace ("runtime tables begin");
  gDxeCoreST = AllocateRuntimeCopyPool (sizeof (EFI_SYSTEM_TABLE), &mEfiSystemTableTemplate);
  ASSERT (gDxeCoreST != NULL);

  gDxeCoreRT = AllocateRuntimeCopyPool (sizeof (EFI_RUNTIME_SERVICES), &mEfiRuntimeServicesTableTemplate);
  ASSERT (gDxeCoreRT != NULL);
  GxDxeTrace ("runtime tables allocated");

  gDxeCoreST->RuntimeServices = gDxeCoreRT;

  //
  // Update DXE Core Loaded Image Protocol with allocated UEFI System Table
  //
  gDxeCoreLoadedImage->SystemTable = gDxeCoreST;

  //
  // Call constructor for all libraries
  //
  GxDxeTrace ("library constructors begin");
  ProcessLibraryConstructorList (gDxeCoreImageHandle, gDxeCoreST);
  GxDxeTrace ("library constructors complete");
  PERF_CROSSMODULE_END ("PEI");
  PERF_CROSSMODULE_BEGIN ("DXE");

  //
  // Log MemoryBaseAddress and MemoryLength again (from
  // CoreInitializeMemoryServices()), now that library constructors have
  // executed.
  //
  DEBUG ((
    DEBUG_INFO,
    "%a: MemoryBaseAddress=0x%Lx MemoryLength=0x%Lx\n",
    __func__,
    MemoryBaseAddress,
    MemoryLength
    ));

  //
  // Report DXE Core image information to the PE/COFF Extra Action Library
  //
  ZeroMem (&ImageContext, sizeof (ImageContext));
  ImageContext.ImageAddress  = (EFI_PHYSICAL_ADDRESS)(UINTN)gDxeCoreLoadedImage->ImageBase;
  ImageContext.PdbPointer    = PeCoffLoaderGetPdbPointer ((VOID *)(UINTN)ImageContext.ImageAddress);
  ImageContext.SizeOfHeaders = PeCoffGetSizeOfHeaders ((VOID *)(UINTN)ImageContext.ImageAddress);
  Status                     = PeCoffLoaderGetEntryPoint ((VOID *)(UINTN)ImageContext.ImageAddress, &EntryPoint);
  if (Status == EFI_SUCCESS) {
    ImageContext.EntryPoint = (EFI_PHYSICAL_ADDRESS)(UINTN)EntryPoint;
  }

  ImageContext.Handle    = (VOID *)(UINTN)gDxeCoreLoadedImage->ImageBase;
  ImageContext.ImageRead = PeCoffLoaderImageReadFromMemory;
  PeCoffLoaderRelocateImageExtraAction (&ImageContext);
  GxDxeTrace ("PE/COFF image action complete");

  //
  // Install the DXE Services Table into the EFI System Tables's Configuration Table
  //
  Status = CoreInstallConfigurationTable (&gEfiDxeServicesTableGuid, gDxeCoreDS);
  GxDxeTraceStatus ("DXE services table", Status);
  ASSERT_EFI_ERROR (Status);

  //
  // Install the HOB List into the EFI System Tables's Configuration Table
  //
  Status = CoreInstallConfigurationTable (&gEfiHobListGuid, HobStart);
  GxDxeTraceStatus ("HOB configuration table", Status);
  ASSERT_EFI_ERROR (Status);

  //
  // Install Memory Type Information Table into the EFI System Tables's Configuration Table
  //
  Status = CoreInstallConfigurationTable (&gEfiMemoryTypeInformationGuid, &gMemoryTypeInformation);
  GxDxeTraceStatus ("memory type table", Status);
  ASSERT_EFI_ERROR (Status);

  //
  // If Loading modules At fixed address feature is enabled, install Load moduels at fixed address
  // Configuration Table so that user could easily to retrieve the top address to load Dxe and PEI
  // Code and Tseg base to load SMM driver.
  //
  if (PcdGet64 (PcdLoadModuleAtFixAddressEnable) != 0) {
    Status = CoreInstallConfigurationTable (&gLoadFixedAddressConfigurationTableGuid, &gLoadModuleAtFixAddressConfigurationTable);
    ASSERT_EFI_ERROR (Status);
  }

  //
  // Report Status Code here for DXE_ENTRY_POINT once it is available
  //
  REPORT_STATUS_CODE (
    EFI_PROGRESS_CODE,
    (EFI_SOFTWARE_DXE_CORE | EFI_SW_DXE_CORE_PC_ENTRY_POINT)
    );

  //
  // Create the aligned system table pointer structure that is used by external
  // debuggers to locate the system table...  Also, install debug image info
  // configuration table.
  //
  CoreInitializeDebugImageInfoTable ();
  CoreNewDebugImageInfoEntry (
    EFI_DEBUG_IMAGE_INFO_TYPE_NORMAL,
    gDxeCoreLoadedImage,
    gDxeCoreImageHandle
    );
  GxDxeTrace ("debug image table ready");

  DEBUG ((DEBUG_INFO | DEBUG_LOAD, "HOBLIST address in DXE = 0x%p\n", HobStart));

  DEBUG_CODE_BEGIN ();
  EFI_PEI_HOB_POINTERS  Hob;

  for (Hob.Raw = HobStart; !END_OF_HOB_LIST (Hob); Hob.Raw = GET_NEXT_HOB (Hob)) {
    if (GET_HOB_TYPE (Hob) == EFI_HOB_TYPE_MEMORY_ALLOCATION) {
      DEBUG ((
        DEBUG_ERROR,
        "Memory Allocation 0x%08x 0x%0lx - 0x%0lx\n", \
        Hob.MemoryAllocation->AllocDescriptor.MemoryType,                      \
        Hob.MemoryAllocation->AllocDescriptor.MemoryBaseAddress,               \
        Hob.MemoryAllocation->AllocDescriptor.MemoryBaseAddress + Hob.MemoryAllocation->AllocDescriptor.MemoryLength - 1
        ));
    }
  }

  for (Hob.Raw = HobStart; !END_OF_HOB_LIST (Hob); Hob.Raw = GET_NEXT_HOB (Hob)) {
    if (GET_HOB_TYPE (Hob) == EFI_HOB_TYPE_FV) {
      DEBUG ((
        DEBUG_ERROR,
        "FV Hob            0x%0lx - 0x%0lx\n",
        Hob.FirmwareVolume->BaseAddress,
        Hob.FirmwareVolume->BaseAddress + Hob.FirmwareVolume->Length - 1
        ));
    } else if (GET_HOB_TYPE (Hob) == EFI_HOB_TYPE_FV2) {
      DEBUG ((
        DEBUG_ERROR,
        "FV2 Hob           0x%0lx - 0x%0lx\n",
        Hob.FirmwareVolume2->BaseAddress,
        Hob.FirmwareVolume2->BaseAddress + Hob.FirmwareVolume2->Length - 1
        ));
      DEBUG ((
        DEBUG_ERROR,
        "                  %g - %g\n",
        &Hob.FirmwareVolume2->FvName,
        &Hob.FirmwareVolume2->FileName
        ));
    } else if (GET_HOB_TYPE (Hob) == EFI_HOB_TYPE_FV3) {
      DEBUG ((
        DEBUG_ERROR,
        "FV3 Hob           0x%0lx - 0x%0lx - 0x%x - 0x%x\n",
        Hob.FirmwareVolume3->BaseAddress,
        Hob.FirmwareVolume3->BaseAddress + Hob.FirmwareVolume3->Length - 1,
        Hob.FirmwareVolume3->AuthenticationStatus,
        Hob.FirmwareVolume3->ExtractedFv
        ));
      if (Hob.FirmwareVolume3->ExtractedFv) {
        DEBUG ((
          DEBUG_ERROR,
          "                  %g - %g\n",
          &Hob.FirmwareVolume3->FvName,
          &Hob.FirmwareVolume3->FileName
          ));
      }
    }
  }

  DEBUG_CODE_END ();
  GxDxeTrace ("HOB scan complete");

  //
  // Initialize the Event Services
  //
  Status = CoreInitializeEventServices ();
  GxDxeTraceStatus ("event services", Status);
  ASSERT_EFI_ERROR (Status);

  //
  // Give the debug agent a chance to initialize with events.
  //
  InitializeDebugAgent (DEBUG_AGENT_INIT_DXE_CORE_LATE, HobStart, NULL);

  MemoryProfileInstallProtocol ();

  CoreInitializeMemoryAttributesTable ();
  CoreInitializeMemoryProtection ();
  GxDxeTrace ("memory protection ready");

  //
  // Get persisted vector hand-off info from GUIDeed HOB again due to HobStart may be updated,
  // and install configuration table
  //
  GxDxeTrace ("vector handoff begin");
  GuidHob = GetNextGuidHob (&gEfiVectorHandoffInfoPpiGuid, HobStart);
  if (GuidHob != NULL) {
    VectorInfoList = (EFI_VECTOR_HANDOFF_INFO *)(GET_GUID_HOB_DATA (GuidHob));
    VectorInfo     = VectorInfoList;
    Index          = 1;
    while (VectorInfo->Attribute != EFI_VECTOR_HANDOFF_LAST_ENTRY) {
      VectorInfo++;
      Index++;
    }

    VectorInfo = AllocateCopyPool (sizeof (EFI_VECTOR_HANDOFF_INFO) * Index, (VOID *)VectorInfoList);
    ASSERT (VectorInfo != NULL);
    Status = CoreInstallConfigurationTable (&gEfiVectorHandoffTableGuid, (VOID *)VectorInfo);
    GxDxeTraceStatus ("vector handoff table", Status);
    ASSERT_EFI_ERROR (Status);
  }
  GxDxeTrace ("vector handoff complete");

  //
  // Get the Protocols that were passed in from PEI to DXE through GUIDed HOBs
  //
  // These Protocols are not architectural. This implementation is sharing code between
  // PEI and DXE in order to save FLASH space. These Protocols could also be implemented
  // as part of the DXE Core. However, that would also require the DXE Core to be ported
  // each time a different CPU is used, a different Decompression algorithm is used, or a
  // different Image type is used. By placing these Protocols in PEI, the DXE Core remains
  // generic, and only PEI and the Arch Protocols need to be ported from Platform to Platform,
  // and from CPU to CPU.
  //

  //
  // Publish the EFI, Tiano, and Custom Decompress protocols for use by other DXE components
  //
  Status = CoreInstallMultipleProtocolInterfaces (
             &mDecompressHandle,
             &gEfiDecompressProtocolGuid,
             &gEfiDecompress,
             NULL
             );
  GxDxeTraceStatus ("decompression protocol", Status);
  ASSERT_EFI_ERROR (Status);

  //
  // Register for the GUIDs of the Architectural Protocols, so the rest of the
  // EFI Boot Services and EFI Runtime Services tables can be filled in.
  // Also register for the GUIDs of optional protocols.
  //
  CoreNotifyOnProtocolInstallation ();
  GxDxeTrace ("architectural protocol notifications ready");

  //
  // Produce Firmware Volume Protocols, one for each FV in the HOB list.
  //
  Status = FwVolBlockDriverInit (gDxeCoreImageHandle, gDxeCoreST);
  GxDxeTraceStatus ("firmware volume block", Status);
  ASSERT_EFI_ERROR (Status);

  Status = FwVolDriverInit (gDxeCoreImageHandle, gDxeCoreST);
  GxDxeTraceStatus ("firmware volume driver", Status);
  ASSERT_EFI_ERROR (Status);

  //
  // Produce the Section Extraction Protocol
  //
  Status = InitializeSectionExtraction (gDxeCoreImageHandle, gDxeCoreST);
  GxDxeTraceStatus ("section extraction", Status);
  ASSERT_EFI_ERROR (Status);

  //
  // Initialize the DXE Dispatcher
  //
  GxDxeTrace ("DXE diagnostics protocol install begin");
  mGxDxeDiagnosticsProtocol.Log = GxDxeDiagnosticsProtocolLog;
  mGxDxeDiagnosticsProtocol.GetDriverStatus = GxDxeDiagnosticsProtocolGetDriverStatus;
  mGxDxeDiagnosticsProtocol.SetShellLoadTrace = GxDxeDiagnosticsProtocolSetShellLoadTrace;
  mGxDxeDiagnosticsProtocol.RunChecks = GxDxeDiagnosticsRunChecks;
  mGxDxeDiagnosticsProtocol.GetCheck = GxDxeDiagnosticsGetCheck;
  mGxDxeDiagnosticsProtocol.GetSummary = GxDxeDiagnosticsGetSummary;
  Status = gBS->InstallProtocolInterface (
                  &mGxDxeDiagnosticsHandle,
                  &gGxDxeDiagnosticsProtocolGuid,
                  EFI_NATIVE_INTERFACE,
                  &mGxDxeDiagnosticsProtocol
                  );
  GxDxeTraceStatus ("DXE diagnostics protocol install", Status);
  ASSERT_EFI_ERROR (Status);

  GxDxeTrace ("DXE dispatcher init begin");
  CoreInitializeDispatcher ();
  GxDxeCheckpoint (4, "DXE dispatcher initialized");

  //
  // Invoke the DXE Dispatcher
  //
  GxDxeTrace ("DXE dispatcher run begin");
  GxDxeCheckpoint (8, "DXE dispatcher running");
  Status = CoreDispatcher ();
  GxDxeTraceStatus ("DXE dispatcher returned", Status);
  GxDxeCheckpoint (5, "DXE dispatcher completed");

  //
  // Display Architectural protocols that were not loaded if this is DEBUG build
  //
  DEBUG_CODE_BEGIN ();
  CoreDisplayMissingArchProtocols ();
  DEBUG_CODE_END ();

  //
  // Display any drivers that were not dispatched because dependency expression
  // evaluated to false if this is a debug build
  //
  DEBUG_CODE_BEGIN ();
  CoreDisplayDiscoveredNotDispatched ();
  DEBUG_CODE_END ();

  //
  // Assert if the Architectural Protocols are not present.
  //
  Status = CoreAllEfiServicesAvailable ();
  if (EFI_ERROR (Status)) {
    GxDxeCheckpoint (6, "DXE architectural protocol failure");
    //
    // Report Status code that some Architectural Protocols are not present.
    //
    REPORT_STATUS_CODE (
      EFI_ERROR_CODE | EFI_ERROR_MAJOR,
      (EFI_SOFTWARE_DXE_CORE | EFI_SW_DXE_CORE_EC_NO_ARCH)
      );
  }

  ASSERT_EFI_ERROR (Status);

  //
  // Report Status code before transfer control to BDS
  //
  REPORT_STATUS_CODE (
    EFI_PROGRESS_CODE,
    (EFI_SOFTWARE_DXE_CORE | EFI_SW_DXE_CORE_PC_HANDOFF_TO_NEXT)
    );

  //
  // Transfer control to the BDS Architectural Protocol
  //
  GxDxeCheckpoint (7, "BDS handoff");
  gBds->Entry (gBds);

  //
  // BDS should never return
  //
  ASSERT (FALSE);
  CpuDeadLoop ();

  UNREACHABLE ();
}

/**
  Place holder function until all the Boot Services and Runtime Services are
  available.

  @param  Arg1                   Undefined

  @return EFI_NOT_AVAILABLE_YET

**/
EFI_STATUS
EFIAPI
CoreEfiNotAvailableYetArg1 (
  UINTN  Arg1
  )
{
  //
  // This function should never be executed.  If it does, then the architectural protocols
  // have not been designed correctly.  The CpuBreakpoint () is commented out for now until the
  // DXE Core and all the Architectural Protocols are complete.
  //

  return EFI_NOT_AVAILABLE_YET;
}

/**
  Place holder function until all the Boot Services and Runtime Services are available.

  @param  Arg1                   Undefined
  @param  Arg2                   Undefined

  @return EFI_NOT_AVAILABLE_YET

**/
EFI_STATUS
EFIAPI
CoreEfiNotAvailableYetArg2 (
  UINTN  Arg1,
  UINTN  Arg2
  )
{
  //
  // This function should never be executed.  If it does, then the architectural protocols
  // have not been designed correctly.  The CpuBreakpoint () is commented out for now until the
  // DXE Core and all the Architectural Protocols are complete.
  //

  return EFI_NOT_AVAILABLE_YET;
}

/**
  Place holder function until all the Boot Services and Runtime Services are available.

  @param  Arg1                   Undefined
  @param  Arg2                   Undefined
  @param  Arg3                   Undefined

  @return EFI_NOT_AVAILABLE_YET

**/
EFI_STATUS
EFIAPI
CoreEfiNotAvailableYetArg3 (
  UINTN  Arg1,
  UINTN  Arg2,
  UINTN  Arg3
  )
{
  //
  // This function should never be executed.  If it does, then the architectural protocols
  // have not been designed correctly.  The CpuBreakpoint () is commented out for now until the
  // DXE Core and all the Architectural Protocols are complete.
  //

  return EFI_NOT_AVAILABLE_YET;
}

/**
  Place holder function until all the Boot Services and Runtime Services are available.

  @param  Arg1                   Undefined
  @param  Arg2                   Undefined
  @param  Arg3                   Undefined
  @param  Arg4                   Undefined

  @return EFI_NOT_AVAILABLE_YET

**/
EFI_STATUS
EFIAPI
CoreEfiNotAvailableYetArg4 (
  UINTN  Arg1,
  UINTN  Arg2,
  UINTN  Arg3,
  UINTN  Arg4
  )
{
  //
  // This function should never be executed.  If it does, then the architectural protocols
  // have not been designed correctly.  The CpuBreakpoint () is commented out for now until the
  // DXE Core and all the Architectural Protocols are complete.
  //

  return EFI_NOT_AVAILABLE_YET;
}

/**
  Place holder function until all the Boot Services and Runtime Services are available.

  @param  Arg1                   Undefined
  @param  Arg2                   Undefined
  @param  Arg3                   Undefined
  @param  Arg4                   Undefined
  @param  Arg5                   Undefined

  @return EFI_NOT_AVAILABLE_YET

**/
EFI_STATUS
EFIAPI
CoreEfiNotAvailableYetArg5 (
  UINTN  Arg1,
  UINTN  Arg2,
  UINTN  Arg3,
  UINTN  Arg4,
  UINTN  Arg5
  )
{
  //
  // This function should never be executed.  If it does, then the architectural protocols
  // have not been designed correctly.  The CpuBreakpoint () is commented out for now until the
  // DXE Core and all the Architectural Protocols are complete.
  //

  return EFI_NOT_AVAILABLE_YET;
}

/**
  Calcualte the 32-bit CRC in a EFI table using the service provided by the
  gRuntime service.

  @param  Hdr                    Pointer to an EFI standard header

**/
VOID
CalculateEfiHdrCrc (
  IN  OUT EFI_TABLE_HEADER  *Hdr
  )
{
  UINT32  Crc;

  Hdr->CRC32 = 0;

  //
  // If gBS->CalculateCrce32 () == CoreEfiNotAvailableYet () then
  //  Crc will come back as zero if we set it to zero here
  //
  Crc = 0;
  gBS->CalculateCrc32 ((UINT8 *)Hdr, Hdr->HeaderSize, &Crc);
  Hdr->CRC32 = Crc;
}

/**
  Terminates all boot services.

  @param  ImageHandle            Handle that identifies the exiting image.
  @param  MapKey                 Key to the latest memory map.

  @retval EFI_SUCCESS            Boot Services terminated
  @retval EFI_INVALID_PARAMETER  MapKey is incorrect.

**/
EFI_STATUS
EFIAPI
CoreExitBootServices (
  IN EFI_HANDLE  ImageHandle,
  IN UINTN       MapKey
  )
{
  EFI_STATUS                 Status;
  EFI_STATUS                 GxImageStatus;
  CHAR8                      GxMessage[224];
  EFI_LOADED_IMAGE_PROTOCOL  *GxLoadedImage;
  LOADED_IMAGE_PRIVATE_DATA   *GxImage;

  GxLoadedImage = NULL;
  GxDxeEbsCheckpoint ("OS EBS ENTRY: locating loaded-image metadata");
  GxImageStatus = CoreHandleProtocol (ImageHandle, &gEfiLoadedImageProtocolGuid, (VOID **)&GxLoadedImage);
  GxImage       = (!EFI_ERROR (GxImageStatus) && (GxLoadedImage != NULL)) ?
                  LOADED_IMAGE_PRIVATE_DATA_FROM_THIS (GxLoadedImage) : NULL;
  AsciiSPrint (GxMessage, sizeof (GxMessage),
    "OS EBS BEGIN image=%p loaded_image_status=%r parent=%p base=%p size=%lu machine=%u entry=%p options=%u map_key=0x%lx",
    ImageHandle,
    GxImageStatus,
    (GxLoadedImage != NULL) ? GxLoadedImage->ParentHandle : NULL,
    (GxLoadedImage != NULL) ? GxLoadedImage->ImageBase : NULL,
    (GxLoadedImage != NULL) ? (UINT64)GxLoadedImage->ImageSize : 0,
    (GxImage != NULL) ? GxImage->Machine : 0,
    (GxImage != NULL) ? GxImage->EntryPoint : NULL,
    (GxLoadedImage != NULL) ? GxLoadedImage->LoadOptionsSize : 0,
    (UINT64)MapKey);
  GxDxeLog (GxMessage);
  GxDxeEbsCheckpoint (GxMessage);

  GxDxeLogCurrentImagePath (ImageHandle);

  if ((GxLoadedImage != NULL) && (GxLoadedImage->LoadOptions != NULL) && (GxLoadedImage->LoadOptionsSize != 0)) {
    GxDxeLogUnicodeData ("OS EBS OPTIONS", (CONST CHAR16 *)GxLoadedImage->LoadOptions, GxLoadedImage->LoadOptionsSize);
  } else {
    GxDxeLog ("OS EBS OPTIONS <none>");
  }

  //
  // Notify other drivers of their last chance to use boot services
  // before the memory map is terminated.
  //
  if (!mExitBootServicesCalled) {
    GxDxeLog ("OS EBS BEFORE_EXIT_BOOT_SERVICES NOTIFY BEGIN");
    GxDxeEbsCheckpoint ("OS EBS BEFORE_EXIT_BOOT_SERVICES notify begin");
    CoreNotifySignalList (&gEfiEventBeforeExitBootServicesGuid);
    GxDxeLog ("OS EBS BEFORE_EXIT_BOOT_SERVICES NOTIFY RETURNED");
    GxDxeEbsCheckpoint ("OS EBS BEFORE_EXIT_BOOT_SERVICES notify returned");
    mExitBootServicesCalled = TRUE;
  }

  //
  // Disable Timer
  //
  GxDxeEbsCheckpoint ("OS EBS timer disable begin");
  gTimer->SetTimerPeriod (gTimer, 0);
  GxDxeEbsCheckpoint ("OS EBS timer disable returned");

  //
  // Terminate memory services if the MapKey matches
  //
  AsciiSPrint (GxMessage, sizeof (GxMessage), "OS EBS memory-map validation begin map_key=0x%lx", (UINT64)MapKey);
  GxDxeEbsCheckpoint (GxMessage);
  Status = CoreTerminateMemoryMap (MapKey);
  if (EFI_ERROR (Status)) {
    AsciiSPrint (GxMessage, sizeof (GxMessage), "OS EBS REJECTED map_key=0x%lx status=%r",
      (UINT64)MapKey, Status);
    GxDxeLog (GxMessage);
    GxDxeEbsCheckpoint (GxMessage);
    //
    // Notify other drivers that ExitBootServices fail
    //
    GxDxeEbsCheckpoint ("OS EBS failed-map notification begin");
    CoreNotifySignalList (&gEventExitBootServicesFailedGuid);
    GxDxeEbsCheckpoint ("OS EBS failed-map notification returned");
    return Status;
  }

  gMemoryMapTerminated = TRUE;
  AsciiSPrint (GxMessage, sizeof (GxMessage), "OS EBS MAP KEY ACCEPTED status=EFI_SUCCESS map_key=0x%lx cleanup=IN_PROGRESS", (UINT64)MapKey);
  GxDxeEbsCheckpoint (GxMessage);

  //
  // Notify other drivers that we are exiting boot services.
  //
  GxDxeEbsCheckpoint ("OS EBS EXIT_BOOT_SERVICES notify begin");
  CoreNotifySignalList (&gEfiEventExitBootServicesGuid);
  GxDxeEbsCheckpoint ("OS EBS EXIT_BOOT_SERVICES notify returned");

  //
  // Report that ExitBootServices() has been called
  //
  GxDxeEbsCheckpoint ("OS EBS progress status-code report begin");
  REPORT_STATUS_CODE (
    EFI_PROGRESS_CODE,
    (EFI_SOFTWARE_EFI_BOOT_SERVICE | EFI_SW_BS_PC_EXIT_BOOT_SERVICES)
    );
  GxDxeEbsCheckpoint ("OS EBS progress status-code report returned");

  GxDxeEbsCheckpoint ("OS EBS memory-protection callback begin");
  MemoryProtectionExitBootServicesCallback ();
  GxDxeEbsCheckpoint ("OS EBS memory-protection callback returned");

  //
  // Disable interrupt of Debug timer.
  //
  GxDxeEbsCheckpoint ("OS EBS debug-timer interrupt disable begin");
  SaveAndSetDebugTimerInterrupt (FALSE);
  GxDxeEbsCheckpoint ("OS EBS debug-timer interrupt disable returned");

  //
  // Disable CPU Interrupts
  //
  GxDxeEbsCheckpoint ("OS EBS CPU interrupt disable begin");
  gCpu->DisableInterrupt (gCpu);
  GxDxeEbsCheckpoint ("OS EBS CPU interrupts disabled");

  //
  // Clear the non-runtime values of the EFI System Table
  //
  gDxeCoreST->BootServices        = NULL;
  gDxeCoreST->ConIn               = NULL;
  gDxeCoreST->ConsoleInHandle     = NULL;
  gDxeCoreST->ConOut              = NULL;
  gDxeCoreST->ConsoleOutHandle    = NULL;
  gDxeCoreST->StdErr              = NULL;
  gDxeCoreST->StandardErrorHandle = NULL;
  GxDxeEbsCheckpoint ("OS EBS console and boot-services pointers cleared");

  //
  // Recompute the 32-bit CRC of the EFI System Table
  //
  GxDxeEbsCheckpoint ("OS EBS system-table CRC calculation begin");
  CalculateEfiHdrCrc (&gDxeCoreST->Hdr);
  GxDxeEbsCheckpoint ("OS EBS system-table CRC calculation returned");

  //
  // Zero out the Boot Service Table
  //
  GxDxeEbsCheckpoint ("OS EBS boot-services table zero begin");
  ZeroMem (gBS, sizeof (EFI_BOOT_SERVICES));
  gBS = NULL;
  GxDxeEbsCheckpoint ("OS EBS boot-services table zero returned");

  //
  // Update the AtRuntime field in Runtiem AP.
  //
  GxDxeEbsCheckpoint ("OS EBS runtime-state transition begin");
  gRuntime->AtRuntime = TRUE;
  GxDxeEbsCheckpoint ("OS EBS runtime-state transition returned");

  //
  // This is the final diagnostic checkpoint before returning success to the
  // caller of ExitBootServices(). Keep it after all core cleanup, and do not
  // add boot-service allocations or protocol calls after the memory map has
  // been terminated.
  //
  GxDxeEbsCheckpoint ("OS EBS core returning status=EFI_SUCCESS");
  GxDxeEbsCheckpoint ("OS EBS VGA background restore begin");
  GxDxeRestoreScreenLogBackground ();
  SerialPortWrite ((UINT8 *)"[GX-EBS] OS EBS VGA background restore returned\r\n",
    sizeof ("[GX-EBS] OS EBS VGA background restore returned\r\n") - 1);

  return Status;
}

STATIC VOID
GxDiagAdd (IN CONST CHAR8 *Name, IN GX_DIAG_RESULT Result, IN BOOLEAN Required, IN CONST CHAR8 *Detail)
{
  GX_DIAG_CHECK *Check;
  if (mGxDiagCheckCount >= GX_DIAG_MAX_CHECKS) return;
  Check = &mGxDiagChecks[mGxDiagCheckCount++];
  ZeroMem (Check, sizeof (*Check));
  AsciiStrCpyS (Check->Name, sizeof (Check->Name), Name);
  AsciiStrCpyS (Check->Detail, sizeof (Check->Detail), Detail);
  Check->Result = Result;
  Check->Required = Required;
}

STATIC EFI_STATUS EFIAPI
GxDxeDiagnosticsRunChecks (IN CONST CHAR8 *Category)
{
  EFI_STATUS Status;
  EFI_MEMORY_DESCRIPTOR *Map;
  UINTN MapSize, MapKey, DescriptorSize, Index, HandleCount;
  UINT32 DescriptorVersion;
  EFI_CONFIGURATION_TABLE *Table;
  VOID *HobList;
  BOOLEAN FoundHob, FoundAcpi;
  EFI_GUID AcpiGuid = EFI_ACPI_20_TABLE_GUID;
  EFI_GUID GopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
  EFI_GUID BlockGuid = EFI_BLOCK_IO_PROTOCOL_GUID;
  EFI_GUID FsGuid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
  EFI_GUID MpGuid = EFI_MP_SERVICES_PROTOCOL_GUID;
  EFI_HANDLE *Handles;
  EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop;
  UINTN Required, BootOrderSize;
  (VOID)Category;
  mGxDiagCheckCount = 0;

  FoundHob = FALSE; HobList = NULL;
  if (gDxeCoreST != NULL) {
    for (Index = 0; Index < gDxeCoreST->NumberOfTableEntries; Index++) {
      Table = &gDxeCoreST->ConfigurationTable[Index];
      if (CompareGuid (&Table->VendorGuid, &gEfiHobListGuid)) { FoundHob = TRUE; HobList = Table->VendorTable; break; }
    }
  }
  {
    CHAR8  Message[GX_DXE_FB_COLUMNS + 1];

    AsciiSPrint (Message, sizeof (Message), "PREBOOT HOB table present=%u list=%p entries=%lu", FoundHob, HobList,
      (UINT64)((gDxeCoreST != NULL) ? gDxeCoreST->NumberOfTableEntries : 0));
    GxDxeLogHobSerialOnly (Message);
  }
  if (FoundHob && (HobList != NULL)) {
    EFI_HOB_HANDOFF_INFO_TABLE *Phit = HobList;
    EFI_HOB_GENERIC_HEADER *Hob = HobList;
    UINTN Seen = 0;
    BOOLEAN EndSeen = FALSE, Valid = TRUE;
    UINTN Remaining;
    CONST CHAR8 *HobFailure = "end marker not found";
    BOOLEAN PhitHeaderValid;
    BOOLEAN FreeBoundsValid;
    CHAR8 Message[GX_DXE_FB_COLUMNS + 1];

    AsciiSPrint (Message, sizeof (Message), "PREBOOT HOB PHIT addr=%p type=0x%04x length=%u expected_type=0x%04x min_length=%u",
      HobList, Phit->Header.HobType, (UINT32)Phit->Header.HobLength, EFI_HOB_TYPE_HANDOFF, (UINT32)sizeof (*Phit));
    GxDxeLogHobSerialOnly (Message);
    PhitHeaderValid = (BOOLEAN)((Phit->Header.HobType == EFI_HOB_TYPE_HANDOFF) &&
                                (Phit->Header.HobLength >= sizeof (*Phit)));
    if (!PhitHeaderValid) {
      Valid = FALSE;
      HobFailure = (Phit->Header.HobType != EFI_HOB_TYPE_HANDOFF) ? "invalid PHIT type" : "PHIT length too short";
    }

    if (PhitHeaderValid) {
      AsciiSPrint (Message, sizeof (Message),
        "PREBOOT PHIT memory bottom=0x%016lx top=0x%016lx free_bottom=0x%016lx free_top=0x%016lx end_hob=0x%016lx list=0x%016lx",
        Phit->EfiMemoryBottom, Phit->EfiMemoryTop, Phit->EfiFreeMemoryBottom, Phit->EfiFreeMemoryTop,
        Phit->EfiEndOfHobList, (UINT64)(UINTN)HobList);
      GxDxeLogSerialOnly (Message);
      if ((UINTN)Phit->EfiEndOfHobList < (UINTN)HobList) {
        Valid = FALSE;
        HobFailure = "end pointer before HOB list";
      }
      if (!Valid) {
        AsciiSPrint (Message, sizeof (Message), "PREBOOT HOB FAIL reason=%a", HobFailure);
        GxDxeLogHobSerialOnly (Message);
      }
    }

    Remaining = Valid ? ((UINTN)Phit->EfiEndOfHobList - (UINTN)HobList + sizeof (*Hob)) : 0;
    while (Valid && (Seen < 4096) && (Remaining >= sizeof (*Hob))) {
      UINT16 HobLength;

      AsciiSPrint (Message, sizeof (Message), "PREBOOT HOB[%lu] addr=%p type=0x%04x length=%u remaining=%lu",
        (UINT64)Seen, Hob, Hob->HobType, (UINT32)Hob->HobLength, (UINT64)Remaining);
      GxDxeLogHobSerialOnly (Message);
      switch (G4delDiagHobHeaderReason (Hob->HobType, Hob->HobLength, Remaining,
                                        EFI_HOB_TYPE_END_OF_HOB_LIST, sizeof (*Hob))) {
        case G4delDiagHobHeaderTooShort:
          Valid = FALSE;
          HobFailure = "HOB length shorter than header";
          break;
        case G4delDiagHobHeaderExceedsRemaining:
          Valid = FALSE;
          HobFailure = "HOB length exceeds remaining span";
          break;
        case G4delDiagHobEndMarkerWrongLength:
          Valid = FALSE;
          HobFailure = "end marker has invalid length";
          break;
        case G4delDiagHobHeaderValidReason:
        default:
          break;
      }
      if (!Valid) {
        AsciiSPrint (Message, sizeof (Message), "PREBOOT HOB FAIL index=%lu addr=%p type=0x%04x length=%u remaining=%lu reason=%a",
          (UINT64)Seen, Hob, Hob->HobType, (UINT32)Hob->HobLength, (UINT64)Remaining, HobFailure);
        GxDxeLogHobSerialOnly (Message);
        break;
      }
      if (Hob->HobType == EFI_HOB_TYPE_END_OF_HOB_LIST) { EndSeen = TRUE; HobFailure = "none"; break; }
      HobLength = Hob->HobLength;
      Hob = (EFI_HOB_GENERIC_HEADER *)((UINT8 *)Hob + Hob->HobLength);
      Remaining -= HobLength;
      Seen++;
    }
    if (Valid && !EndSeen) {
      Valid = FALSE;
      HobFailure = (Seen >= 4096) ? "HOB walk limit reached" : "truncated before end marker";
      AsciiSPrint (Message, sizeof (Message), "PREBOOT HOB FAIL index=%lu remaining=%lu reason=%a",
        (UINT64)Seen, (UINT64)Remaining, HobFailure);
      GxDxeLogHobSerialOnly (Message);
    }
    FreeBoundsValid = FALSE;
    if (PhitHeaderValid) {
      FreeBoundsValid = (BOOLEAN)(Phit->EfiFreeMemoryTop > Phit->EfiFreeMemoryBottom);
      AsciiSPrint (Message, sizeof (Message),
        "PREBOOT PHIT free range bottom=0x%016lx top=0x%016lx comparison=top>bottom result=%a",
        Phit->EfiFreeMemoryBottom, Phit->EfiFreeMemoryTop, FreeBoundsValid ? "PASS" : "FAIL");
      GxDxeLogHobSerialOnly (Message);
    } else {
      AsciiSPrint (Message, sizeof (Message), "PREBOOT PHIT free-range check unavailable: %a", HobFailure);
      GxDxeLogHobSerialOnly (Message);
    }
    GxDiagAdd ("PEI memory init", (FreeBoundsValid) ? GxDiagPass : GxDiagFail, TRUE,
      "PHIT free-memory bounds checked");
    GxDiagAdd ("HOB list", (Valid && EndSeen) ? GxDiagPass : GxDiagFail, TRUE, EndSeen ? "bounded walk found end marker" : "invalid/truncated list");
  } else {
    GxDxeLogHobSerialOnly (FoundHob ? "PREBOOT HOB FAIL: configuration table pointer is NULL" : "PREBOOT HOB FAIL: configuration table missing");
    GxDiagAdd ("HOB list", GxDiagFail, TRUE, "HOB configuration table missing");
  }

  MapSize = 0; DescriptorSize = 0; MapKey = 0; DescriptorVersion = 0;
  Status = gBS->GetMemoryMap (&MapSize, NULL, &MapKey, &DescriptorSize, &DescriptorVersion);
  Map = NULL;
  if (Status == EFI_BUFFER_TOO_SMALL && DescriptorSize >= sizeof (*Map)) {
    MapSize += DescriptorSize * 4;
    Status = CoreAllocatePool (EfiBootServicesData, MapSize, (VOID **)&Map);
    if (!EFI_ERROR (Status)) Status = gBS->GetMemoryMap (&MapSize, Map, &MapKey, &DescriptorSize, &DescriptorVersion);
  }
  if (!EFI_ERROR (Status) && (DescriptorSize >= sizeof (*Map)) && (MapSize % DescriptorSize == 0)) {
    BOOLEAN Valid = TRUE;
    for (Index = 0; Index < MapSize / DescriptorSize; Index++) {
      EFI_MEMORY_DESCRIPTOR *Desc = (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)Map + Index * DescriptorSize);
      UINTN Other;
      UINT64 Length;
      if ((Desc->NumberOfPages == 0) || (Desc->NumberOfPages > (~(UINT64)0 / EFI_PAGE_SIZE)) ||
          !G4delDiagRangeValid (Desc->PhysicalStart, Desc->NumberOfPages * EFI_PAGE_SIZE)) { Valid = FALSE; break; }
      Length = Desc->NumberOfPages * EFI_PAGE_SIZE;
      for (Other = 0; Other < Index; Other++) {
        EFI_MEMORY_DESCRIPTOR *Previous = (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)Map + Other * DescriptorSize);
        UINT64 PreviousLength = Previous->NumberOfPages * EFI_PAGE_SIZE;
        if (G4delDiagRangesOverlap (Desc->PhysicalStart, Length, Previous->PhysicalStart, PreviousLength)) { Valid = FALSE; break; }
      }
      if (!Valid) break;
    }
    GxDiagAdd ("Memory map", Valid ? GxDiagPass : GxDiagFail, TRUE, "descriptor ranges and lengths checked");
  } else GxDiagAdd ("Memory map", GxDiagFail, TRUE, "GetMemoryMap failed or malformed descriptors");
  if (Map != NULL) CoreFreePool (Map);

  FoundAcpi = FALSE;
  {
    EFI_PHYSICAL_ADDRESS RootAddress = 0;
    BOOLEAN RootValid = FALSE;
    UINT8 *Rsdp = NULL;
  for (Index = 0; Index < gDxeCoreST->NumberOfTableEntries; Index++) {
    Table = &gDxeCoreST->ConfigurationTable[Index];
    if (CompareGuid (&Table->VendorGuid, &AcpiGuid)) {
      Rsdp = Table->VendorTable;
      FoundAcpi = (Rsdp != NULL) && (AsciiStrnCmp ((CHAR8 *)Rsdp, "RSD PTR ", 8) == 0) && G4delDiagChecksumValid (Rsdp, 20);
      if (FoundAcpi && (Rsdp[15] >= 2)) {
        UINT32 RsdpLength;
        CopyMem (&RsdpLength, Rsdp + 20, sizeof (RsdpLength));
        FoundAcpi = (RsdpLength >= 36) && (RsdpLength <= 4096) && G4delDiagChecksumValid (Rsdp, RsdpLength);
      }
      if (FoundAcpi) {
        if (Rsdp[15] >= 2) CopyMem (&RootAddress, Rsdp + 24, sizeof (RootAddress));
        else { UINT32 RsdtAddress; CopyMem (&RsdtAddress, Rsdp + 16, sizeof (RsdtAddress)); RootAddress = RsdtAddress; }
      }
      break;
    }
  }
  GxDiagAdd ("ACPI RSDP", FoundAcpi ? GxDiagPass : GxDiagFail, TRUE, FoundAcpi ? "signature/checksum valid" : "missing or checksum invalid");
    if (FoundAcpi && (RootAddress != 0)) {
      typedef struct { UINT32 Signature; UINT32 Length; UINT8 Revision; UINT8 Checksum; UINT8 OemId[6]; UINT8 OemTableId[8]; UINT32 OemRevision; UINT32 CreatorId; UINT32 CreatorRevision; } GX_ACPI_HEADER;
      GX_ACPI_HEADER *Root = (GX_ACPI_HEADER *)(UINTN)RootAddress;
      UINT32 ExpectedSignature = (Rsdp[15] >= 2) ? SIGNATURE_32 ('X','S','D','T') : SIGNATURE_32 ('R','S','D','T');
      RootValid = (Root->Signature == ExpectedSignature) && (Root->Length >= sizeof (*Root)) &&
                  (Root->Length <= 1024 * 1024) && G4delDiagChecksumValid ((UINT8 *)Root, Root->Length);
    }
    GxDiagAdd ("ACPI root", RootValid ? GxDiagPass : GxDiagFail, TRUE, RootValid ? "XSDT/RSDT signature and checksum valid" : "root table missing or malformed");
  }

  {
    EFI_MP_SERVICES_PROTOCOL *Mp;
    UINTN CpuCount = 0, EnabledCpuCount = 0;
    Status = gBS->LocateProtocol (&MpGuid, NULL, (VOID **)&Mp);
    if (!EFI_ERROR (Status) && (Mp != NULL)) Status = Mp->GetNumberOfProcessors (Mp, &CpuCount, &EnabledCpuCount);
    GxDiagAdd ("CPU status",
      (EFI_ERROR (Status) || (CpuCount == 0) || (EnabledCpuCount == 0)) ? GxDiagWarn : GxDiagPass,
      FALSE, EFI_ERROR (Status) ? "MP services unavailable; BSP remains active" :
      ((CpuCount == 0) || (EnabledCpuCount == 0)) ? "processor count unavailable; check platform CPU status" : "processor topology query succeeded");
  }

  Handles = NULL; HandleCount = 0;
  Status = gBS->LocateHandleBuffer (ByProtocol, &GopGuid, NULL, &HandleCount, &Handles);
  if (!EFI_ERROR (Status) && (HandleCount != 0) && !EFI_ERROR (gBS->HandleProtocol (Handles[0], &GopGuid, (VOID **)&Gop)) &&
      (Gop != NULL) && (Gop->Mode != NULL) && (Gop->Mode->Info != NULL) &&
      G4delDiagFramebufferValid (Gop->Mode->FrameBufferBase, Gop->Mode->FrameBufferSize,
        Gop->Mode->Info->HorizontalResolution, Gop->Mode->Info->VerticalResolution,
        Gop->Mode->Info->PixelsPerScanLine, sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL))) {
    GxDiagAdd ("GOP", GxDiagPass, FALSE, "mode and framebuffer bounds valid");
  } else GxDiagAdd ("GOP", GxDiagWarn, FALSE, "no GOP or invalid framebuffer mode");
  if (Handles != NULL) CoreFreePool (Handles);
  Handles = NULL; HandleCount = 0;
  Status = gBS->LocateHandleBuffer (ByProtocol, &BlockGuid, NULL, &HandleCount, &Handles);
  GxDiagAdd ("Block I/O", (!EFI_ERROR (Status) && HandleCount != 0) ? GxDiagPass : GxDiagWarn, FALSE, (!EFI_ERROR (Status) && HandleCount != 0) ? "storage handles present" : "no block devices");
  if (Handles != NULL) CoreFreePool (Handles);
  Handles = NULL; HandleCount = 0;
  Status = gBS->LocateHandleBuffer (ByProtocol, &FsGuid, NULL, &HandleCount, &Handles);
  GxDiagAdd ("File systems", (!EFI_ERROR (Status) && HandleCount != 0) ? GxDiagPass : GxDiagWarn, FALSE, (!EFI_ERROR (Status) && HandleCount != 0) ? "filesystem handles present" : "no filesystem handles");
  if (Handles != NULL) CoreFreePool (Handles);
  GxDiagAdd ("UEFI consoles", (gDxeCoreST->ConOut != NULL && gDxeCoreST->ConIn != NULL) ? GxDiagPass : GxDiagFail, TRUE, "ConIn/ConOut pointers checked");

  BootOrderSize = 0;
  Status = gDxeCoreST->RuntimeServices->GetVariable (L"BootOrder", &gEfiGlobalVariableGuid, NULL, &BootOrderSize, NULL);
  if (Status == EFI_BUFFER_TOO_SMALL && (BootOrderSize != 0) && ((BootOrderSize % sizeof (UINT16)) == 0)) {
    UINT16 *BootOrder;
    Required = BootOrderSize;
    Status = CoreAllocatePool (EfiBootServicesData, Required, (VOID **)&BootOrder);
    if (!EFI_ERROR (Status)) {
      Status = gDxeCoreST->RuntimeServices->GetVariable (L"BootOrder", &gEfiGlobalVariableGuid, NULL, &Required, BootOrder);
      if (!EFI_ERROR (Status) && (Required % sizeof (UINT16) == 0)) {
        CHAR8 BootMessage[96];

        GxDiagAdd ("Boot paths", (Required != 0) ? GxDiagPass : GxDiagWarn, FALSE, "BootOrder entries readable");
        for (Index = 0; Index < (Required / sizeof (UINT16)); Index++) {
          AsciiSPrint (BootMessage, sizeof (BootMessage), "PREBOOT BootOrder[%lu]=Boot%04x",
            (UINT64)Index, BootOrder[Index]);
          GxDxeLogSerialOnly (BootMessage);
        }
      } else GxDiagAdd ("Boot paths", GxDiagWarn, FALSE, "BootOrder could not be read");
      CoreFreePool (BootOrder);
    } else GxDiagAdd ("Boot paths", GxDiagWarn, FALSE, "BootOrder allocation failed");
  } else GxDiagAdd ("Boot paths", GxDiagWarn, FALSE, "BootOrder absent or malformed");

  if ((Category != NULL) && (AsciiStrCmp (Category, "all") != 0)) {
    UINTN ReadIndex, WriteIndex;
    CONST CHAR8 *Wanted = Category;
    BOOLEAN CategoryValid = (AsciiStrCmp (Wanted, "hob") == 0) || (AsciiStrCmp (Wanted, "memory") == 0) ||
      (AsciiStrCmp (Wanted, "acpi") == 0) || (AsciiStrCmp (Wanted, "cpu") == 0) || (AsciiStrCmp (Wanted, "graphics") == 0) ||
      (AsciiStrCmp (Wanted, "console") == 0) || (AsciiStrCmp (Wanted, "storage") == 0) || (AsciiStrCmp (Wanted, "boot") == 0);
    if (!CategoryValid) return EFI_INVALID_PARAMETER;
    WriteIndex = 0;
    for (ReadIndex = 0; ReadIndex < mGxDiagCheckCount; ReadIndex++) {
      GX_DIAG_CHECK *Check = &mGxDiagChecks[ReadIndex];
      BOOLEAN Keep = ((AsciiStrCmp (Wanted, "hob") == 0) && (AsciiStrCmp (Check->Name, "HOB list") == 0)) ||
        ((AsciiStrCmp (Wanted, "memory") == 0) && (AsciiStrCmp (Check->Name, "Memory map") == 0)) ||
        ((AsciiStrCmp (Wanted, "acpi") == 0) && (AsciiStrCmp (Check->Name, "ACPI RSDP") == 0)) ||
        ((AsciiStrCmp (Wanted, "acpi") == 0) && (AsciiStrCmp (Check->Name, "ACPI root") == 0)) ||
        ((AsciiStrCmp (Wanted, "memory") == 0) && (AsciiStrCmp (Check->Name, "PEI memory init") == 0)) ||
        ((AsciiStrCmp (Wanted, "cpu") == 0) && (AsciiStrCmp (Check->Name, "CPU status") == 0)) ||
        ((AsciiStrCmp (Wanted, "graphics") == 0) && (AsciiStrCmp (Check->Name, "GOP") == 0)) ||
        ((AsciiStrCmp (Wanted, "console") == 0) && (AsciiStrCmp (Check->Name, "UEFI consoles") == 0)) ||
        ((AsciiStrCmp (Wanted, "storage") == 0) && ((AsciiStrCmp (Check->Name, "Block I/O") == 0) || (AsciiStrCmp (Check->Name, "File systems") == 0))) ||
        ((AsciiStrCmp (Wanted, "boot") == 0) && (AsciiStrCmp (Check->Name, "Boot paths") == 0));
      if (Keep) { if (WriteIndex != ReadIndex) CopyMem (&mGxDiagChecks[WriteIndex], Check, sizeof (*Check)); WriteIndex++; }
    }
    mGxDiagCheckCount = WriteIndex;
  }
  return EFI_SUCCESS;
}

STATIC EFI_STATUS EFIAPI
GxDxeDiagnosticsGetCheck (IN UINTN Index, OUT GX_DIAG_CHECK *Check)
{
  if ((Check == NULL) || (Index >= mGxDiagCheckCount)) return EFI_NOT_FOUND;
  CopyMem (Check, &mGxDiagChecks[Index], sizeof (*Check));
  return EFI_SUCCESS;
}

STATIC EFI_STATUS EFIAPI
GxDxeDiagnosticsGetSummary (OUT UINTN *Count, OUT UINTN *Failures, OUT UINTN *Warnings)
{
  UINTN Index;
  if ((Count == NULL) || (Failures == NULL) || (Warnings == NULL)) return EFI_INVALID_PARAMETER;
  *Count = mGxDiagCheckCount; *Failures = 0; *Warnings = 0;
  for (Index = 0; Index < mGxDiagCheckCount; Index++) {
    if (mGxDiagChecks[Index].Result == GxDiagFail) (*Failures)++;
    if (mGxDiagChecks[Index].Result == GxDiagWarn) (*Warnings)++;
  }
  return EFI_SUCCESS;
}

/**
  Given a compressed source buffer, this function retrieves the size of the
  uncompressed buffer and the size of the scratch buffer required to decompress
  the compressed source buffer.

  The GetInfo() function retrieves the size of the uncompressed buffer and the
  temporary scratch buffer required to decompress the buffer specified by Source
  and SourceSize. If the size of the uncompressed buffer or the size of the
  scratch buffer cannot be determined from the compressed data specified by
  Source and SourceData, then EFI_INVALID_PARAMETER is returned. Otherwise, the
  size of the uncompressed buffer is returned in DestinationSize, the size of
  the scratch buffer is returned in ScratchSize, and EFI_SUCCESS is returned.
  The GetInfo() function does not have scratch buffer available to perform a
  thorough checking of the validity of the source data. It just retrieves the
  "Original Size" field from the beginning bytes of the source data and output
  it as DestinationSize. And ScratchSize is specific to the decompression
  implementation.

  @param  This               A pointer to the EFI_DECOMPRESS_PROTOCOL instance.
  @param  Source             The source buffer containing the compressed data.
  @param  SourceSize         The size, in bytes, of the source buffer.
  @param  DestinationSize    A pointer to the size, in bytes, of the
                             uncompressed buffer that will be generated when the
                             compressed buffer specified by Source and
                             SourceSize is decompressed.
  @param  ScratchSize        A pointer to the size, in bytes, of the scratch
                             buffer that is required to decompress the
                             compressed buffer specified by Source and
                             SourceSize.

  @retval EFI_SUCCESS        The size of the uncompressed data was returned in
                             DestinationSize and the size of the scratch buffer
                             was returned in ScratchSize.
  @retval EFI_INVALID_PARAMETER The size of the uncompressed data or the size of
                                the scratch buffer cannot be determined from the
                                compressed data specified by Source and
                                SourceSize.

**/
EFI_STATUS
EFIAPI
DxeMainUefiDecompressGetInfo (
  IN EFI_DECOMPRESS_PROTOCOL  *This,
  IN   VOID                   *Source,
  IN   UINT32                 SourceSize,
  OUT  UINT32                 *DestinationSize,
  OUT  UINT32                 *ScratchSize
  )
{
  if ((Source == NULL) || (DestinationSize == NULL) || (ScratchSize == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  return UefiDecompressGetInfo (Source, SourceSize, DestinationSize, ScratchSize);
}

/**
  Decompresses a compressed source buffer.

  The Decompress() function extracts decompressed data to its original form.
  This protocol is designed so that the decompression algorithm can be
  implemented without using any memory services. As a result, the Decompress()
  Function is not allowed to call AllocatePool() or AllocatePages() in its
  implementation. It is the caller's responsibility to allocate and free the
  Destination and Scratch buffers.
  If the compressed source data specified by Source and SourceSize is
  successfully decompressed into Destination, then EFI_SUCCESS is returned. If
  the compressed source data specified by Source and SourceSize is not in a
  valid compressed data format, then EFI_INVALID_PARAMETER is returned.

  @param  This                A pointer to the EFI_DECOMPRESS_PROTOCOL instance.
  @param  Source              The source buffer containing the compressed data.
  @param  SourceSize          SourceSizeThe size of source data.
  @param  Destination         On output, the destination buffer that contains
                              the uncompressed data.
  @param  DestinationSize     The size of the destination buffer.  The size of
                              the destination buffer needed is obtained from
                              EFI_DECOMPRESS_PROTOCOL.GetInfo().
  @param  Scratch             A temporary scratch buffer that is used to perform
                              the decompression.
  @param  ScratchSize         The size of scratch buffer. The size of the
                              scratch buffer needed is obtained from GetInfo().

  @retval EFI_SUCCESS         Decompression completed successfully, and the
                              uncompressed buffer is returned in Destination.
  @retval EFI_INVALID_PARAMETER  The source buffer specified by Source and
                                 SourceSize is corrupted (not in a valid
                                 compressed format).

**/
EFI_STATUS
EFIAPI
DxeMainUefiDecompress (
  IN     EFI_DECOMPRESS_PROTOCOL  *This,
  IN     VOID                     *Source,
  IN     UINT32                   SourceSize,
  IN OUT VOID                     *Destination,
  IN     UINT32                   DestinationSize,
  IN OUT VOID                     *Scratch,
  IN     UINT32                   ScratchSize
  )
{
  EFI_STATUS  Status;
  UINT32      TestDestinationSize;
  UINT32      TestScratchSize;

  if ((Source == NULL) || (Destination == NULL) || (Scratch == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = UefiDecompressGetInfo (Source, SourceSize, &TestDestinationSize, &TestScratchSize);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if ((ScratchSize < TestScratchSize) || (DestinationSize < TestDestinationSize)) {
    return RETURN_INVALID_PARAMETER;
  }

  return UefiDecompress (Source, Destination, Scratch);
}
