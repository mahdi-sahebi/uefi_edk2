/** @file
  C functions in SEC

Copyright (c) 2013, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/


#include "SecMain.h"
#include "version.h"

//////////////////////////// mahdi
#include "edkii_vga.h"
VOID EFIAPI SecSerialPortConfigure (IN UINT32 BootloaderParameter);
#include <Library/TimerLib.h>
#include <Library/SerialPortLib.h>
/////////////////////////////////

STATIC CONST CHAR8 mUefiCorebootVersionBanner[] =
  "[EDK2] UEFI_COREBOOT_VERSION=" UEFI_COREBOOT_VERSION_STRING
  " BUILD_DATE=" UEFI_COREBOOT_BUILD_DATE "\r\n";



EFI_PEI_TEMPORARY_RAM_SUPPORT_PPI gSecTemporaryRamSupportPpi = {
  SecTemporaryRamSupport
};

EFI_PEI_PPI_DESCRIPTOR            mPeiSecPlatformInformationPpi[] = {
  {
    (EFI_PEI_PPI_DESCRIPTOR_PPI | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST),
    &gEfiTemporaryRamSupportPpiGuid,
    &gSecTemporaryRamSupportPpi
  }
};

//
// These are IDT entries pointing to 10:FFFFFFE4h.
//
UINT64  mIdtEntryTemplate = 0xffff8e000010ffe4ULL;

/**
  Caller provided function to be invoked at the end of InitializeDebugAgent().

  Entry point to the C language phase of SEC. After the SEC assembly
  code has initialized some temporary memory and set up the stack,
  the control is transferred to this function.

  @param[in] Context    The first input parameter of InitializeDebugAgent().

**/
VOID
EFIAPI
SecStartupPhase2(
  IN VOID                     *Context
  );


/**

  Entry point to the C language phase of SEC. After the SEC assembly
  code has initialized some temporary memory and set up the stack,
  the control is transferred to this function.


  @param SizeOfRam           Size of the temporary memory available for use.
  @param TempRamBase         Base address of temporary ram
  @param BootFirmwareVolume  Base address of the Boot Firmware Volume.
  @param BootloaderParameter A parameter from bootloader, e.g. HobList from SlimBootloader

**/
VOID
EFIAPI
SecStartup (
  IN UINT32                   SizeOfRam,
  IN UINT32                   TempRamBase,
  IN VOID                     *BootFirmwareVolume,
  IN UINT32                   BootloaderParameter
  )
{
  //////////////////////////// mahdi
  SecSerialPortConfigure (BootloaderParameter);
  SerialPortWrite ((UINT8 *)mUefiCorebootVersionBanner, sizeof (mUefiCorebootVersionBanner) - 1);
  edkii_vga_configure_from_coreboot (BootloaderParameter);
  edkii_vga_clear();
  edkii_vga_print (0, "EDK II SEC ENTRY 1405-08-06");
  SerialPortWrite ((UINT8 *)"EDK II SEC ENTRY 1405-08-06\r\n", sizeof ("EDK II SEC ENTRY 1405-08-06\r\n") - 1);
  MicroSecondDelay (2000000);
  DEBUG ((DEBUG_ERROR, "[EDK2-DIAG] CHECKPOINT SEC entry arg=0x%x fv=%p stack=0x%x+0x%x\n",
    BootloaderParameter, BootFirmwareVolume, TempRamBase, SizeOfRam));
  ///////////////////////////
// MicroSecondDelay (5000000);
  EFI_SEC_PEI_HAND_OFF        SecCoreData;
  IA32_DESCRIPTOR             IdtDescriptor;
  SEC_IDT_TABLE               IdtTableInStack;
  UINT32                      Index;
  UINT32                      PeiStackSize;

  PeiStackSize = (SizeOfRam >> 1);

//////////////////////////////////////////
    // char buf[32];
// //edkii_vga_print(1, "Params:");

// AsciiSPrint(buf, sizeof(buf), "HDR: 0x%X", *(UINT32*)BootloaderParameter);
// //edkii_vga_print(2, buf);

// AsciiSPrint(buf, sizeof(buf), "SOR: 0x%X", SizeOfRam);
// //edkii_vga_print(3, buf);

// AsciiSPrint(buf, sizeof(buf), "TRB: 0x%X", TempRamBase);
// //edkii_vga_print(4, buf);

// AsciiSPrint(buf, sizeof(buf), "BFV: 0x%X", (UINT32)(UINTN)BootFirmwareVolume);
// //edkii_vga_print(5, buf);

// AsciiSPrint(buf, sizeof(buf), "BLP: 0x%X", BootloaderParameter);
// //edkii_vga_print(6, buf);


// // Also verify the coreboot table signature
// UINT32 *lb = (UINT32 *)BootloaderParameter;
// if (lb && lb[0] == 0x4F49424C) {  // "LBIO" in little-endian
//     //edkii_vga_print(7, "LBIO OK");
// } else {
//     //edkii_vga_print(7, "LBIO FAIL");
// }

// if (SizeOfRam == 0 || TempRamBase == 0) {
//     //edkii_vga_print(8, "ERROR: Invalid params!");
// }
///////////////////////////////////////////
  ASSERT (PeiStackSize < SizeOfRam);

  //
  // Process all libraries constructor function linked to SecCore.
  //
  // //edkii_vga_print(0, "EDK II Started - 1");
  DEBUG ((DEBUG_ERROR, "[EDK2-DIAG] CHECKPOINT SEC constructors begin\n"));
  ProcessLibraryConstructorList ();
  DEBUG ((DEBUG_ERROR, "[EDK2-DIAG] CHECKPOINT SEC constructors complete\n"));

  //
  // Initialize floating point operating environment
  // to be compliant with UEFI spec.
  //
  // //edkii_vga_print(0, "EDK II Started - 2");
  InitializeFloatingPointUnits ();


  // |-------------------|---->
  // |Idt Table          |
  // |-------------------|
  // |PeiService Pointer |    PeiStackSize
  // |-------------------|
  // |                   |
  // |      Stack        |
  // |-------------------|---->
  // |                   |
  // |                   |
  // |      Heap         |    PeiTemporaryRamSize
  // |                   |
  // |                   |
  // |-------------------|---->  TempRamBase
  // //edkii_vga_print(0, "EDK II Started - 3");

  IdtTableInStack.PeiService = 0;
  for (Index = 0; Index < SEC_IDT_ENTRY_COUNT; Index ++) {
    CopyMem ((VOID*)&IdtTableInStack.IdtTable[Index], (VOID*)&mIdtEntryTemplate, sizeof (UINT64));
  }
  // //edkii_vga_print(0, "EDK II Started - 4");

  IdtDescriptor.Base  = (UINTN) &IdtTableInStack.IdtTable;
  IdtDescriptor.Limit = (UINT16)(sizeof (IdtTableInStack.IdtTable) - 1);

  // //edkii_vga_print(0, "EDK II Started - 5");
  AsmWriteIdtr (&IdtDescriptor);
  // //edkii_vga_print(0, "EDK II Started - 6");

  AsmWriteIdtr (&IdtDescriptor);

  //
  // Update the base address and length of Pei temporary memory
  //
  SecCoreData.DataSize               = (UINT16) sizeof (EFI_SEC_PEI_HAND_OFF);
  SecCoreData.BootFirmwareVolumeBase = BootFirmwareVolume;
  SecCoreData.BootFirmwareVolumeSize = (UINTN)(0x100000000ULL - (UINTN) BootFirmwareVolume);
  SecCoreData.TemporaryRamBase       = (VOID*)(UINTN) TempRamBase;
  SecCoreData.TemporaryRamSize       = SizeOfRam;
  SecCoreData.PeiTemporaryRamBase    = SecCoreData.TemporaryRamBase;
  SecCoreData.PeiTemporaryRamSize    = SizeOfRam - PeiStackSize;
  SecCoreData.StackBase              = (VOID*)(UINTN)(TempRamBase + SecCoreData.PeiTemporaryRamSize);
  SecCoreData.StackSize              = PeiStackSize;

  //
  // Initialize Debug Agent to support source level debug in SEC/PEI phases before memory ready.
  //
  InitializeDebugAgent (DEBUG_AGENT_INIT_PREMEM_SEC, &SecCoreData, SecStartupPhase2);

}

/**
  Caller provided function to be invoked at the end of InitializeDebugAgent().

  Entry point to the C language phase of SEC. After the SEC assembly
  code has initialized some temporary memory and set up the stack,
  the control is transferred to this function.

  @param[in] Context    The first input parameter of InitializeDebugAgent().

**/
VOID
EFIAPI
SecStartupPhase2(
  IN VOID                     *Context
  )
{
  DEBUG ((DEBUG_ERROR, "[GX-SEC] phase2 VGA clear delay begin usec=1000000\n"));
  MicroSecondDelay (1000000);
  edkii_vga_clear();
  DEBUG ((DEBUG_ERROR, "[GX-SEC] phase2 VGA full-page clear returned\n"));
  
  // edkii_vga_print(0, "EDK II- Testing the delay - 1");
  // MicroSecondDelay(5000000);
  // edkii_vga_print(0, "EDK II- Testing the delay - 1");
  // MicroSecondDelay(5000000);
  // edkii_vga_clear();
  edkii_vga_print(0, "EDK II SecStartupPhase2");

  EFI_SEC_PEI_HAND_OFF        *SecCoreData;
  EFI_PEI_CORE_ENTRY_POINT    PeiCoreEntryPoint;

  SecCoreData = (EFI_SEC_PEI_HAND_OFF *) Context;
  //
  // Find Pei Core entry point. It will report SEC and Pei Core debug information if remote debug
  // is enabled.
  //
  DEBUG ((DEBUG_ERROR, "[GX-SEC] find PEI begin fv=%p size=0x%lx temp=%p+0x%lx stack=%p+0x%lx\n",
    SecCoreData->BootFirmwareVolumeBase, (UINT64)SecCoreData->BootFirmwareVolumeSize,
    SecCoreData->TemporaryRamBase, (UINT64)SecCoreData->TemporaryRamSize,
    SecCoreData->StackBase, (UINT64)SecCoreData->StackSize));
  edkii_vga_print (1, "SEC find PEI begin");
  FindAndReportEntryPoints ((EFI_FIRMWARE_VOLUME_HEADER *) SecCoreData->BootFirmwareVolumeBase, &PeiCoreEntryPoint);
  DEBUG ((DEBUG_ERROR, "[GX-SEC] find PEI returned entry=%p\n", PeiCoreEntryPoint));
  edkii_vga_print (1, "SEC find PEI returned");
  edkii_vga_sprintf(9, "SPa-%x-%x-%x,%x-%x-%x",
    SecCoreData,
    SecCoreData->BootFirmwareVolumeBase, 
    PeiCoreEntryPoint,
    mPeiSecPlatformInformationPpi->Flags,
    mPeiSecPlatformInformationPpi->Guid,
    mPeiSecPlatformInformationPpi->Ppi
  );
  if (PeiCoreEntryPoint == NULL)
  {
    // edkii_vga_print(0, "SePh2 - PeiCoreEntryPoint is NULL");
    DEBUG ((DEBUG_ERROR, "[EDK2-DIAG] ERROR SEC could not find PEI Core\n"));
    CpuDeadLoop ();
  }

  //
  // Transfer the control to the PEI core
  //
  // edkii_vga_print(3, "SePh2[3]");
  ASSERT (PeiCoreEntryPoint != NULL);

  // edkii_vga_hex_dump((void*)PeiCoreEntryPoint, 64, 9);
  // edkii_vga_hex_dump((void*)*PeiCoreEntryPoint, 64, 14);
  // edkii_vga_hex_dump((void*)SecCoreData, 64, 19);

  // MicroSecondDelay(5000000);
  // ASSERT (PeiCoreEntryPoint != NULL);
  DEBUG ((DEBUG_ERROR, "[EDK2-DIAG] CHECKPOINT SEC -> PEI entry=%p handoff=%p\n",
    PeiCoreEntryPoint, SecCoreData));
  (*PeiCoreEntryPoint) (SecCoreData, (EFI_PEI_PPI_DESCRIPTOR *)&mPeiSecPlatformInformationPpi);
  DEBUG ((DEBUG_ERROR, "[EDK2-DIAG] ERROR PEI Core returned to SEC\n"));
  edkii_vga_sprintf(10, "SPb-%x-%x-%x,%x-%x-%x",
    SecCoreData,
    SecCoreData->BootFirmwareVolumeBase, 
    PeiCoreEntryPoint,
    mPeiSecPlatformInformationPpi->Flags,
    mPeiSecPlatformInformationPpi->Guid,
    mPeiSecPlatformInformationPpi->Ppi
  );
  // edkii_vga_print(0, "SePh2[4] Fatal");

// int test = 1;
//   while (test);
  //
  // Should not come here.
  //
  return ;
}

/**
  This service of the TEMPORARY_RAM_SUPPORT_PPI that migrates temporary RAM into
  permanent memory.

  @param PeiServices            Pointer to the PEI Services Table.
  @param TemporaryMemoryBase    Source Address in temporary memory from which the SEC or PEIM will copy the
                                Temporary RAM contents.
  @param PermanentMemoryBase    Destination Address in permanent memory into which the SEC or PEIM will copy the
                                Temporary RAM contents.
  @param CopySize               Amount of memory to migrate from temporary to permanent memory.

  @retval EFI_SUCCESS           The data was successfully returned.
  @retval EFI_INVALID_PARAMETER PermanentMemoryBase + CopySize > TemporaryMemoryBase when
                                TemporaryMemoryBase > PermanentMemoryBase.

**/
EFI_STATUS
EFIAPI
SecTemporaryRamSupport (
  IN CONST EFI_PEI_SERVICES   **PeiServices,
  IN EFI_PHYSICAL_ADDRESS     TemporaryMemoryBase,
  IN EFI_PHYSICAL_ADDRESS     PermanentMemoryBase,
  IN UINTN                    CopySize
  )
{
  IA32_DESCRIPTOR   IdtDescriptor;
  VOID*             OldHeap;
  VOID*             NewHeap;
  VOID*             OldStack;
  VOID*             NewStack;
  DEBUG_AGENT_CONTEXT_POSTMEM_SEC  DebugAgentContext;
  BOOLEAN           OldStatus;
  UINTN             PeiStackSize;

  PeiStackSize = (CopySize >> 1);

  ASSERT (PeiStackSize < CopySize);

  //
  // |-------------------|---->
  // |      Stack        |    PeiStackSize
  // |-------------------|---->
  // |      Heap         |    PeiTemporaryRamSize
  // |-------------------|---->  TempRamBase
  //
  // |-------------------|---->
  // |      Heap         |    PeiTemporaryRamSize
  // |-------------------|---->
  // |      Stack        |    PeiStackSize
  // |-------------------|---->  PermanentMemoryBase
  //

  OldHeap = (VOID*)(UINTN)TemporaryMemoryBase;
  NewHeap = (VOID*)((UINTN)PermanentMemoryBase + PeiStackSize);

  OldStack = (VOID*)((UINTN)TemporaryMemoryBase + CopySize - PeiStackSize);
  NewStack = (VOID*)(UINTN)PermanentMemoryBase;

  DebugAgentContext.HeapMigrateOffset = (UINTN)NewHeap - (UINTN)OldHeap;
  DebugAgentContext.StackMigrateOffset = (UINTN)NewStack - (UINTN)OldStack;

  OldStatus = SaveAndSetDebugTimerInterrupt (FALSE);
  //
  // Initialize Debug Agent to support source level debug in PEI phase after memory ready.
  // It will build HOB and fix up the pointer in IDT table.
  //
  InitializeDebugAgent (DEBUG_AGENT_INIT_POSTMEM_SEC, (VOID *) &DebugAgentContext, NULL);

  //
  // Migrate Heap
  //
  DEBUG ((DEBUG_ERROR, "[GX-SEC] migrate begin old=0x%lx new=0x%lx size=0x%lx\n",
    TemporaryMemoryBase, PermanentMemoryBase, (UINT64)CopySize));
  CopyMem (NewHeap, OldHeap, CopySize - PeiStackSize);

  //
  // Migrate Stack
  //
  CopyMem (NewStack, OldStack, PeiStackSize);


  //
  // We need *not* fix the return address because currently,
  // The PeiCore is executed in flash.
  //

  //
  // Rebase IDT table in permanent memory
  //
  AsmReadIdtr (&IdtDescriptor);
  IdtDescriptor.Base = IdtDescriptor.Base - (UINTN)OldStack + (UINTN)NewStack;

  AsmWriteIdtr (&IdtDescriptor);


  //
  // Program MTRR
  //

  //
  // SecSwitchStack function must be invoked after the memory migration
  // immediately, also we need fixup the stack change caused by new call into
  // permanent memory.
  //
  SecSwitchStack (
    (UINT32) (UINTN) OldStack,
    (UINT32) (UINTN) NewStack
    );

  DEBUG ((DEBUG_ERROR, "[GX-SEC] migrate returned\n"));
  SaveAndSetDebugTimerInterrupt (OldStatus);

  return EFI_SUCCESS;
}

