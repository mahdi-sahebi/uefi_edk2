/** @file
  Internal DXE diagnostic protocol shared by DxeCore and payload drivers.
**/

#ifndef GX_DXE_DIAGNOSTICS_PROTOCOL_H_
#define GX_DXE_DIAGNOSTICS_PROTOCOL_H_

#include <Uefi.h>

typedef struct _GX_DXE_DIAGNOSTICS_PROTOCOL GX_DXE_DIAGNOSTICS_PROTOCOL;

#define GX_DIAG_MAX_CHECKS  16
#define GX_DIAG_NAME_SIZE   24
#define GX_DIAG_DETAIL_SIZE 80

typedef enum { GxDiagPass = 0, GxDiagWarn = 1, GxDiagFail = 2 } GX_DIAG_RESULT;
typedef struct {
  CHAR8          Name[GX_DIAG_NAME_SIZE];
  CHAR8          Detail[GX_DIAG_DETAIL_SIZE];
  GX_DIAG_RESULT Result;
  BOOLEAN        Required;
} GX_DIAG_CHECK;

typedef struct {
  EFI_GUID   FileName;
  BOOLEAN    Discovered;
  BOOLEAN    Initialized;
  BOOLEAN    Scheduled;
  BOOLEAN    Dependent;
  BOOLEAN    Unrequested;
  BOOLEAN    Untrusted;
  BOOLEAN    DepexProtocolError;
  BOOLEAN    LoadAttempted;
  BOOLEAN    ImageLoaded;
  EFI_STATUS LoadStatus;
  BOOLEAN    StartAttempted;
  EFI_STATUS StartStatus;
} GX_DXE_DRIVER_STATUS;

typedef
VOID
(EFIAPI *GX_DXE_DIAGNOSTICS_LOG)(
  IN CONST CHAR8  *Message
  );

typedef
EFI_STATUS
(EFIAPI *GX_DXE_DIAGNOSTICS_GET_DRIVER_STATUS)(
  IN  CONST EFI_GUID          *FileName,
  OUT GX_DXE_DRIVER_STATUS    *DriverStatus
  );

typedef
EFI_STATUS
(EFIAPI *GX_DXE_DIAGNOSTICS_SET_SHELL_LOAD_TRACE)(
  IN BOOLEAN  Active
  );

typedef EFI_STATUS (EFIAPI *GX_DXE_DIAGNOSTICS_RUN_CHECKS)(IN CONST CHAR8 *Category);
typedef EFI_STATUS (EFIAPI *GX_DXE_DIAGNOSTICS_GET_CHECK)(IN UINTN Index, OUT GX_DIAG_CHECK *Check);
typedef EFI_STATUS (EFIAPI *GX_DXE_DIAGNOSTICS_GET_SUMMARY)(OUT UINTN *Count, OUT UINTN *Failures, OUT UINTN *Warnings);

struct _GX_DXE_DIAGNOSTICS_PROTOCOL {
  GX_DXE_DIAGNOSTICS_LOG                  Log;
  GX_DXE_DIAGNOSTICS_GET_DRIVER_STATUS    GetDriverStatus;
  GX_DXE_DIAGNOSTICS_SET_SHELL_LOAD_TRACE SetShellLoadTrace;
  GX_DXE_DIAGNOSTICS_RUN_CHECKS          RunChecks;
  GX_DXE_DIAGNOSTICS_GET_CHECK           GetCheck;
  GX_DXE_DIAGNOSTICS_GET_SUMMARY         GetSummary;
};

extern EFI_GUID  gGxDxeDiagnosticsProtocolGuid;

#endif
