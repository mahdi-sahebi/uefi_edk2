/** @file
  Internal DXE diagnostic protocol shared by DxeCore and payload drivers.
**/

#ifndef GX_DXE_DIAGNOSTICS_PROTOCOL_H_
#define GX_DXE_DIAGNOSTICS_PROTOCOL_H_

#include <Uefi.h>

typedef struct _GX_DXE_DIAGNOSTICS_PROTOCOL GX_DXE_DIAGNOSTICS_PROTOCOL;

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

struct _GX_DXE_DIAGNOSTICS_PROTOCOL {
  GX_DXE_DIAGNOSTICS_LOG                  Log;
  GX_DXE_DIAGNOSTICS_GET_DRIVER_STATUS    GetDriverStatus;
};

extern EFI_GUID  gGxDxeDiagnosticsProtocolGuid;

#endif
