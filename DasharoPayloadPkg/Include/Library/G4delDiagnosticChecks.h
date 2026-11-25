/** @file
  Portable validation helpers shared by G4DEL diagnostics and host tests.
**/

#ifndef G4DEL_DIAGNOSTIC_CHECKS_H_
#define G4DEL_DIAGNOSTIC_CHECKS_H_

#if defined (G4DEL_DIAGNOSTIC_HOST_TEST)
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef uint64_t  G4DEL_UINT64;
typedef uint32_t  G4DEL_UINT32;
typedef uint16_t  G4DEL_UINT16;
typedef uint8_t   G4DEL_UINT8;
typedef size_t    G4DEL_UINTN;
typedef bool      BOOLEAN;
#define TRUE  true
#define FALSE false
#else
#include <Uefi.h>
typedef UINT64  G4DEL_UINT64;
typedef UINT32  G4DEL_UINT32;
typedef UINT16  G4DEL_UINT16;
typedef UINT8   G4DEL_UINT8;
typedef UINTN   G4DEL_UINTN;
#endif

BOOLEAN
G4delDiagRangeValid (
  G4DEL_UINT64  Base,
  G4DEL_UINT64  Length
  );

BOOLEAN
G4delDiagRangesOverlap (
  G4DEL_UINT64  BaseA,
  G4DEL_UINT64  LengthA,
  G4DEL_UINT64  BaseB,
  G4DEL_UINT64  LengthB
  );

BOOLEAN
G4delDiagHobHeaderValid (
  G4DEL_UINT16  Type,
  G4DEL_UINT16  Length,
  G4DEL_UINTN   Remaining,
  G4DEL_UINT16  EndType,
  G4DEL_UINT16  HeaderSize
  );

typedef enum {
  G4delDiagHobHeaderValidReason,
  G4delDiagHobHeaderTooShort,
  G4delDiagHobHeaderExceedsRemaining,
  G4delDiagHobEndMarkerWrongLength
} G4DEL_DIAG_HOB_HEADER_REASON;

G4DEL_DIAG_HOB_HEADER_REASON
G4delDiagHobHeaderReason (
  G4DEL_UINT16  Type,
  G4DEL_UINT16  Length,
  G4DEL_UINTN   Remaining,
  G4DEL_UINT16  EndType,
  G4DEL_UINT16  HeaderSize
  );

BOOLEAN
G4delDiagChecksumValid (
  const G4DEL_UINT8  *Bytes,
  G4DEL_UINTN        Length
  );

BOOLEAN
G4delDiagFramebufferValid (
  G4DEL_UINT64  Base,
  G4DEL_UINT64  Size,
  G4DEL_UINT32  Width,
  G4DEL_UINT32  Height,
  G4DEL_UINT32  PixelsPerScanLine,
  G4DEL_UINT32  BytesPerPixel
  );

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
  );

BOOLEAN
G4delDiagBootAllowed (
  G4DEL_UINTN  FailureCount,
  BOOLEAN      OneTimeOverride
  );

#endif
