/** @file
  x64-specifc functionality for DxeLoad.

Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "DxeIpl.h"
#include "X64/VirtualMemory.h"


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

#define mde_8__VGA_FB 0xB8000
#define mde_8__VGA_COLUMNS 80

char mde_8_g_buffer[80];

void mde_8_edkii_vga_clear(void);
void mde_8_edkii_vga_sprintf(unsigned int row, const char* format, ...);
void mde_8_edkii_vga_print(unsigned int line, const char *string);
void mde_8_edkii_vga_write_at_offset(unsigned int line, unsigned int offset, const char *string);
void mde_8_delay_s(int n);

void mde_8_edkii_vga_write_at_offset(unsigned int line, unsigned int offset, const char *string)
{
	if (!string || line >= 25 || offset >= mde_8__VGA_COLUMNS)
		return;

	volatile unsigned short *p = (volatile unsigned short *)mde_8__VGA_FB + (mde_8__VGA_COLUMNS * line) + offset;
	unsigned int i, len = AsciiStrLen(string);

	for (i = 0; i < (mde_8__VGA_COLUMNS - offset); i++) {
		if (i < len)
			p[i] = 0x0F00 | (unsigned char)string[i];
		else
			p[i] = 0x0F00;
	}
}


void mde_8_edkii_vga_print(unsigned int line, const char *string)
{
	mde_8_edkii_vga_write_at_offset(line, 0, string);
}

void mde_8_edkii_vga_sprintf(
  unsigned int row,
  const char* format,
  ...)
{
  VA_LIST  marker;
  
  VA_START (marker, format);
  AsciiVSPrint(mde_8_g_buffer, sizeof(mde_8_g_buffer), format, marker);
  VA_END (marker);
  
  DEBUG ((DEBUG_ERROR, "[GX-DxeLoadX64] row=%u %a\n", row, mde_8_g_buffer));
  mde_8_edkii_vga_print (row, mde_8_g_buffer);
}

void mde_8_edkii_vga_clear()
{
  for (unsigned int index = 0; index <= 24; index++) {
    mde_8_edkii_vga_print(index, "                                                                                                    ");
  }
}

void mde_8_edkii_vga_hex_dump(const unsigned char *addr, unsigned int len, int start_row) 
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
            mde_8_edkii_vga_write_at_offset(row, offset_pos++, buf);
        }
        {
            char last_nibble = ptr_val & 0x0F;
            char c = (last_nibble < 10) ? ('0' + last_nibble) : ('A' + last_nibble - 10);
            char buf[2] = {c, '\0'};
            mde_8_edkii_vga_write_at_offset(row, offset_pos++, buf);
        }
        mde_8_edkii_vga_write_at_offset(row, offset_pos++, ": ");
        
        // Write hex bytes
        for (j = 0; j < 16 && (i + j < len); j++) {
            unsigned char byte = addr[i + j];
            // Write high nibble
            char high = (byte >> 4) & 0x0F;
            char c1 = (high < 10) ? ('0' + high) : ('A' + high - 10);
            char buf1[2] = {c1, '\0'};
            mde_8_edkii_vga_write_at_offset(row, offset_pos++, buf1);
            // Write low nibble
            char low = byte & 0x0F;
            char c2 = (low < 10) ? ('0' + low) : ('A' + low - 10);
            char buf2[2] = {c2, '\0'};
            mde_8_edkii_vga_write_at_offset(row, offset_pos++, buf2);
            // Write space
            mde_8_edkii_vga_write_at_offset(row, offset_pos++, " ");
        }
        
        // Pad remaining hex spaces
        for (; j < 16; j++) {
            mde_8_edkii_vga_write_at_offset(row, offset_pos++, "   ");
        }
        
        // Write ASCII representation
        mde_8_edkii_vga_write_at_offset(row, offset_pos++, "  ");
        
        for (j = 0; j < 16 && (i + j < len); j++) {
            unsigned char byte = addr[i + j];
            char c = (byte >= 0x20 && byte <= 0x7e) ? (char)byte : '.';
            char buf[2] = {c, '\0'};
            mde_8_edkii_vga_write_at_offset(row, offset_pos + j, buf);
        }
    }
}


void mde_8_delay_s(int n)
{
  volatile unsigned long t = 25;
  volatile unsigned long x = (unsigned long)n * 10000UL;

  while (x--) {
    for (unsigned long i1 = 0; i1 < 1000UL; ++i1) {
        for (int i = 0; i < 10; ++i) {
            t = t * 14823424UL + x + 1UL;
        }
    }
  }

  mde_8_edkii_vga_sprintf(23, "%x", t);
}


/////////////////////////////////////////////////////


/**
   Transfers control to DxeCore.

   This function performs a CPU architecture specific operations to execute
   the entry point of DxeCore with the parameters of HobList.
   It also installs EFI_END_OF_PEI_PPI to signal the end of PEI phase.

   @param DxeCoreEntryPoint         The entry point of DxeCore.
   @param HobList                   The start of HobList passed to DxeCore.

**/
VOID
HandOffToDxeCore (
  IN EFI_PHYSICAL_ADDRESS  DxeCoreEntryPoint,
  IN EFI_PEI_HOB_POINTERS  HobList
  )
{
  VOID                             *BaseOfStack;
  VOID                             *TopOfStack;
  EFI_STATUS                       Status;
  UINTN                            PageTables;
  UINT32                           Index;
  EFI_VECTOR_HANDOFF_INFO          *VectorInfo;
  EFI_PEI_VECTOR_HANDOFF_INFO_PPI  *VectorHandoffInfoPpi;
  VOID                             *GhcbBase;
  UINTN                            GhcbSize;

  mde_8_edkii_vga_clear();

  mde_8_edkii_vga_sprintf(0, "DxeHOa-%x-%x,%x-%x,%x-%x",
    DxeCoreEntryPoint,
    HobList,
    HobList.Capsule->BaseAddress,
    HobList.Capsule->Length,
    HobList.HandoffInformationTable->BootMode,
    HobList.HandoffInformationTable->EfiEndOfHobList
  );
  mde_8_edkii_vga_sprintf(1, "b-%x-%x,%x-%x,%x-%x",
    HobList.HandoffInformationTable->EfiFreeMemoryBottom,
    HobList.HandoffInformationTable->EfiFreeMemoryTop,
    HobList.HandoffInformationTable->EfiMemoryBottom,
    HobList.HandoffInformationTable->EfiMemoryTop,
    HobList.MemoryAllocation->AllocDescriptor.MemoryBaseAddress,
    HobList.MemoryAllocation->AllocDescriptor.MemoryLength
  );
  mde_8_edkii_vga_sprintf(2, "c-%x-%x,%x-%x,%x-%x",
    HobList.MemoryAllocation->AllocDescriptor.MemoryType,
    HobList.MemoryAllocation->AllocDescriptor.Name,
    HobList.MemoryAllocationStack->AllocDescriptor.MemoryBaseAddress,
    HobList.MemoryAllocationStack->AllocDescriptor.MemoryLength,
    HobList.MemoryAllocationStack->AllocDescriptor.MemoryType,
    HobList.MemoryAllocationStack->AllocDescriptor.Name
  );
  mde_8_edkii_vga_sprintf(3, "d-%x-%x",
    HobList.Cpu->Header.HobLength,
    HobList.Cpu->Header.HobType
  );
  mde_8_delay_s(2);

  //
  // Clear page 0 and mark it as allocated if NULL pointer detection is enabled.
  //
  if (IsNullDetectionEnabled ()) {
    
    mde_8_edkii_vga_sprintf(4, "4a-%x",
      0
    );
    mde_8_delay_s(2);
    
    ClearFirst4KPage (HobList.Raw);
    
    mde_8_edkii_vga_sprintf(4, "4b-%x",
      0
    );
    mde_8_delay_s(2);

    BuildMemoryAllocationHob (0, EFI_PAGES_TO_SIZE (1), EfiBootServicesData);
    
    mde_8_edkii_vga_sprintf(4, "4c-%x",
      0
    );
    mde_8_delay_s(2);
  }

  
  mde_8_edkii_vga_sprintf(5, "5a-%x",
    0
  );
  mde_8_delay_s(2);
  //
  // Get Vector Hand-off Info PPI and build Guided HOB
  //
  Status = PeiServicesLocatePpi (
             &gEfiVectorHandoffInfoPpiGuid,
             0,
             NULL,
             (VOID **)&VectorHandoffInfoPpi
             );
             
  mde_8_edkii_vga_sprintf(5, "5b-%x",
    Status
  );
  mde_8_delay_s(2);
  if (Status == EFI_SUCCESS) {
    DEBUG ((DEBUG_INFO, "Vector Hand-off Info PPI is gotten, GUIDed HOB is created!\n"));
    VectorInfo = VectorHandoffInfoPpi->Info;
    Index      = 1;
    while (VectorInfo->Attribute != EFI_VECTOR_HANDOFF_LAST_ENTRY) {
      VectorInfo++;
      Index++;
    }

      
    mde_8_edkii_vga_sprintf(5, "5c-%x",
      Index
    );
    mde_8_delay_s(2);

    BuildGuidDataHob (
      &gEfiVectorHandoffInfoPpiGuid,
      VectorHandoffInfoPpi->Info,
      sizeof (EFI_VECTOR_HANDOFF_INFO) * Index
      );
      
      mde_8_edkii_vga_sprintf(5, "5d-%x",
        Index
      );
  }


    mde_8_edkii_vga_sprintf(6, "6a-%x",
      0
    );
    mde_8_delay_s(2);
  //
  // Allocate 128KB for the Stack
  //
  BaseOfStack = AllocatePages (EFI_SIZE_TO_PAGES (STACK_SIZE));
  
  mde_8_edkii_vga_sprintf(6, "6b-%x",
    BaseOfStack
  );
  mde_8_delay_s(2);
  ASSERT (BaseOfStack != NULL);

  //
  // Compute the top of the stack we were allocated. Pre-allocate a UINTN
  // for safety.
  //
  TopOfStack = (VOID *)((UINTN)BaseOfStack + EFI_SIZE_TO_PAGES (STACK_SIZE) * EFI_PAGE_SIZE - CPU_STACK_ALIGNMENT);
  TopOfStack = ALIGN_POINTER (TopOfStack, CPU_STACK_ALIGNMENT);

  //
  // Get the address and size of the GHCB pages
  //
  GhcbBase = (VOID *)PcdGet64 (PcdGhcbBase);
  GhcbSize = PcdGet64 (PcdGhcbSize);

  
  mde_8_edkii_vga_sprintf(6, "6c-%x-%x-%x-%x",
    BaseOfStack,
    TopOfStack,
    GhcbBase,
    GhcbSize
  );
  mde_8_delay_s(2);

  PageTables = 0;
  if (FeaturePcdGet (PcdDxeIplBuildPageTables)) {
    
    mde_8_edkii_vga_sprintf(7, "7a-%x",
      0
    );
    mde_8_delay_s(2);
    //
    // Create page table and save PageMapLevel4 to CR3
    //
    PageTables = CreateIdentityMappingPageTables (
                   (EFI_PHYSICAL_ADDRESS)(UINTN)BaseOfStack,
                   STACK_SIZE,
                   (EFI_PHYSICAL_ADDRESS)(UINTN)GhcbBase,
                   GhcbSize
                   );
                   
      mde_8_edkii_vga_sprintf(7, "7b-%x",
        PageTables
      );
  } else {
    //
    // Set NX for stack feature also require PcdDxeIplBuildPageTables be TRUE
    // for the DxeIpl and the DxeCore are both X64.
    //
    ASSERT (PcdGetBool (PcdSetNxForStack) == FALSE);
    ASSERT (PcdGetBool (PcdCpuStackGuard) == FALSE);
  }

  
  mde_8_edkii_vga_sprintf(9, "9a-%x",
    0
  );
  mde_8_delay_s(2);
  //
  // End of PEI phase signal
  //
  Status = PeiServicesInstallPpi (&gEndOfPeiSignalPpi);
  
  mde_8_edkii_vga_sprintf(9, "9b-%x-%x,%x-%x-%x-%x,%x",
    Status,
    gEndOfPeiSignalPpi.Flags,
    gEndOfPeiSignalPpi.Guid->Data1,
    gEndOfPeiSignalPpi.Guid->Data2,
    gEndOfPeiSignalPpi.Guid->Data3,
    gEndOfPeiSignalPpi.Guid->Data1,
    gEndOfPeiSignalPpi.Ppi
  );
  mde_8_delay_s(2);

  ASSERT_EFI_ERROR (Status);

  if (FeaturePcdGet (PcdDxeIplBuildPageTables)) {
    AsmWriteCr3 (PageTables);
  }

  
  mde_8_edkii_vga_sprintf(9, "9c-%x",
    0
  );
  mde_8_delay_s(2);
  //
  // Update the contents of BSP stack HOB to reflect the real stack info passed to DxeCore.
  //
  UpdateStackHob ((EFI_PHYSICAL_ADDRESS)(UINTN)BaseOfStack, STACK_SIZE);

  
  mde_8_edkii_vga_sprintf(9, "9d-%x-%x-%x",
    DxeCoreEntryPoint,
    *(unsigned int*)DxeCoreEntryPoint,
    ((unsigned int*)DxeCoreEntryPoint)[1],
  );
  mde_8_delay_s(2);

  //
  // Transfer the control to the entry point of DxeCore.
  //
  SwitchStack (
    (SWITCH_STACK_ENTRY_POINT)(UINTN)DxeCoreEntryPoint,
    HobList.Raw,
    NULL,
    TopOfStack
    );

  
  mde_8_edkii_vga_sprintf(9, "9e-%x",
    0
  );
  mde_8_delay_s(2);
}
