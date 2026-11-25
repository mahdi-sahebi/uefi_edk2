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
  return (BOOLEAN)(G4delDiagHobHeaderReason (Type, Length, Remaining, EndType, HeaderSize) == G4delDiagHobHeaderValidReason);
}

G4DEL_DIAG_HOB_HEADER_REASON
G4delDiagHobHeaderReason (
  G4DEL_UINT16  Type,
  G4DEL_UINT16  Length,
  G4DEL_UINTN   Remaining,
  G4DEL_UINT16  EndType,
  G4DEL_UINT16  HeaderSize
  )
{
  if (Length < HeaderSize) {
    return G4delDiagHobHeaderTooShort;
  }

  if (Length > Remaining) {
    return G4delDiagHobHeaderExceedsRemaining;
  }

  if ((Type == EndType) && (Length != HeaderSize)) {
    return G4delDiagHobEndMarkerWrongLength;
  }

  return G4delDiagHobHeaderValidReason;
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
G4delDiagFramebufferIdentityMatches (
  G4DEL_UINT64  BaseA,
  G4DEL_UINT64  SizeA,
  G4DEL_UINT32  WidthA,
  G4DEL_UINT32  HeightA,
  G4DEL_UINT32  PitchA,
  G4DEL_UINT32  FormatA,
  G4DEL_UINT64  BaseB,
  G4DEL_UINT64  SizeB,
  G4DEL_UINT32  WidthB,
  G4DEL_UINT32  HeightB,
  G4DEL_UINT32  PitchB,
  G4DEL_UINT32  FormatB
  )
{
  return (BOOLEAN)(
    (BaseA == BaseB) && (SizeA == SizeB) &&
    (WidthA == WidthB) && (HeightA == HeightB) &&
    (PitchA == PitchB) && (FormatA == FormatB) &&
    G4delDiagFramebufferValid (BaseA, SizeA, WidthA, HeightA, PitchA, 4) &&
    G4delDiagFramebufferValid (BaseB, SizeB, WidthB, HeightB, PitchB, 4));
}

BOOLEAN
G4delDiagBootAllowed (
  G4DEL_UINTN  FailureCount,
  BOOLEAN      OneTimeOverride
  )
{
  return (BOOLEAN)((FailureCount == 0) || OneTimeOverride);
}
