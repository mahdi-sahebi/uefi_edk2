/** @file
  Portable range, HOB, checksum, framebuffer and gate checks.
**/

#include <Library/G4delDiagnosticChecks.h>

BOOLEAN
G4delDiagRangeValid (
  G4DEL_UINT64  Base,
  G4DEL_UINT64  Length
  )
{
  return (BOOLEAN)((Length != 0) && (Base <= (~(G4DEL_UINT64)0 - Length)));
}

BOOLEAN
G4delDiagRangesOverlap (
  G4DEL_UINT64  BaseA,
  G4DEL_UINT64  LengthA,
  G4DEL_UINT64  BaseB,
  G4DEL_UINT64  LengthB
  )
{
  if (!G4delDiagRangeValid (BaseA, LengthA) || !G4delDiagRangeValid (BaseB, LengthB)) {
    return TRUE;
  }

  return (BOOLEAN)((BaseA < (BaseB + LengthB)) && (BaseB < (BaseA + LengthA)));
}

BOOLEAN
G4delDiagHobHeaderValid (
  G4DEL_UINT16  Type,
  G4DEL_UINT16  Length,
  G4DEL_UINTN   Remaining,
  G4DEL_UINT16  EndType,
  G4DEL_UINT16  HeaderSize
  )
{
  if ((Length < HeaderSize) || (Length > Remaining)) {
    return FALSE;
  }

  if ((Type == EndType) && (Length != HeaderSize)) {
    return FALSE;
  }

  return TRUE;
}

BOOLEAN
G4delDiagChecksumValid (
  const G4DEL_UINT8  *Bytes,
  G4DEL_UINTN        Length
  )
{
  G4DEL_UINTN  Index;
  G4DEL_UINT8  Sum;

  if ((Bytes == NULL) || (Length == 0)) {
    return FALSE;
  }

  Sum = 0;
  for (Index = 0; Index < Length; Index++) {
    Sum = (G4DEL_UINT8)(Sum + Bytes[Index]);
  }

  return (BOOLEAN)(Sum == 0);
}

BOOLEAN
G4delDiagFramebufferValid (
  G4DEL_UINT64  Base,
  G4DEL_UINT64  Size,
  G4DEL_UINT32  Width,
  G4DEL_UINT32  Height,
  G4DEL_UINT32  PixelsPerScanLine,
  G4DEL_UINT32  BytesPerPixel
  )
{
  G4DEL_UINT64  RowBytes;
  G4DEL_UINT64  RequiredBytes;

  if (!G4delDiagRangeValid (Base, Size) || (Width == 0) || (Height == 0) ||
      (PixelsPerScanLine < Width) || (BytesPerPixel == 0))
  {
    return FALSE;
  }

  if ((G4DEL_UINT64)PixelsPerScanLine > (~(G4DEL_UINT64)0 / BytesPerPixel)) {
    return FALSE;
  }

  RowBytes = (G4DEL_UINT64)PixelsPerScanLine * BytesPerPixel;
  if (RowBytes > (~(G4DEL_UINT64)0 / Height)) {
    return FALSE;
  }

  RequiredBytes = RowBytes * Height;
  return (BOOLEAN)(RequiredBytes <= Size);
}

BOOLEAN
G4delDiagBootAllowed (
  G4DEL_UINTN  FailureCount,
  BOOLEAN      OneTimeOverride
  )
{
  return (BOOLEAN)((FailureCount == 0) || OneTimeOverride);
}
