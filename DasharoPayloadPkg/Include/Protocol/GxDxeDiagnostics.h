/** @file
  Internal DXE diagnostic protocol shared by DxeCore and payload drivers.
**/

#ifndef GX_DXE_DIAGNOSTICS_PROTOCOL_H_
#define GX_DXE_DIAGNOSTICS_PROTOCOL_H_

#include <Uefi.h>

typedef struct _GX_DXE_DIAGNOSTICS_PROTOCOL GX_DXE_DIAGNOSTICS_PROTOCOL;

typedef
VOID
(EFIAPI *GX_DXE_DIAGNOSTICS_LOG)(
  IN CONST CHAR8  *Message
  );

struct _GX_DXE_DIAGNOSTICS_PROTOCOL {
  GX_DXE_DIAGNOSTICS_LOG  Log;
};

extern EFI_GUID  gGxDxeDiagnosticsProtocolGuid;

#endif
