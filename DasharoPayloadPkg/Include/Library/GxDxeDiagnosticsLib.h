/** @file
  Convenience wrapper for the internal DXE diagnostic protocol.
**/

#ifndef GX_DXE_DIAGNOSTICS_LIB_H_
#define GX_DXE_DIAGNOSTICS_LIB_H_

#include <Protocol/GxDxeDiagnostics.h>
#include <Library/UefiBootServicesTableLib.h>

STATIC
VOID
GxDxeDiagnosticsLog (
  IN CONST CHAR8  *Message
  )
{
  EFI_STATUS                   Status;
  GX_DXE_DIAGNOSTICS_PROTOCOL  *Diagnostics;

  Status = gBS->LocateProtocol (&gGxDxeDiagnosticsProtocolGuid, NULL, (VOID **)&Diagnostics);
  if (!EFI_ERROR (Status) && (Diagnostics != NULL) && (Diagnostics->Log != NULL)) {
    Diagnostics->Log (Message);
  }
}

STATIC inline
VOID
GxDxeDiagnosticsSetShellLoadTrace (
  IN BOOLEAN  Active
  )
{
  EFI_STATUS                   Status;
  GX_DXE_DIAGNOSTICS_PROTOCOL  *Diagnostics;

  Status = gBS->LocateProtocol (&gGxDxeDiagnosticsProtocolGuid, NULL, (VOID **)&Diagnostics);
  if (!EFI_ERROR (Status) && (Diagnostics != NULL) && (Diagnostics->SetShellLoadTrace != NULL)) {
    Diagnostics->SetShellLoadTrace (Active);
  }
}

#endif
