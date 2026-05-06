/** @file
  C functions in SEC

Copyright (c) 2013, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/


#include "SecMain.h"

//////////////////////////// mahdi
#include "edkii_vga.h"
/////////////////////////////////



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
  //edkii_vga_clear();
  //edkii_vga_print(0, "EDK II Started ");
  ///////////////////////////

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
  ProcessLibraryConstructorList ();

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
  char buf[78];
  edkii_vga_clear();
  edkii_vga_print(0, "SePh2-start");

  EFI_SEC_PEI_HAND_OFF        *SecCoreData;
  EFI_PEI_CORE_ENTRY_POINT    PeiCoreEntryPoint;

  SecCoreData = (EFI_SEC_PEI_HAND_OFF *) Context;
  //
  // Find Pei Core entry point. It will report SEC and Pei Core debug information if remote debug
  // is enabled.
  //
  FindAndReportEntryPoints ((EFI_FIRMWARE_VOLUME_HEADER *) SecCoreData->BootFirmwareVolumeBase, &PeiCoreEntryPoint);
  if (PeiCoreEntryPoint == NULL)
  {
    edkii_vga_print(0, "SePh2 - PeiCoreEntryPoint is NULL");
    CpuDeadLoop ();
  }

  //
  // Transfer the control to the PEI core
  //
  // edkii_vga_print(3, "SePh2[3]");
  ASSERT (PeiCoreEntryPoint != NULL);
  AsciiSPrint(buf, sizeof(buf), "SePh2: 0x%X, 0x%X,", 
    (UINT32)(UINTN)SecCoreData->BootFirmwareVolumeBase, 
    (UINT32)(UINTN)PeiCoreEntryPoint);
  edkii_vga_print(0, buf);

  // edkii_vga_hex_dump((void*)PeiCoreEntryPoint, 64, 11);
  // edkii_vga_hex_dump((void*)SecCoreData, 64, 16);
  ASSERT (PeiCoreEntryPoint != NULL);
  // (*PeiCoreEntryPoint) (SecCoreData, (EFI_PEI_PPI_DESCRIPTOR *)&mPeiSecPlatformInformationPpi);
  PeiCoreEntryPoint(SecCoreData, (EFI_PEI_PPI_DESCRIPTOR *)&mPeiSecPlatformInformationPpi);

  edkii_vga_print(0, "SePh2[4] Fatal");

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

  SaveAndSetDebugTimerInterrupt (OldStatus);

  return EFI_SUCCESS;
}

