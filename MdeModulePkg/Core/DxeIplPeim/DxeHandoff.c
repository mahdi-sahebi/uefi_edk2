/** @file
  Generic version of arch-specific functionality for DxeLoad.

Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
Copyright (c) 2023, Google, LLC. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "DxeIpl.h"

#include <Ppi/MemoryAttribute.h>



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

#define mde_7__VGA_FB 0xB8000
#define mde_7__VGA_COLUMNS 80

char mde_7_g_buffer[80];

void mde_7_edkii_vga_clear(void);
void mde_7_edkii_vga_sprintf(unsigned int row, const char* format, ...);
void mde_7_edkii_vga_print(unsigned int line, const char *string);
void mde_7_edkii_vga_write_at_offset(unsigned int line, unsigned int offset, const char *string);
void mde_7_delay_s(int n);

// unsigned int mn_6_strlen(char *String)
// {
//     UINTN Length = 0;

//     if (String == NULL) {
//         return 0;
//     }

//     while (*String != '\0') {
//         String++;
//         Length++;
//     }

//     return Length;
// }

void mde_7_edkii_vga_write_at_offset(unsigned int line, unsigned int offset, const char *string)
{
	if (!string || line >= 25 || offset >= mde_7__VGA_COLUMNS)
		return;

	volatile unsigned short *p = (volatile unsigned short *)mde_7__VGA_FB + (mde_7__VGA_COLUMNS * line) + offset;
	unsigned int i, len = AsciiStrLen(string);

	for (i = 0; i < (mde_7__VGA_COLUMNS - offset); i++) {
		if (i < len)
			p[i] = 0x0F00 | (unsigned char)string[i];
		else
			p[i] = 0x0F00;
	}
}


void mde_7_edkii_vga_print(unsigned int line, const char *string)
{
	mde_7_edkii_vga_write_at_offset(line, 0, string);
}

void mde_7_edkii_vga_sprintf(
  unsigned int row,
  const char* format,
  ...)
{
  VA_LIST  marker;
  
  VA_START (marker, format);
  AsciiVSPrint(mde_7_g_buffer, sizeof(mde_7_g_buffer), format, marker);
  VA_END (marker);
  
  DEBUG ((DEBUG_ERROR, "[GX-DxeHandoff] row=%u %a\n", row, mde_7_g_buffer));
  mde_7_edkii_vga_print (row, mde_7_g_buffer);
}

void mde_7_edkii_vga_clear()
{
  for (unsigned int index = 0; index <= 24; index++) {
    mde_7_edkii_vga_print(index, "                                                                                                    ");
  }
}

void mde_7_edkii_vga_hex_dump(const unsigned char *addr, unsigned int len, int start_row) 
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
            mde_7_edkii_vga_write_at_offset(row, offset_pos++, buf);
        }
        {
            char last_nibble = ptr_val & 0x0F;
            char c = (last_nibble < 10) ? ('0' + last_nibble) : ('A' + last_nibble - 10);
            char buf[2] = {c, '\0'};
            mde_7_edkii_vga_write_at_offset(row, offset_pos++, buf);
        }
        mde_7_edkii_vga_write_at_offset(row, offset_pos++, ": ");
        
        // Write hex bytes
        for (j = 0; j < 16 && (i + j < len); j++) {
            unsigned char byte = addr[i + j];
            // Write high nibble
            char high = (byte >> 4) & 0x0F;
            char c1 = (high < 10) ? ('0' + high) : ('A' + high - 10);
            char buf1[2] = {c1, '\0'};
            mde_7_edkii_vga_write_at_offset(row, offset_pos++, buf1);
            // Write low nibble
            char low = byte & 0x0F;
            char c2 = (low < 10) ? ('0' + low) : ('A' + low - 10);
            char buf2[2] = {c2, '\0'};
            mde_7_edkii_vga_write_at_offset(row, offset_pos++, buf2);
            // Write space
            mde_7_edkii_vga_write_at_offset(row, offset_pos++, " ");
        }
        
        // Pad remaining hex spaces
        for (; j < 16; j++) {
            mde_7_edkii_vga_write_at_offset(row, offset_pos++, "   ");
        }
        
        // Write ASCII representation
        mde_7_edkii_vga_write_at_offset(row, offset_pos++, "  ");
        
        for (j = 0; j < 16 && (i + j < len); j++) {
            unsigned char byte = addr[i + j];
            char c = (byte >= 0x20 && byte <= 0x7e) ? (char)byte : '.';
            char buf[2] = {c, '\0'};
            mde_7_edkii_vga_write_at_offset(row, offset_pos + j, buf);
        }
    }
}



void mde_7_delay_s(int n)
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

  mde_7_edkii_vga_sprintf(23, "%x", t);
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
  VOID                        *BaseOfStack;
  VOID                        *TopOfStack;
  EFI_STATUS                  Status;
  EDKII_MEMORY_ATTRIBUTE_PPI  *MemoryPpi;

  mde_7_edkii_vga_clear();
  mde_7_edkii_vga_sprintf(0, "DxeHOa-%x-%x,%x-%x,%x-%x",
    DxeCoreEntryPoint,
    HobList,
    HobList.Capsule->BaseAddress,
    HobList.Capsule->Length,
    HobList.HandoffInformationTable->BootMode,
    HobList.HandoffInformationTable->EfiEndOfHobList
  );
  mde_7_edkii_vga_sprintf(1, "b-%x-%x,%x-%x,%x-%x",
    HobList.HandoffInformationTable->EfiFreeMemoryBottom,
    HobList.HandoffInformationTable->EfiFreeMemoryTop,
    HobList.HandoffInformationTable->EfiMemoryBottom,
    HobList.HandoffInformationTable->EfiMemoryTop,
    HobList.MemoryAllocation->AllocDescriptor.MemoryBaseAddress,
    HobList.MemoryAllocation->AllocDescriptor.MemoryLength
  );
  mde_7_edkii_vga_sprintf(2, "c-%x-%x,%x-%x,%x-%x",
    HobList.MemoryAllocation->AllocDescriptor.MemoryType,
    HobList.MemoryAllocation->AllocDescriptor.Name,
    HobList.MemoryAllocationStack->AllocDescriptor.MemoryBaseAddress,
    HobList.MemoryAllocationStack->AllocDescriptor.MemoryLength,
    HobList.MemoryAllocationStack->AllocDescriptor.MemoryType,
    HobList.MemoryAllocationStack->AllocDescriptor.Name
  );
  mde_7_edkii_vga_sprintf(3, "d-%x-%x",
    HobList.Cpu->Header.HobLength,
    HobList.Cpu->Header.HobType
  );
  mde_7_delay_s(2);

  //
  // Allocate 128KB for the Stack
  //
  BaseOfStack = AllocatePages (EFI_SIZE_TO_PAGES (STACK_SIZE));
  
  mde_7_edkii_vga_sprintf(4, "e-%x-%x-%x",
    BaseOfStack,
    STACK_SIZE,
    EFI_SIZE_TO_PAGES (STACK_SIZE)
  );
  mde_7_delay_s(2);
  ASSERT (BaseOfStack != NULL);

  if (PcdGetBool (PcdSetNxForStack)) {  
    mde_7_edkii_vga_sprintf(5, "fa-%x",
      0
    );
    mde_7_delay_s(2);
    Status = PeiServicesLocatePpi (
               &gEdkiiMemoryAttributePpiGuid,
               0,
               NULL,
               (VOID **)&MemoryPpi
               );
    mde_7_edkii_vga_sprintf(5, "fb-%x,%x-%x-%x-%x",
      Status,
      &gEdkiiMemoryAttributePpiGuid.Data1,
      &gEdkiiMemoryAttributePpiGuid.Data2,
      &gEdkiiMemoryAttributePpiGuid.Data3,
      &gEdkiiMemoryAttributePpiGuid.Data4,
    );
    mde_7_delay_s(2);
    ASSERT_EFI_ERROR (Status);

    Status = MemoryPpi->SetPermissions (
                          MemoryPpi,
                          (UINTN)BaseOfStack,
                          STACK_SIZE,
                          EFI_MEMORY_XP,
                          EFI_MEMORY_XP
                          );
                          
    mde_7_edkii_vga_sprintf(5, "fc-%x,%x-%x-%x-%x",
      Status,
      MemoryPpi,
      MemoryPpi->SetPermissions,
      (UINTN)BaseOfStack
    );
    mde_7_delay_s(2);
    ASSERT_EFI_ERROR (Status);
  }

  //
  // Compute the top of the stack we were allocated. Pre-allocate a UINTN
  // for safety.
  //
  TopOfStack = (VOID *)((UINTN)BaseOfStack + EFI_SIZE_TO_PAGES (STACK_SIZE) * EFI_PAGE_SIZE - CPU_STACK_ALIGNMENT);
  TopOfStack = ALIGN_POINTER (TopOfStack, CPU_STACK_ALIGNMENT);

  mde_7_edkii_vga_sprintf(6, "6a-%x,%x-%x-%x-%x",
    BaseOfStack,
    TopOfStack,
    EFI_SIZE_TO_PAGES(STACK_SIZE),
    EFI_PAGE_SIZE,
    CPU_STACK_ALIGNMENT
  );
  mde_7_delay_s(2);

  //
  // End of PEI phase signal
  //
  Status = PeiServicesInstallPpi (&gEndOfPeiSignalPpi);
  
  mde_7_edkii_vga_sprintf(7, "7a-%x-%x,%x-%x-%x-%x,%x",
    Status,
    gEndOfPeiSignalPpi.Flags,
    gEndOfPeiSignalPpi.Guid->Data1,
    gEndOfPeiSignalPpi.Guid->Data2,
    gEndOfPeiSignalPpi.Guid->Data3,
    gEndOfPeiSignalPpi.Guid->Data4,
    gEndOfPeiSignalPpi.Ppi
  );
  mde_7_delay_s(2);
  ASSERT_EFI_ERROR (Status);

  //
  // Update the contents of BSP stack HOB to reflect the real stack info passed to DxeCore.
  //
  UpdateStackHob ((EFI_PHYSICAL_ADDRESS)(UINTN)BaseOfStack, STACK_SIZE);

  mde_7_edkii_vga_sprintf(8, "8a-%x-%x,%x",
    HobList.Raw,
    TopOfStack,
    DxeCoreEntryPoint
  );
  mde_7_delay_s(2);

  //
  // Transfer the control to the entry point of DxeCore.
  //
  SwitchStack (
    (SWITCH_STACK_ENTRY_POINT)(UINTN)DxeCoreEntryPoint,
    HobList.Raw,
    NULL,
    TopOfStack
    );
}
