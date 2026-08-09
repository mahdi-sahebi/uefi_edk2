/** @file
  Pei Core Main Entry Point

Copyright (c) 2006 - 2024, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "PeiMain.h"



/////////////////////////////////////////////////////

#include <stdarg.h> 

#include <Library/IoLib.h>
#include <Library/PrintLib.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PcdLib.h>
// #include <Library/CpuLib.h>
// #include <Library/PeCoffGetEntryPointLib.h>
// #include <Library/PeCoffExtraActionLib.h>
#include <Library/DebugAgentLib.h>

#define mde_1__VGA_FB 0xB8000
#define mde_1__VGA_COLUMNS 80

char mde_1_g_buffer[80];

void mde_1_edkii_vga_write_at_offset(unsigned int line, unsigned int offset, const char *string)
{
	if (!string)
		return;

	unsigned short *p = (unsigned short *)mde_1__VGA_FB + (mde_1__VGA_COLUMNS * line) + offset;
	unsigned int i, len = AsciiStrLen(string);

	for (i = 0; i < (mde_1__VGA_COLUMNS - offset); i++) {
		if (i < len)
			p[i] = 0x0F00 | (unsigned char)string[i];
		else
			p[i] = 0x0F00;
	}
}


void mde_1_edkii_vga_print(unsigned int line, const char *string)
{
	mde_1_edkii_vga_write_at_offset(line, 0, string);
}

void mde_1_edkii_vga_sprintf(
  unsigned int row,
  const char* format,
  ...)
{
  VA_LIST  marker;
  
  VA_START (marker, format);
  AsciiVSPrint(mde_1_g_buffer, sizeof(mde_1_g_buffer), format, marker);
  VA_END (marker);
  
  mde_1_edkii_vga_print (row, mde_1_g_buffer);
}

void mde_1_edkii_vga_clear()
{
  mde_1_edkii_vga_print(0, "                                                                                                    ");
  mde_1_edkii_vga_print(1, "                                                                                                    ");
  mde_1_edkii_vga_print(2, "                                                                                                    ");
  mde_1_edkii_vga_print(3, "                                                                                                    ");
  mde_1_edkii_vga_print(4, "                                                                                                    ");
  mde_1_edkii_vga_print(5, "                                                                                                    ");
  mde_1_edkii_vga_print(6, "                                                                                                    ");
  mde_1_edkii_vga_print(7, "                                                                                                    ");
  mde_1_edkii_vga_print(8, "                                                                                                    ");
  mde_1_edkii_vga_print(9, "                                                                                                    ");
  mde_1_edkii_vga_print(10, "                                                                                                    ");
  mde_1_edkii_vga_print(11, "                                                                                                    ");
  mde_1_edkii_vga_print(12, "                                                                                                    ");
  mde_1_edkii_vga_print(13, "                                                                                                    ");
  mde_1_edkii_vga_print(14, "                                                                                                    ");
  mde_1_edkii_vga_print(15, "                                                                                                    ");
  mde_1_edkii_vga_print(16, "                                                                                                    ");
  mde_1_edkii_vga_print(17, "                                                                                                    ");
  mde_1_edkii_vga_print(18, "                                                                                                    ");
  mde_1_edkii_vga_print(19, "                                                                                                    ");
  mde_1_edkii_vga_print(20, "                                                                                                    ");
  mde_1_edkii_vga_print(21, "                                                                                                    ");
  mde_1_edkii_vga_print(22, "                                                                                                    ");
  mde_1_edkii_vga_print(23, "                                                                                                    ");
  mde_1_edkii_vga_print(24, "                                                                                                    ");
}

void mde_1_edkii_vga_hex_dump(const unsigned char *addr, unsigned int len, int start_row) 
{
    unsigned int i;
    
    for (i = 0; i < len; i += 16) {
        unsigned int j;
        int row = start_row + (i / 16);
        int offset_pos = 0;
        
        // Write offset character by character
        unsigned long ptr_val = (unsigned long)(addr + i);
        for (j = 28; j > 0; j -= 4) {
            char nibble = (ptr_val >> j) & 0x0F;
            char c = (nibble < 10) ? ('0' + nibble) : ('A' + nibble - 10);
            char buf[2] = {c, '\0'};
            mde_1_edkii_vga_write_at_offset(row, offset_pos++, buf);
        }
        {
            char last_nibble = ptr_val & 0x0F;
            char c = (last_nibble < 10) ? ('0' + last_nibble) : ('A' + last_nibble - 10);
            char buf[2] = {c, '\0'};
            mde_1_edkii_vga_write_at_offset(row, offset_pos++, buf);
        }
        mde_1_edkii_vga_write_at_offset(row, offset_pos++, ": ");
        
        // Write hex bytes
        for (j = 0; j < 16 && (i + j < len); j++) {
            unsigned char byte = addr[i + j];
            // Write high nibble
            char high = (byte >> 4) & 0x0F;
            char c1 = (high < 10) ? ('0' + high) : ('A' + high - 10);
            char buf1[2] = {c1, '\0'};
            mde_1_edkii_vga_write_at_offset(row, offset_pos++, buf1);
            // Write low nibble
            char low = byte & 0x0F;
            char c2 = (low < 10) ? ('0' + low) : ('A' + low - 10);
            char buf2[2] = {c2, '\0'};
            mde_1_edkii_vga_write_at_offset(row, offset_pos++, buf2);
            // Write space
            mde_1_edkii_vga_write_at_offset(row, offset_pos++, " ");
        }
        
        // Pad remaining hex spaces
        for (; j < 16; j++) {
            mde_1_edkii_vga_write_at_offset(row, offset_pos++, "   ");
        }
        
        // Write ASCII representation
        mde_1_edkii_vga_write_at_offset(row, offset_pos++, "  ");
        
        for (j = 0; j < 16 && (i + j < len); j++) {
            unsigned char byte = addr[i + j];
            char c = (byte >= 0x20 && byte <= 0x7e) ? (char)byte : '.';
            char buf[2] = {c, '\0'};
            mde_1_edkii_vga_write_at_offset(row, offset_pos + j, buf);
        }
    }
}




static void delay_s(int n)
{
  volatile unsigned long t = 25;
  volatile unsigned long x = (unsigned long)n * 10000UL;

  while (x--) {
    for (unsigned long i1 = 0; i1 < 1000UL; ++i1) {
        for (int i = 0; i < 20; ++i) {
            t = t * 14823424UL + x + 1UL;
        }
    }
  }

  mde_1_edkii_vga_sprintf(24, "%x", t);
}
/////////////////////////////////////////////////////









EFI_PEI_PPI_DESCRIPTOR  mMemoryDiscoveredPpi = {
  (EFI_PEI_PPI_DESCRIPTOR_PPI | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST),
  &gEfiPeiMemoryDiscoveredPpiGuid,
  NULL
};
EFI_PEI_PPI_DESCRIPTOR  mMigrateTempRamPpi = {
  (EFI_PEI_PPI_DESCRIPTOR_PPI | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST),
  &gEdkiiPeiMigrateTempRamPpiGuid,
  NULL
};

///
/// Pei service instance
///
EFI_PEI_SERVICES  gPs = {
  {
    PEI_SERVICES_SIGNATURE,
    PEI_SERVICES_REVISION,
    sizeof (EFI_PEI_SERVICES),
    0,
    0
  },
  PeiInstallPpi,
  PeiReInstallPpi,
  PeiLocatePpi,
  PeiNotifyPpi,

  PeiGetBootMode,
  PeiSetBootMode,

  PeiGetHobList,
  PeiCreateHob,

  PeiFfsFindNextVolume,
  PeiFfsFindNextFile,
  PeiFfsFindSectionData,

  PeiInstallPeiMemory,
  PeiAllocatePages,
  PeiAllocatePool,
  (EFI_PEI_COPY_MEM)CopyMem,
  (EFI_PEI_SET_MEM)SetMem,

  PeiReportStatusCode,
  PeiResetSystem,

  &gPeiDefaultCpuIoPpi,
  &gPeiDefaultPciCfg2Ppi,

  PeiFfsFindFileByName,
  PeiFfsGetFileInfo,
  PeiFfsGetVolumeInfo,
  PeiRegisterForShadow,
  PeiFfsFindSectionData3,
  PeiFfsGetFileInfo2,
  PeiResetSystem2,
  PeiFreePages,
};

/**
  Shadow PeiCore module from flash to installed memory.

  @param PrivateData    PeiCore's private data structure

  @return PeiCore function address after shadowing.
**/
PEICORE_FUNCTION_POINTER
ShadowPeiCore (
  IN PEI_CORE_INSTANCE  *PrivateData
  )
{
  EFI_PEI_FILE_HANDLE           PeiCoreFileHandle;
  EFI_PHYSICAL_ADDRESS          EntryPoint;
  EFI_STATUS                    Status;
  UINT32                        AuthenticationState;
  UINTN                         Index;
  EFI_PEI_CORE_FV_LOCATION_PPI  *PeiCoreFvLocationPpi;
  UINTN                         PeiCoreFvIndex;

  PeiCoreFileHandle = NULL;
  //
  // Default PeiCore is in BFV
  //
  PeiCoreFvIndex = 0;
  //
  // Find the PEI Core either from EFI_PEI_CORE_FV_LOCATION_PPI indicated FV or BFV
  //
  Status = PeiServicesLocatePpi (
             &gEfiPeiCoreFvLocationPpiGuid,
             0,
             NULL,
             (VOID **)&PeiCoreFvLocationPpi
             );
  if (!EFI_ERROR (Status) && (PeiCoreFvLocationPpi->PeiCoreFvLocation != NULL)) {
    //
    // If PeiCoreFvLocation present, the PEI Core should be found from indicated FV
    //
    for (Index = 0; Index < PrivateData->FvCount; Index++) {
      if (PrivateData->Fv[Index].FvHandle == PeiCoreFvLocationPpi->PeiCoreFvLocation) {
        PeiCoreFvIndex = Index;
        break;
      }
    }

    ASSERT (Index < PrivateData->FvCount);
  }

  //
  // Find PEI Core from the given FV index
  //
  Status = PrivateData->Fv[PeiCoreFvIndex].FvPpi->FindFileByType (
                                                    PrivateData->Fv[PeiCoreFvIndex].FvPpi,
                                                    EFI_FV_FILETYPE_PEI_CORE,
                                                    PrivateData->Fv[PeiCoreFvIndex].FvHandle,
                                                    &PeiCoreFileHandle
                                                    );
  ASSERT_EFI_ERROR (Status);

  //
  // Shadow PEI Core into memory so it will run faster
  //
  Status = PeiLoadImage (
             GetPeiServicesTablePointer (),
             *((EFI_PEI_FILE_HANDLE *)&PeiCoreFileHandle),
             PEIM_STATE_REGISTER_FOR_SHADOW,
             &EntryPoint,
             &AuthenticationState
             );
  ASSERT_EFI_ERROR (Status);

  //
  // Compute the PeiCore's function address after shadowed PeiCore.
  // _ModuleEntryPoint is PeiCore main function entry
  //
  return (PEICORE_FUNCTION_POINTER)((UINTN)EntryPoint + (UINTN)PeiCore - (UINTN)_ModuleEntryPoint);
}

/**
  This routine is invoked by main entry of PeiMain module during transition
  from SEC to PEI. After switching stack in the PEI core, it will restart
  with the old core data.

  @param SecCoreDataPtr  Points to a data structure containing information about the PEI core's operating
                         environment, such as the size and location of temporary RAM, the stack location and
                         the BFV location.
  @param PpiList         Points to a list of one or more PPI descriptors to be installed initially by the PEI core.
                         An empty PPI list consists of a single descriptor with the end-tag
                         EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST. As part of its initialization
                         phase, the PEI Foundation will add these SEC-hosted PPIs to its PPI database such
                         that both the PEI Foundation and any modules can leverage the associated service
                         calls and/or code in these early PPIs
  @param Data            Pointer to old core data that is used to initialize the
                         core's data areas.
                         If NULL, it is first PeiCore entering.

**/
// #include <stdint.h>
static int g_counter = 0;

VOID
EFIAPI
PeiCore (
  IN CONST EFI_SEC_PEI_HAND_OFF    *SecCoreDataPtr,
  IN CONST EFI_PEI_PPI_DESCRIPTOR  *PpiList,
  IN VOID                          *Data
  )
{

  PEI_CORE_INSTANCE               PrivateData;
  EFI_SEC_PEI_HAND_OFF            *SecCoreData;
  EFI_SEC_PEI_HAND_OFF            NewSecCoreData;
  EFI_STATUS                      Status;
  PEI_CORE_TEMP_POINTERS          TempPtr;
  PEI_CORE_INSTANCE               *OldCoreData;
  EFI_PEI_CPU_IO_PPI              *CpuIo;
  EFI_PEI_PCI_CFG2_PPI            *PciCfg;
  EFI_HOB_HANDOFF_INFO_TABLE      *HandoffInformationTable;
  EFI_PEI_TEMPORARY_RAM_DONE_PPI  *TemporaryRamDonePpi;
  UINTN                           Index;

  mde_1_edkii_vga_clear();
  g_counter++;

  //
  // Retrieve context passed into PEI Core
  //
  OldCoreData = (PEI_CORE_INSTANCE *)Data;
  SecCoreData = (EFI_SEC_PEI_HAND_OFF *)SecCoreDataPtr;

  delay_s(2);
  mde_1_edkii_vga_sprintf(0, "[%d]PC:D=%X S=%X P=%X", 
    g_counter, // [1]
    Data, // PC:D=0
    SecCoreDataPtr,  // S=8fe94
    PpiList);//P=8030b4
    
  delay_s(2);
  mde_1_edkii_vga_sprintf(1, "[%d]BFV=%X,SZ=%X,TB=%X,TS=%X",
    g_counter,// [1]
    (UINT32)(UINTN)SecCoreData->BootFirmwareVolumeBase,//BFV=800000
    (UINT32)(UINTN)SecCoreData->BootFirmwareVolumeSize,// SZ=FF800000
    (UINT32)(UINTN)SecCoreData->TemporaryRamBase,// TB=80000
    (UINT32)(UINTN)SecCoreData->TemporaryRamSize);//TS=10000
    
  //
  // Perform PEI Core phase specific actions.
  //
  if (OldCoreData == NULL) {
    //
    // If OldCoreData is NULL, means current is the first entry into the PEI Core before memory is available.
    //
    ZeroMem (&PrivateData, sizeof (PEI_CORE_INSTANCE));
    PrivateData.Signature = PEI_CORE_HANDLE_SIGNATURE;
    CopyMem (&PrivateData.ServiceTableShadow, &gPs, sizeof (gPs));
    
    mde_1_edkii_vga_sprintf(2, "[%d]PD=%X PS=%X",
      g_counter, //[1]
      &PrivateData, //PD=8fa30
      PrivateData.Ps);//PS=0
  } else {
    mde_1_edkii_vga_sprintf(3, "[%d]OD=%X SPC=%X HO=%X PI=%d,HH=%X FVC=%d",
      g_counter,
      OldCoreData,
      OldCoreData->ShadowedPeiCore,
      (UINT32)OldCoreData->HeapOffset,
      OldCoreData->PeiMemoryInstalled,
      OldCoreData->HobList.HandoffInformationTable,
      OldCoreData->FvCount);
    
    // Memory is available to the PEI Core.  See if the PEI Core has been shadowed to memory yet.
    //
    if (OldCoreData->ShadowedPeiCore == NULL) {
      //
      // Fixup the PeiCore's private data
      //
      OldCoreData->Ps    = &OldCoreData->ServiceTableShadow;
      OldCoreData->CpuIo = &OldCoreData->ServiceTableShadow.CpuIo;
      if (OldCoreData->HeapOffsetPositive) {
        OldCoreData->HobList.Raw = (VOID *)(OldCoreData->HobList.Raw + OldCoreData->HeapOffset);
        if (OldCoreData->UnknownFvInfo != NULL) {
          OldCoreData->UnknownFvInfo = (PEI_CORE_UNKNOW_FORMAT_FV_INFO *)((UINT8 *)OldCoreData->UnknownFvInfo + OldCoreData->HeapOffset);
        }

        if (OldCoreData->CurrentFvFileHandles != NULL) {
          OldCoreData->CurrentFvFileHandles = (EFI_PEI_FILE_HANDLE *)((UINT8 *)OldCoreData->CurrentFvFileHandles + OldCoreData->HeapOffset);
        }

        if (OldCoreData->PpiData.PpiList.PpiPtrs != NULL) {
          OldCoreData->PpiData.PpiList.PpiPtrs = (PEI_PPI_LIST_POINTERS *)((UINT8 *)OldCoreData->PpiData.PpiList.PpiPtrs + OldCoreData->HeapOffset);
        }

        if (OldCoreData->PpiData.CallbackNotifyList.NotifyPtrs != NULL) {
          OldCoreData->PpiData.CallbackNotifyList.NotifyPtrs = (PEI_PPI_LIST_POINTERS *)((UINT8 *)OldCoreData->PpiData.CallbackNotifyList.NotifyPtrs + OldCoreData->HeapOffset);
        }

        if (OldCoreData->PpiData.DispatchNotifyList.NotifyPtrs != NULL) {
          OldCoreData->PpiData.DispatchNotifyList.NotifyPtrs = (PEI_PPI_LIST_POINTERS *)((UINT8 *)OldCoreData->PpiData.DispatchNotifyList.NotifyPtrs + OldCoreData->HeapOffset);
        }

        OldCoreData->Fv = (PEI_CORE_FV_HANDLE *)((UINT8 *)OldCoreData->Fv + OldCoreData->HeapOffset);
        for (Index = 0; Index < OldCoreData->FvCount; Index++) {
          if (OldCoreData->Fv[Index].PeimState != NULL) {
            OldCoreData->Fv[Index].PeimState = (UINT8 *)OldCoreData->Fv[Index].PeimState + OldCoreData->HeapOffset;
          }

          if (OldCoreData->Fv[Index].FvFileHandles != NULL) {
            OldCoreData->Fv[Index].FvFileHandles = (EFI_PEI_FILE_HANDLE *)((UINT8 *)OldCoreData->Fv[Index].FvFileHandles + OldCoreData->HeapOffset);
          }
        }

        OldCoreData->TempFileGuid    = (EFI_GUID *)((UINT8 *)OldCoreData->TempFileGuid + OldCoreData->HeapOffset);
        OldCoreData->TempFileHandles = (EFI_PEI_FILE_HANDLE *)((UINT8 *)OldCoreData->TempFileHandles + OldCoreData->HeapOffset);
      } else {
        OldCoreData->HobList.Raw = (VOID *)(OldCoreData->HobList.Raw - OldCoreData->HeapOffset);
        if (OldCoreData->UnknownFvInfo != NULL) {
          OldCoreData->UnknownFvInfo = (PEI_CORE_UNKNOW_FORMAT_FV_INFO *)((UINT8 *)OldCoreData->UnknownFvInfo - OldCoreData->HeapOffset);
        }

        if (OldCoreData->CurrentFvFileHandles != NULL) {
          OldCoreData->CurrentFvFileHandles = (EFI_PEI_FILE_HANDLE *)((UINT8 *)OldCoreData->CurrentFvFileHandles - OldCoreData->HeapOffset);
        }

        if (OldCoreData->PpiData.PpiList.PpiPtrs != NULL) {
          OldCoreData->PpiData.PpiList.PpiPtrs = (PEI_PPI_LIST_POINTERS *)((UINT8 *)OldCoreData->PpiData.PpiList.PpiPtrs - OldCoreData->HeapOffset);
        }

        if (OldCoreData->PpiData.CallbackNotifyList.NotifyPtrs != NULL) {
          OldCoreData->PpiData.CallbackNotifyList.NotifyPtrs = (PEI_PPI_LIST_POINTERS *)((UINT8 *)OldCoreData->PpiData.CallbackNotifyList.NotifyPtrs - OldCoreData->HeapOffset);
        }

        if (OldCoreData->PpiData.DispatchNotifyList.NotifyPtrs != NULL) {
          OldCoreData->PpiData.DispatchNotifyList.NotifyPtrs = (PEI_PPI_LIST_POINTERS *)((UINT8 *)OldCoreData->PpiData.DispatchNotifyList.NotifyPtrs - OldCoreData->HeapOffset);
        }

        OldCoreData->Fv = (PEI_CORE_FV_HANDLE *)((UINT8 *)OldCoreData->Fv - OldCoreData->HeapOffset);
        for (Index = 0; Index < OldCoreData->FvCount; Index++) {
          if (OldCoreData->Fv[Index].PeimState != NULL) {
            OldCoreData->Fv[Index].PeimState = (UINT8 *)OldCoreData->Fv[Index].PeimState - OldCoreData->HeapOffset;
          }

          if (OldCoreData->Fv[Index].FvFileHandles != NULL) {
            OldCoreData->Fv[Index].FvFileHandles = (EFI_PEI_FILE_HANDLE *)((UINT8 *)OldCoreData->Fv[Index].FvFileHandles - OldCoreData->HeapOffset);
          }
        }

        OldCoreData->TempFileGuid    = (EFI_GUID *)((UINT8 *)OldCoreData->TempFileGuid - OldCoreData->HeapOffset);
        OldCoreData->TempFileHandles = (EFI_PEI_FILE_HANDLE *)((UINT8 *)OldCoreData->TempFileHandles - OldCoreData->HeapOffset);
      }

      // Force relocating the dispatch table
      OldCoreData->DelayedDispatchTable = NULL;

      //
      // Fixup for PeiService's address
      //
      mde_1_edkii_vga_sprintf(4, "[%d]SPSTP %X, %X,FPM:%X,HIT:%X", 
        g_counter,
        (UINT32)(UINTN)(&OldCoreData->Ps), 
        (UINT32)(UINTN)OldCoreData->Ps,
        OldCoreData->FreePhysicalMemoryTop,
        HandoffInformationTable->EfiEndOfHobList
      );
      SetPeiServicesTablePointer ((CONST EFI_PEI_SERVICES **)&OldCoreData->Ps);

      //
      // Initialize libraries that the PEI Core is linked against
      //
      mde_1_edkii_vga_sprintf(5, "[%d]PLCL,HIT:%X,HOP:%d,PMB:%X,OHIT:%X", 
        g_counter,
        OldCoreData->HobList.HandoffInformationTable,
        (int)OldCoreData->HeapOffsetPositive,
        OldCoreData->PhysicalMemoryBegin,
        OldCoreData->PhysicalMemoryLength
      );
      ProcessLibraryConstructorList (NULL, (CONST EFI_PEI_SERVICES **)&OldCoreData->Ps);

      //
      // Update HandOffHob for new installed permanent memory
      //
      HandoffInformationTable = OldCoreData->HobList.HandoffInformationTable;
      if (OldCoreData->HeapOffsetPositive) {
        HandoffInformationTable->EfiEndOfHobList = HandoffInformationTable->EfiEndOfHobList + OldCoreData->HeapOffset;
      } else {
        HandoffInformationTable->EfiEndOfHobList = HandoffInformationTable->EfiEndOfHobList - OldCoreData->HeapOffset;
      }

      HandoffInformationTable->EfiMemoryTop        = OldCoreData->PhysicalMemoryBegin + OldCoreData->PhysicalMemoryLength;
      HandoffInformationTable->EfiMemoryBottom     = OldCoreData->PhysicalMemoryBegin;
      HandoffInformationTable->EfiFreeMemoryTop    = OldCoreData->FreePhysicalMemoryTop;
      HandoffInformationTable->EfiFreeMemoryBottom = HandoffInformationTable->EfiEndOfHobList + sizeof (EFI_HOB_GENERIC_HEADER);

      //
      // We need convert MemoryBaseAddress in memory allocation HOBs
      //
      mde_1_edkii_vga_sprintf(6, "[%d]OD1,HIT:%X,HOP:%d,PMB:%X,OHIT:%X", 
        g_counter,
        OldCoreData->HobList.HandoffInformationTable,
        (int)OldCoreData->HeapOffsetPositive,
        OldCoreData->PhysicalMemoryBegin,
        OldCoreData->PhysicalMemoryLength
      );
      ConvertMemoryAllocationHobs (OldCoreData);

      //
      // We need convert the PPI descriptor's pointer
      //
      mde_1_edkii_vga_sprintf(7, "[%d]OD2,HIT:%X,HOP:%d,PMB:%X,OHIT:%X", 
        g_counter,
        OldCoreData->HobList.HandoffInformationTable,
        (int)OldCoreData->HeapOffsetPositive,
        OldCoreData->PhysicalMemoryBegin,
        OldCoreData->PhysicalMemoryLength
      );
      ConvertPpiPointers (SecCoreData, OldCoreData);

      //
      // After the whole temporary memory is migrated, then we can allocate page in
      // permanent memory.
      //
      OldCoreData->PeiMemoryInstalled = TRUE;

      mde_1_edkii_vga_sprintf(8, "[%d]OD2,HIT:%X,HOP:%d,PMB:%X,OHIT:%X", 
        g_counter,
        OldCoreData->HobList.HandoffInformationTable,
        (int)OldCoreData->HeapOffsetPositive,
        OldCoreData->PhysicalMemoryBegin,
        OldCoreData->PhysicalMemoryLength
      );
      if (PcdGetBool (PcdMigrateTemporaryRamFirmwareVolumes)) {
        mde_1_edkii_vga_sprintf(9, "[%d] PcdGetBool", g_counter);
        DEBUG ((DEBUG_VERBOSE, "Early Migration - PPI lists before temporary RAM evacuation:\n"));
        DumpPpiList (OldCoreData);
        mde_1_edkii_vga_sprintf(10, "[%d] DumpPpiList", g_counter);

        //
        // Migrate installed content from Temporary RAM to Permanent RAM at this
        // stage when PEI core still runs from a cached location.
        // FVs that doesn't contain PEI_CORE should be migrated here.
        //
        EvacuateTempRam (OldCoreData, SecCoreData);
        mde_1_edkii_vga_sprintf(11, "[%d] EvacuateTempRam", g_counter);

        DEBUG ((DEBUG_VERBOSE, "Early Migration - PPI lists after temporary RAM evacuation:\n"));
        DumpPpiList (OldCoreData);
        mde_1_edkii_vga_sprintf(12, "[%d] DumpPpiList", g_counter);
      }

      //
      // Indicate that PeiCore reenter
      //
      OldCoreData->PeimDispatcherReenter = TRUE;

      mde_1_edkii_vga_sprintf(13, "[%d]IPCR-%X", 
        g_counter,
        OldCoreData->HobList.HandoffInformationTable->BootMode
      );
      if ((PcdGet64 (PcdLoadModuleAtFixAddressEnable) != 0) && (OldCoreData->HobList.HandoffInformationTable->BootMode != BOOT_ON_S3_RESUME)) {
        //
        // if Loading Module at Fixed Address is enabled, allocate the PEI code memory range usage bit map array.
        // Every bit in the array indicate the status of the corresponding memory page available or not
        //
        mde_1_edkii_vga_sprintf(14, "[%d]PG1-%X", g_counter, OldCoreData->PeiCodeMemoryRangeUsageBitMap);
        OldCoreData->PeiCodeMemoryRangeUsageBitMap = AllocateZeroPool (((PcdGet32 (PcdLoadFixAddressPeiCodePageNumber)>>6) + 1)*sizeof (UINT64));
        mde_1_edkii_vga_sprintf(14, "[%d]PG2-%X", g_counter, OldCoreData->PeiCodeMemoryRangeUsageBitMap);
      }


      //
      // Shadow PEI Core. When permanent memory is available, shadow
      // PEI Core and PEIMs to get high performance.
      //
      OldCoreData->ShadowedPeiCore = (PEICORE_FUNCTION_POINTER)(UINTN)PeiCore;
      
      mde_1_edkii_vga_sprintf(15, "[%d]C1-%X, %X", 
        g_counter,
        OldCoreData->ShadowedPeiCore,
        HandoffInformationTable->BootMode
      );

      if (PcdGetBool (PcdMigrateTemporaryRamFirmwareVolumes) ||
          ((HandoffInformationTable->BootMode == BOOT_ON_S3_RESUME) && PcdGetBool (PcdShadowPeimOnS3Boot)) ||
          ((HandoffInformationTable->BootMode != BOOT_ON_S3_RESUME) && PcdGetBool (PcdShadowPeimOnBoot)))
      {
        mde_1_edkii_vga_sprintf(16, "[%d]C2", g_counter);
        OldCoreData->ShadowedPeiCore = ShadowPeiCore (OldCoreData);
        mde_1_edkii_vga_sprintf(16, "[%d]C3-%X", g_counter, OldCoreData->ShadowedPeiCore);
      }

      //
      // PEI Core has now been shadowed to memory.  Restart PEI Core in memory.
      //
      mde_1_edkii_vga_sprintf(17, "[%d]C4", g_counter);
      OldCoreData->ShadowedPeiCore (SecCoreData, PpiList, OldCoreData);
      mde_1_edkii_vga_sprintf(17, "[%d]C5", g_counter);

      //
      // Should never reach here.
      //
      ASSERT (FALSE);
      CpuDeadLoop ();

      UNREACHABLE ();
    }

    mde_1_edkii_vga_sprintf(18, "[%d]a-NS:%X, SC:%X, SNS:%X", 
      g_counter,
      &NewSecCoreData, 
      SecCoreDataPtr, 
      sizeof(NewSecCoreData));

    //
    // Memory is available to the PEI Core and the PEI Core has been shadowed to memory.
    //
    CopyMem (&NewSecCoreData, SecCoreDataPtr, sizeof (NewSecCoreData));
    SecCoreData = &NewSecCoreData;
    CopyMem (&PrivateData, OldCoreData, sizeof (PrivateData));

    CpuIo  = (VOID *)PrivateData.ServiceTableShadow.CpuIo;
    PciCfg = (VOID *)PrivateData.ServiceTableShadow.PciCfg;

    CopyMem (&PrivateData.ServiceTableShadow, &gPs, sizeof (gPs));

    PrivateData.ServiceTableShadow.CpuIo  = CpuIo;
    PrivateData.ServiceTableShadow.PciCfg = PciCfg;
  }

  //
  // Cache a pointer to the PEI Services Table that is either in temporary memory or permanent memory
  //
  PrivateData.Ps = &PrivateData.ServiceTableShadow;

  delay_s(2);
  mde_1_edkii_vga_sprintf(19, "[%d]b-Ps:%X, SC:%X, OD:%X", 
    g_counter,//1
    &PrivateData.Ps, //8fa30
    SecCoreData, //8fe94
    OldCoreData);//0
    
  delay_s(2);
  //
  // Save PeiServicePointer so that it can be retrieved anywhere.
  //
  SetPeiServicesTablePointer ((CONST EFI_PEI_SERVICES **)&PrivateData.Ps);

  mde_1_edkii_vga_sprintf(19, "[%d]c-Ps:%X, SC:%X, OD:%X", 
    g_counter,
    &PrivateData.Ps, 
    SecCoreData, 
    OldCoreData);
    
  delay_s(2);
  //
  // Initialize libraries that the PEI Core is linked against
  //
  ProcessLibraryConstructorList (NULL, (CONST EFI_PEI_SERVICES **)&PrivateData.Ps);

  
  mde_1_edkii_vga_sprintf(19, "[%d]d-Ps:%X, SC:%X, OD:%X", 
    g_counter,
    &PrivateData.Ps, 
    SecCoreData, 
    OldCoreData);
  delay_s(2);
  //
  // Initialize PEI Core Services
  //
  InitializeMemoryServices (&PrivateData, SecCoreData, OldCoreData);

  mde_1_edkii_vga_sprintf(19, "[%d]e-Ps:%X, SC:%X, OD:%X", 
    g_counter,
    &PrivateData.Ps, 
    SecCoreData, 
    OldCoreData);
  delay_s(2);
  //
  // Update performance measurements
  //
  if (OldCoreData == NULL) {
    PERF_EVENT ("SEC"); // Means the end of SEC phase.

    //
    // If first pass, start performance measurement.
    //
    PERF_CROSSMODULE_BEGIN ("PEI");
    PERF_INMODULE_BEGIN ("PreMem");
  } else {
    PERF_INMODULE_END ("PreMem");
    PERF_INMODULE_BEGIN ("PostMem");
  }

  
  mde_1_edkii_vga_sprintf(19, "[%d]f-Ps:%X, SC:%X, OD:%X", 
    g_counter,
    &PrivateData.Ps, 
    SecCoreData, 
    OldCoreData);
  delay_s(2);
  //
  // Complete PEI Core Service initialization
  //
  InitializeSecurityServices (&PrivateData.Ps, OldCoreData);
  
  mde_1_edkii_vga_sprintf(19, "[%d]g-Ps:%X, SC:%X, OD:%X", 
    g_counter,
    &PrivateData.Ps, 
    SecCoreData, 
    OldCoreData);
  delay_s(2);

  InitializeDispatcherData (&PrivateData, OldCoreData, SecCoreData);
  
  mde_1_edkii_vga_sprintf(19, "[%d]h-Ps:%X, SC:%X, OD:%X", 
    g_counter,
    &PrivateData.Ps, 
    SecCoreData, 
    OldCoreData);
  delay_s(2);

  InitializeImageServices (&PrivateData, OldCoreData);

  mde_1_edkii_vga_sprintf(19, "[%d]i-Ps:%X, SC:%X, OD:%X", 
    g_counter,
    &PrivateData.Ps, 
    SecCoreData, 
    OldCoreData);
  delay_s(2);
  //
  // Perform PEI Core Phase specific actions
  //
  if (OldCoreData == NULL) {
    //
    // Report Status Code EFI_SW_PC_INIT
    //
    REPORT_STATUS_CODE (
      EFI_PROGRESS_CODE,
      (EFI_SOFTWARE_PEI_CORE | EFI_SW_PC_INIT)
      );

    //
    // If SEC provided the PpiList, process it.
    //
    if (PpiList != NULL) {
      
      mde_1_edkii_vga_sprintf(19, "[%d]j-Ps:%X, SC:%X, OD:%X", 
        g_counter, 
        &PrivateData.Ps, 
        SecCoreData, 
        OldCoreData);
      ProcessPpiListFromSec ((CONST EFI_PEI_SERVICES **)&PrivateData.Ps, PpiList);
      
      mde_1_edkii_vga_sprintf(19, "[%d]k-Ps:%X, SC:%X, OD:%X",
        g_counter,// [1]
        &PrivateData.Ps, // Ps:8fa34
        SecCoreData, // SC:8fe94
        OldCoreData);// OD:0
      }
  } else {
    
    mde_1_edkii_vga_sprintf(19, "[%d]l-Ps:%X, SC:%X, OD:%X", 
      g_counter,
      &PrivateData.Ps, 
      SecCoreData, 
      OldCoreData);
  delay_s(2);
    if (PcdGetBool (PcdMigrateTemporaryRamFirmwareVolumes)) {
      
      mde_1_edkii_vga_sprintf(19, "[%d]l-Ps:%X, SC:%X, OD:%X", 
        g_counter,
        &PrivateData.Ps, 
        SecCoreData, 
        OldCoreData);
  delay_s(2);
      //
      // When PcdMigrateTemporaryRamFirmwareVolumes is TRUE, alway shadow all
      // PEIMs no matter the condition of PcdShadowPeimOnBoot and PcdShadowPeimOnS3Boot
      //
      DEBUG ((DEBUG_VERBOSE, "PPI lists before temporary RAM evacuation:\n"));
      DumpPpiList (&PrivateData);

      
      mde_1_edkii_vga_sprintf(19, "[%d]m-Ps:%X, SC:%X, OD:%X", 
        g_counter,
        &PrivateData.Ps, 
        SecCoreData, 
        OldCoreData);
  delay_s(2);
      //
      // Migrate installed content from Temporary RAM to Permanent RAM
      // FVs containing PEI_CORE should be migrated here.
      //
      EvacuateTempRam (&PrivateData, SecCoreData);

      mde_1_edkii_vga_sprintf(19, "[%d]n-Ps:%X, SC:%X, OD:%X", 
        g_counter,
        &PrivateData.Ps, 
        SecCoreData, 
        OldCoreData);
  delay_s(2);
      Status = PeiServicesInstallPpi (&mMigrateTempRamPpi);
      
      mde_1_edkii_vga_sprintf(19, "[%d]o-s:%X, Ps:%X, SC:%X, OD:%X", 
        g_counter,
        Status,
        &PrivateData.Ps, 
        SecCoreData, 
        OldCoreData);
  delay_s(2);
      ASSERT_EFI_ERROR (Status);

      DEBUG ((DEBUG_VERBOSE, "PPI lists after temporary RAM evacuation:\n"));
      DumpPpiList (&PrivateData);
      
      mde_1_edkii_vga_sprintf(19, "[%d]p-Ps:%X, SC:%X, OD:%X", 
        g_counter,
        &PrivateData.Ps, 
        SecCoreData, 
        OldCoreData);
  delay_s(2);
    }

    
    mde_1_edkii_vga_sprintf(20, "[%d]q-eTRP:%X, TRP:%X", 
      g_counter,
      &gEfiTemporaryRamDonePpiGuid, 
      &TemporaryRamDonePpi);
  delay_s(2);
    //
    // Try to locate Temporary RAM Done Ppi.
    //
    Status = PeiServicesLocatePpi (
               &gEfiTemporaryRamDonePpiGuid,
               0,
               NULL,
               (VOID **)&TemporaryRamDonePpi
               );
               
    mde_1_edkii_vga_sprintf(20, "[%d]q-s:%X, eTRP:%X, TRP:%X", 
      g_counter,
      Status,
      &gEfiTemporaryRamDonePpiGuid, 
      &TemporaryRamDonePpi);
  delay_s(2);
    if (!EFI_ERROR (Status)) {
      //
      // Disable the use of Temporary RAM after the transition from Temporary RAM to Permanent RAM is complete.
      //
      TemporaryRamDonePpi->TemporaryRamDone ();
    }

    mde_1_edkii_vga_sprintf(20, "[%d]r-s:%X, eTRP:%X, TRP:%X", 
      g_counter,
      Status,
      &gEfiTemporaryRamDonePpiGuid, 
      &TemporaryRamDonePpi);
  delay_s(2);
    //
    // Alert any listeners that there is permanent memory available
    //
    PERF_INMODULE_BEGIN ("DisMem");
    Status = PeiServicesInstallPpi (&mMemoryDiscoveredPpi);

    mde_1_edkii_vga_sprintf(20, "[%d]s-s:%X, MDP:%X", 
      g_counter,
      Status,
      &mMemoryDiscoveredPpi);
  delay_s(2);
    //
    // Process the Notify list and dispatch any notifies for the Memory Discovered PPI
    //
    ProcessDispatchNotifyList (&PrivateData);

    mde_1_edkii_vga_sprintf(20, "[%d]t-s:%X, MDP:%X", 
      g_counter,
      Status,
      &mMemoryDiscoveredPpi);
  delay_s(2);
    PERF_INMODULE_END ("DisMem");
  }

  mde_1_edkii_vga_sprintf(21, "[%d]u-Sec:%X, PD:%X,BM:%X,PMI:%X", 
    g_counter, // [1]
    SecCoreData, // Sec:8fe94
    &PrivateData, // PD:8fa30
    PrivateData.HobList.HandoffInformationTable->BootMode, // BM:0
    PrivateData.PeiMemoryInstalled // PMI:0
  );
  delay_s(2);
  //
  // Call PEIM dispatcher
  //
  PeiDispatcher (SecCoreData, &PrivateData);

  mde_1_edkii_vga_sprintf(21, "[%d]v-Sec:%X, PD:%X,BM:%X,PMI:%X", 
    g_counter,//3
    SecCoreData,//5f51f1cc
    &PrivateData,//5f51f1f0
    PrivateData.HobList.HandoffInformationTable->BootMode,//4
    PrivateData.PeiMemoryInstalled//1
  );
  delay_s(2);

  if (PrivateData.HobList.HandoffInformationTable->BootMode != BOOT_ON_S3_RESUME) {
    //
    // Check if InstallPeiMemory service was called on non-S3 resume boot path.
    //
    ASSERT (PrivateData.PeiMemoryInstalled == TRUE);
  }

  //
  // Measure PEI Core execution time.
  //
  PERF_INMODULE_END ("PostMem");

  mde_1_edkii_vga_sprintf(22, "[%d]w-Sec:%X, PD:%X,BM:%X,PMI:%X", 
    g_counter,
    SecCoreData,
    &PrivateData,
    PrivateData.HobList.HandoffInformationTable->BootMode,
    PrivateData.PeiMemoryInstalled
  );
  delay_s(2);
  //
  // Lookup DXE IPL PPI
  //
  Status = PeiServicesLocatePpi (
             &gEfiDxeIplPpiGuid,
             0,
             NULL,
             (VOID **)&TempPtr.DxeIpl
             );
             
  mde_1_edkii_vga_sprintf(22, "[%d]w-s:%X,Sec:%X, PD:%X,BM:%X,PMI:%X", 
    g_counter,//3
    Status,//0
    SecCoreData,//5f51f1cc
    &PrivateData,//
    PrivateData.HobList.HandoffInformationTable->BootMode,//4
    PrivateData.PeiMemoryInstalled//1
  );
  delay_s(2);
  ASSERT_EFI_ERROR (Status);

  if (EFI_ERROR (Status)) {
    //
    // Report status code to indicate DXE IPL PPI could not be found.
    //
    REPORT_STATUS_CODE (
      EFI_ERROR_CODE | EFI_ERROR_MAJOR,
      (EFI_SOFTWARE_PEI_CORE | EFI_SW_PEI_CORE_EC_DXEIPL_NOT_FOUND)
      );
    CpuDeadLoop ();
  }
          
  mde_1_edkii_vga_sprintf(22, "[%d]x-s:%X,Sec:%X, PD:%X,BM:%X,PMI:%X", 
    g_counter,//3
    Status,//0
    SecCoreData,//5f51f1cc
    &PrivateData,//5f51f1f0
    PrivateData.HobList.HandoffInformationTable->BootMode,//4
    PrivateData.PeiMemoryInstalled//1
  );


  delay_s(4);
     
  mde_1_edkii_vga_clear();
  mde_1_edkii_vga_sprintf(0, "PEIa-%x-%x,%x,%x-%x", 
    SecCoreData->BootFirmwareVolumeBase,//800000
    SecCoreData->BootFirmwareVolumeSize,//ff800000
    SecCoreData->DataSize,//24
    SecCoreData->PeiTemporaryRamBase,//80000
    SecCoreData->PeiTemporaryRamSize//8000
  );
  mde_1_edkii_vga_sprintf(1, "b-%x-%x,%x-%x", 
    SecCoreData->StackBase,//88000
    SecCoreData->StackSize,//8000
    SecCoreData->TemporaryRamBase,//80000
    SecCoreData->TemporaryRamSize//10000
  );
  delay_s(2);
  mde_1_edkii_vga_sprintf(2, "b-%x-%x,%x-%x,%x-%x-%x", 
    PrivateData.PeiMemoryInstalled,//1
    PrivateData.CurrentPeimCount,//0
    PrivateData.CurrentPeimFvCount,//0
    PrivateData.FreePhysicalMemoryTop,//63500000
    PrivateData.FvCount,//0
    PrivateData.HeapOffset,//22
    PrivateData.HeapOffsetPositive//
  );
  mde_1_edkii_vga_sprintf(3, "c-%x-%x,%x-%x,%x-%x", 
    PrivateData.HobList.Cpu->Header,//380001
    PrivateData.HobList.Cpu->SizeOfIoSpace,//0
    PrivateData.HobList.Cpu->SizeOfMemorySpace,//0
    PrivateData.HobList.Capsule->BaseAddress,//9
    PrivateData.HobList.Capsule->Header,//9
    PrivateData.HobList.Capsule->Length//4
  );
  delay_s(2);
  mde_1_edkii_vga_sprintf(4, "d-%x-%x,%x-%x,%x-%x", 
    PrivateData.HobList.FirmwareVolume->BaseAddress,//9
    PrivateData.HobList.FirmwareVolume->Header,//4
    PrivateData.HobList.FirmwareVolume->Length,//380001
    PrivateData.HobList.HandoffInformationTable->BootMode,//0
    PrivateData.HobList.HandoffInformationTable->EfiEndOfHobList,//63500000
    PrivateData.HobList.HandoffInformationTable->EfiFreeMemoryBottom//0
  );
  mde_1_edkii_vga_sprintf(5, "e-%x-%x,%x-%x,%x", 
    PrivateData.HobList.HandoffInformationTable->EfiFreeMemoryTop,//634b0000
    PrivateData.HobList.HandoffInformationTable->EfiMemoryBottom,//0
    PrivateData.HobList.HandoffInformationTable->EfiMemoryTop,//5f500000
    PrivateData.HobList.HandoffInformationTable->Header,//63500000
    PrivateData.HobList.HandoffInformationTable->Version//0
  );
  mde_1_edkii_vga_sprintf(6, "f-%x-%x,%x-%x", 
    PrivateData.HobList.MemoryAllocation->AllocDescriptor.MemoryBaseAddress,//5f500000
    PrivateData.HobList.MemoryAllocation->AllocDescriptor.MemoryLength,//0
    PrivateData.HobList.MemoryAllocation->AllocDescriptor.MemoryType,//634b0000
    PrivateData.HobList.MemoryAllocation->AllocDescriptor.Name///0
  );


  delay_s(4);
  mde_1_edkii_vga_clear();
  mde_1_edkii_vga_sprintf(0, "PeiPage2-%x-%x",
    TempPtr.DxeIpl,//634da204
    TempPtr.DxeIpl->Entry
  );
  delay_s(2);


  //
  // Enter DxeIpl to load Dxe core.
  //
  // DEBUG ((DEBUG_INFO, "DXE IPL Entry\n"));
  Status = TempPtr.DxeIpl->Entry (
                             TempPtr.DxeIpl,
                             &PrivateData.Ps,
                             PrivateData.HobList
                             );
                                    
  mde_1_edkii_vga_sprintf(1, "a-%x-%x",
    Status,
    PrivateData.Ps->InstallPeiMemory,
    PrivateData.Ps->InstallPpi
  );   
  //
  // Should never reach here.
  //
  ASSERT_EFI_ERROR (Status);
  CpuDeadLoop ();

  UNREACHABLE ();
}
