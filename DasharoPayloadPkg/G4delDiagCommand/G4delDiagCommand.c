#include <Uefi.h>
#include <Protocol/ShellDynamicCommand.h>
#include <Protocol/GxDxeDiagnostics.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>

STATIC EFI_SHELL_DYNAMIC_COMMAND_PROTOCOL mG4delDiagCommand;

STATIC SHELL_STATUS EFIAPI
G4delDiagHandler (IN EFI_SHELL_DYNAMIC_COMMAND_PROTOCOL *This, IN EFI_SYSTEM_TABLE *SystemTable,
                  IN EFI_SHELL_PARAMETERS_PROTOCOL *ShellParameters, IN EFI_SHELL_PROTOCOL *Shell)
{
  EFI_STATUS Status;
  GX_DXE_DIAGNOSTICS_PROTOCOL *Diag;
  GX_DIAG_CHECK Check;
  UINTN Index, Count, Failures, Warnings;
  CONST CHAR8 *Category = NULL;
  (VOID)This; (VOID)SystemTable; (VOID)Shell;
  Status = gBS->LocateProtocol (&gGxDxeDiagnosticsProtocolGuid, NULL, (VOID **)&Diag);
  if (EFI_ERROR (Status)) { Print (L"g4diag: diagnostics protocol unavailable: %r\n", Status); return SHELL_NOT_FOUND; }
  if (ShellParameters->Argc > 1) {
    if ((ShellParameters->Argc < 3) || (StrCmp (ShellParameters->Argv[1], L"run") != 0)) {
      Print (L"Usage: g4diag [run [all|hob|memory|acpi|cpu|graphics|console|storage|boot]]\n"); return SHELL_INVALID_PARAMETER;
    }
    if (StrCmp (ShellParameters->Argv[2], L"all") != 0) {
      Category = AllocatePool ((StrLen (ShellParameters->Argv[2]) + 1) * sizeof (CHAR8));
      if (Category == NULL) return SHELL_OUT_OF_RESOURCES;
      for (Index = 0; ShellParameters->Argv[2][Index] != L'\0'; Index++) ((CHAR8 *)Category)[Index] = (CHAR8)ShellParameters->Argv[2][Index];
      ((CHAR8 *)Category)[Index] = '\0';
    }
    Status = Diag->RunChecks (Category);
    if (Category != NULL) FreePool ((VOID *)Category);
    if (EFI_ERROR (Status)) { Print (L"g4diag: check execution failed: %r\n", Status); return SHELL_ABORTED; }
  }
  Diag->GetSummary (&Count, &Failures, &Warnings);
  Print (L"G4DEL preboot diagnostics: %u checks, %u failures, %u warnings\n", Count, Failures, Warnings);
  for (Index = 0; Index < Count; Index++) {
    if (EFI_ERROR (Diag->GetCheck (Index, &Check))) continue;
    Print (L"[%a] %a: %a%s\n", Check.Result == GxDiagPass ? "PASS" : Check.Result == GxDiagWarn ? "WARN" : "FAIL",
           Check.Name, Check.Detail, Check.Required ? " [required]" : "");
  }
  return Failures == 0 ? SHELL_SUCCESS : SHELL_ABORTED;
}

STATIC CHAR16 *EFIAPI G4delDiagHelp (IN EFI_SHELL_DYNAMIC_COMMAND_PROTOCOL *This, IN CONST CHAR8 *Language)
{
  (VOID)This; (VOID)Language;
  return AllocateCopyPool (sizeof (L"Display saved preboot checks; use g4diag run [all|category]."), L"Display saved preboot checks; use g4diag run [all|category].");
}

STATIC EFI_SHELL_DYNAMIC_COMMAND_PROTOCOL mG4delDiagCommand = { L"g4diag", G4delDiagHandler, G4delDiagHelp };

EFI_STATUS EFIAPI G4delDiagCommandEntry (IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
  (VOID)SystemTable;
  return gBS->InstallProtocolInterface (&ImageHandle, &gEfiShellDynamicCommandProtocolGuid, EFI_NATIVE_INTERFACE, &mG4delDiagCommand);
}
