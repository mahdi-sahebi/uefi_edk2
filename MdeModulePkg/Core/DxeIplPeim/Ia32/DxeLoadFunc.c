/** @file
  Ia32-specific functionality for DxeLoad.

Copyright (c) 2006 - 2023, Intel Corporation. All rights reserved.<BR>
Copyright (c) 2017, AMD Incorporated. All rights reserved.<BR>

SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "DxeIpl.h"
#include "VirtualMemory.h"



/////////////////////////////////////////////////////
// #include <stdint.h>
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

#define mde_9__VGA_FB 0xB8000
#define mde_9__VGA_COLUMNS 80

char mde_8_g_buffer[80];

void mde_9_vga_clear(void);
void mde_9_vga_sprintf(unsigned int row, const char* format, ...);
void mde_9_vga_print(unsigned int line, const char *string);
void mde_9_vga_write_at_offset(unsigned int line, unsigned int offset, const char *string);
void mde_9_delay_s(int n);

 unsigned int mde_9_vga_strlen(const char *s) {
    unsigned int len = 0;
    if (s) while (s[len]) len++;
    return len;
}
void mde_9_vga_write_at_offset(unsigned int line, unsigned int offset, const char *string)
{
	if (!string)
		return;

	unsigned short *p = (unsigned short *)mde_9__VGA_FB + (mde_9__VGA_COLUMNS * line) + offset;
	unsigned int i, len = mde_9_vga_strlen(string);

	for (i = 0; i < (mde_9__VGA_COLUMNS - offset); i++) {
		if (i < len)
			p[i] = 0x0F00 | (unsigned char)string[i];
		else
			p[i] = 0x0F00;
	}
}


void mde_9_vga_print(unsigned int line, const char *string)
{
	mde_9_vga_write_at_offset(line, 0, string);
}

void mde_9_vga_sprintf(
  unsigned int row,
  const char* format,
  ...)
{
  VA_LIST  marker;
  
  VA_START (marker, format);
  AsciiVSPrint(mde_8_g_buffer, sizeof(mde_8_g_buffer), format, marker);
  VA_END (marker);
  
  mde_9_vga_print (row, mde_8_g_buffer);
}

void mde_9_vga_clear()
{
  for (unsigned int index = 0; index <= 24; index++) {
    mde_9_vga_print(index, "                                                                                                    ");
  }
}

void mde_9_vga_hex_dump(const unsigned char *addr, unsigned int len, int start_row) 
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
            mde_9_vga_write_at_offset(row, offset_pos++, buf);
        }
        {
            char last_nibble = ptr_val & 0x0F;
            char c = (last_nibble < 10) ? ('0' + last_nibble) : ('A' + last_nibble - 10);
            char buf[2] = {c, '\0'};
            mde_9_vga_write_at_offset(row, offset_pos++, buf);
        }
        mde_9_vga_write_at_offset(row, offset_pos++, ": ");
        
        // Write hex bytes
        for (j = 0; j < 16 && (i + j < len); j++) {
            unsigned char byte = addr[i + j];
            // Write high nibble
            char high = (byte >> 4) & 0x0F;
            char c1 = (high < 10) ? ('0' + high) : ('A' + high - 10);
            char buf1[2] = {c1, '\0'};
            mde_9_vga_write_at_offset(row, offset_pos++, buf1);
            // Write low nibble
            char low = byte & 0x0F;
            char c2 = (low < 10) ? ('0' + low) : ('A' + low - 10);
            char buf2[2] = {c2, '\0'};
            mde_9_vga_write_at_offset(row, offset_pos++, buf2);
            // Write space
            mde_9_vga_write_at_offset(row, offset_pos++, " ");
        }
        
        // Pad remaining hex spaces
        for (; j < 16; j++) {
            mde_9_vga_write_at_offset(row, offset_pos++, "   ");
        }
        
        // Write ASCII representation
        mde_9_vga_write_at_offset(row, offset_pos++, "  ");
        
        for (j = 0; j < 16 && (i + j < len); j++) {
            unsigned char byte = addr[i + j];
            char c = (byte >= 0x20 && byte <= 0x7e) ? (char)byte : '.';
            char buf[2] = {c, '\0'};
            mde_9_vga_write_at_offset(row, offset_pos + j, buf);
        }
    }
}


void mde_9_delay_s(int n)
{
  volatile unsigned long t = 25;
  volatile unsigned long x = (unsigned long)n * 10000UL;

  while (x--) {
    for (unsigned long i1 = 0; i1 < 1000UL; ++i1) {
        for (int i = 0; i < 100; ++i) {
            t = t * 14823424UL + x + 1UL;
        }
    }
  }

  mde_9_vga_sprintf(23, "%x", t);
}


/////////////////////////////////////////////////////




#define IDT_ENTRY_COUNT  32

typedef struct _X64_IDT_TABLE {
  //
  // Reserved 4 bytes preceding PeiService and IdtTable,
  // since IDT base address should be 8-byte alignment.
  //
  UINT32                     Reserved;
  CONST EFI_PEI_SERVICES     **PeiService;
  X64_IDT_GATE_DESCRIPTOR    IdtTable[IDT_ENTRY_COUNT];
} X64_IDT_TABLE;

//
// Global Descriptor Table (GDT)
//
GLOBAL_REMOVE_IF_UNREFERENCED IA32_GDT  gGdtEntries[] = {
  /* selector { Global Segment Descriptor                              } */
  /* 0x00 */ {
    { 0,      0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0, 0 }
  },                                                                      // null descriptor
  /* 0x08 */ {
    { 0xffff, 0, 0, 0x2, 1, 0, 1, 0xf, 0, 0, 1, 1, 0 }
  },                                                                      // linear data segment descriptor
  /* 0x10 */ {
    { 0xffff, 0, 0, 0xf, 1, 0, 1, 0xf, 0, 0, 1, 1, 0 }
  },                                                                      // linear code segment descriptor
  /* 0x18 */ {
    { 0xffff, 0, 0, 0x3, 1, 0, 1, 0xf, 0, 0, 1, 1, 0 }
  },                                                                      // system data segment descriptor
  /* 0x20 */ {
    { 0xffff, 0, 0, 0xa, 1, 0, 1, 0xf, 0, 0, 1, 1, 0 }
  },                                                                      // system code segment descriptor
  /* 0x28 */ {
    { 0,      0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0, 0 }
  },                                                                      // spare segment descriptor
  /* 0x30 */ {
    { 0xffff, 0, 0, 0x2, 1, 0, 1, 0xf, 0, 0, 1, 1, 0 }
  },                                                                      // system data segment descriptor
  /* 0x38 */ {
    { 0xffff, 0, 0, 0xa, 1, 0, 1, 0xf, 0, 1, 0, 1, 0 }
  },                                                                      // system code segment descriptor
  /* 0x40 */ {
    { 0,      0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0, 0 }
  },                                                                      // spare segment descriptor
};

//
// IA32 Gdt register
//
GLOBAL_REMOVE_IF_UNREFERENCED CONST IA32_DESCRIPTOR  gGdt = {
  sizeof (gGdtEntries) - 1,
  (UINTN)gGdtEntries
};

GLOBAL_REMOVE_IF_UNREFERENCED  IA32_DESCRIPTOR  gLidtDescriptor = {
  sizeof (X64_IDT_GATE_DESCRIPTOR) * IDT_ENTRY_COUNT - 1,
  0
};

/**
  Allocates and fills in the Page Directory and Page Table Entries to
  establish a 4G page table.

  @param[in] StackBase  Stack base address.
  @param[in] StackSize  Stack size.

  @return The address of page table.

**/
UINTN
Create4GPageTablesIa32Pae (
  IN EFI_PHYSICAL_ADDRESS  StackBase,
  IN UINTN                 StackSize
  )
{
  UINT8                           PhysicalAddressBits;
  EFI_PHYSICAL_ADDRESS            PhysicalAddress;
  UINTN                           IndexOfPdpEntries;
  UINTN                           IndexOfPageDirectoryEntries;
  UINT32                          NumberOfPdpEntriesNeeded;
  PAGE_MAP_AND_DIRECTORY_POINTER  *PageMap;
  PAGE_MAP_AND_DIRECTORY_POINTER  *PageDirectoryPointerEntry;
  PAGE_TABLE_ENTRY                *PageDirectoryEntry;
  UINTN                           TotalPagesNum;
  UINTN                           PageAddress;
  UINT64                          AddressEncMask;

  //
  // Make sure AddressEncMask is contained to smallest supported address field
  //
  AddressEncMask = PcdGet64 (PcdPteMemoryEncryptionAddressOrMask) & PAGING_1G_ADDRESS_MASK_64;

  PhysicalAddressBits = 32;

  //
  // Calculate the table entries needed.
  //
  NumberOfPdpEntriesNeeded = (UINT32)LShiftU64 (1, (PhysicalAddressBits - 30));

  TotalPagesNum = NumberOfPdpEntriesNeeded + 1;
  PageAddress   = (UINTN)AllocatePageTableMemory (TotalPagesNum);
  ASSERT (PageAddress != 0);

  PageMap      = (VOID *)PageAddress;
  PageAddress += SIZE_4KB;

  PageDirectoryPointerEntry = PageMap;
  PhysicalAddress           = 0;

  for (IndexOfPdpEntries = 0; IndexOfPdpEntries < NumberOfPdpEntriesNeeded; IndexOfPdpEntries++, PageDirectoryPointerEntry++) {
    //
    // Each Directory Pointer entries points to a page of Page Directory entires.
    // So allocate space for them and fill them in in the IndexOfPageDirectoryEntries loop.
    //
    PageDirectoryEntry = (VOID *)PageAddress;
    PageAddress       += SIZE_4KB;

    //
    // Fill in a Page Directory Pointer Entries
    //
    PageDirectoryPointerEntry->Uint64       = (UINT64)(UINTN)PageDirectoryEntry | AddressEncMask;
    PageDirectoryPointerEntry->Bits.Present = 1;

    for (IndexOfPageDirectoryEntries = 0; IndexOfPageDirectoryEntries < 512; IndexOfPageDirectoryEntries++, PageDirectoryEntry++, PhysicalAddress += SIZE_2MB) {
      if (  (IsNullDetectionEnabled () && (PhysicalAddress == 0))
         || (  (PhysicalAddress < StackBase + StackSize)
            && ((PhysicalAddress + SIZE_2MB) > StackBase)))
      {
        //
        // Need to split this 2M page that covers stack range.
        //
        Split2MPageTo4K (PhysicalAddress, (UINT64 *)PageDirectoryEntry, StackBase, StackSize, 0, 0);
      } else {
        //
        // Fill in the Page Directory entries
        //
        PageDirectoryEntry->Uint64         = (UINT64)PhysicalAddress | AddressEncMask;
        PageDirectoryEntry->Bits.ReadWrite = 1;
        PageDirectoryEntry->Bits.Present   = 1;
        PageDirectoryEntry->Bits.MustBe1   = 1;
      }
    }
  }

  for ( ; IndexOfPdpEntries < 512; IndexOfPdpEntries++, PageDirectoryPointerEntry++) {
    ZeroMem (
      PageDirectoryPointerEntry,
      sizeof (PAGE_MAP_AND_DIRECTORY_POINTER)
      );
  }

  //
  // Protect the page table by marking the memory used for page table to be
  // read-only.
  //
  EnablePageTableProtection ((UINTN)PageMap, 3);

  return (UINTN)PageMap;
}

/**
  The function will check if IA32 PAE is supported.

  @retval TRUE      IA32 PAE is supported.
  @retval FALSE     IA32 PAE is not supported.

**/
BOOLEAN
IsIa32PaeSupport (
  VOID
  )
{
  UINT32   RegEax;
  UINT32   RegEdx;
  BOOLEAN  Ia32PaeSupport;

  Ia32PaeSupport = FALSE;
  AsmCpuid (0x0, &RegEax, NULL, NULL, NULL);
  if (RegEax >= 0x1) {
    AsmCpuid (0x1, NULL, NULL, NULL, &RegEdx);
    if ((RegEdx & BIT6) != 0) {
      Ia32PaeSupport = TRUE;
    }
  }

  return Ia32PaeSupport;
}

/**
  The function will check if page table should be setup or not.

  @retval TRUE      Page table should be created.
  @retval FALSE     Page table should not be created.

**/
BOOLEAN
ToBuildPageTable (
  VOID
  )
{
  if (!IsIa32PaeSupport ()) {
    return FALSE;
  }

  if (IsNullDetectionEnabled ()) {
    return TRUE;
  }

  if (PcdGet8 (PcdHeapGuardPropertyMask) != 0) {
    return TRUE;
  }

  if (PcdGetBool (PcdCpuStackGuard)) {
    return TRUE;
  }

  if (IsEnableNonExecNeeded ()) {
    return TRUE;
  }

  return FALSE;
}

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
  EFI_STATUS                       Status;
  EFI_PHYSICAL_ADDRESS             BaseOfStack;
  EFI_PHYSICAL_ADDRESS             TopOfStack;
  UINTN                            PageTables;
  X64_IDT_GATE_DESCRIPTOR          *IdtTable;
  UINTN                            SizeOfTemplate;
  VOID                             *TemplateBase;
  EFI_PHYSICAL_ADDRESS             VectorAddress;
  UINT32                           Index;
  X64_IDT_TABLE                    *IdtTableForX64;
  EFI_VECTOR_HANDOFF_INFO          *VectorInfo;
  EFI_PEI_VECTOR_HANDOFF_INFO_PPI  *VectorHandoffInfoPpi;
  BOOLEAN                          BuildPageTablesIa32Pae;
  UINT32 stack_size = STACK_SIZE;


  mde_9_vga_clear();
  mde_9_vga_sprintf(0, "HOFIa32-a-%x-%x-%x-%x",//HOFIa32-a-
    (UINTN)DxeCoreEntryPoint,//6347e3ee
    (UINTN)HobList.Raw,//0
    HobList.Cpu->Header.HobLength,
    HobList.Cpu->Header.HobType
  );
  mde_9_vga_sprintf(1, "hb-%x-%x-%x-%x-%x-%x-%x",
    HobList.Cpu->SizeOfIoSpace,
    HobList.Cpu->SizeOfMemorySpace,
    HobList.FirmwareVolume2->BaseAddress,
    HobList.FirmwareVolume2->FileName.Data1,
    HobList.FirmwareVolume2->Header.HobLength,
    HobList.FirmwareVolume2->Length,
    HobList.Guid->Header.HobLength
  );
  mde_9_vga_sprintf(2, "hc-%x-%x-%x-%x-%x-%x-%x",
    HobList.HandoffInformationTable->BootMode,
    HobList.HandoffInformationTable->EfiEndOfHobList,
    HobList.HandoffInformationTable->EfiFreeMemoryBottom,
    HobList.HandoffInformationTable->EfiFreeMemoryTop,
    HobList.HandoffInformationTable->EfiMemoryBottom,
    HobList.HandoffInformationTable->EfiMemoryTop,
    HobList.HandoffInformationTable->Header.HobLength
  );
  mde_9_vga_hex_dump(HobList.Raw, 128, 4);
  mde_9_delay_s(5);
  mde_9_vga_clear();


  //
  // Clear page 0 and mark it as allocated if NULL pointer detection is enabled.
  //
  if (IsNullDetectionEnabled ()) {
    mde_9_vga_sprintf(0, "HOFIa32-b-%x-%x",
      DxeCoreEntryPoint,
      HobList
    );
    mde_9_delay_s(2);
    ClearFirst4KPage (HobList.Raw);
    
    mde_9_vga_sprintf(0, "HOFIa32-c-%x-%x",
      DxeCoreEntryPoint,
      HobList
    );
    mde_9_delay_s(2);
    BuildMemoryAllocationHob (0, EFI_PAGES_TO_SIZE (1), EfiBootServicesData);
    
    mde_9_vga_sprintf(0, "HOFIa32-d-%x-%x",
      DxeCoreEntryPoint,
      (UINTN)HobList.Raw
    );
    mde_9_delay_s(2);
  }
  
  mde_9_vga_sprintf(1, "DHOF32a-%x-%x",//DHOF32a-
    DxeCoreEntryPoint,//6347e3ee
    HobList//0
  );
  mde_9_delay_s(2);

  Status = PeiServicesAllocatePages (EfiBootServicesData, EFI_SIZE_TO_PAGES (STACK_SIZE), &BaseOfStack);
  
  mde_9_vga_sprintf(1, "DHOF32b-%x-%x",//DHOF32b-
    Status,//0
    BaseOfStack//63443000//63442000
  );
  mde_9_delay_s(2);
  ASSERT_EFI_ERROR (Status);

  if (FeaturePcdGet (PcdDxeIplSwitchToLongMode)) {
    //
    // Compute the top of the stack we were allocated, which is used to load X64 dxe core.
    // Pre-allocate a 32 bytes which conforms to x64 calling convention.
    //
    // The first four parameters to a function are passed in rcx, rdx, r8 and r9.
    // Any further parameters are pushed on the stack. Furthermore, space (4 * 8bytes) for the
    // register parameters is reserved on the stack, in case the called function
    // wants to spill them; this is important if the function is variadic.
    //
    TopOfStack = BaseOfStack + EFI_SIZE_TO_PAGES (STACK_SIZE) * EFI_PAGE_SIZE - 32;

    //
    //  x64 Calling Conventions requires that the stack must be aligned to 16 bytes
    //
    TopOfStack = (EFI_PHYSICAL_ADDRESS)(UINTN)ALIGN_POINTER (TopOfStack, 16);

    mde_9_vga_sprintf(2, "DH-2a-%x-%x",//DH-2a-
      TopOfStack,//63462fe0
      stack_size//?
    );
    mde_9_delay_s(2);

    //
    // Load the GDT of Go64. Since the GDT of 32-bit Tiano locates in the BS_DATA
    // memory, it may be corrupted when copying FV to high-end memory
    //
    AsmWriteGdtr (&gGdt);
    
    mde_9_vga_sprintf(2, "DH-2b-%x-%x-%x-%x",//DH-2b-
      TopOfStack,//63462fe0
      STACK_SIZE,//0
      gGdt.Base,//20000
      gGdt.Limit//634db020
    );
    mde_9_delay_s(2);

    //
    // Create page table and save PageMapLevel4 to CR3
    //
    PageTables = CreateIdentityMappingPageTables (BaseOfStack, STACK_SIZE, 0, 0);

    mde_9_vga_sprintf(2, "DH-2c-%x-%x-%x-%x,%x",//DH-2c-
      TopOfStack,//63462fe0
      STACK_SIZE,//0
      gGdt.Base,//20000
      gGdt.Limit,//634db020
      PageTables//47
    );
    mde_9_delay_s(2);

    //
    // End of PEI phase signal
    //
    PERF_EVENT_SIGNAL_BEGIN (gEndOfPeiSignalPpi.Guid);
    
    mde_9_vga_sprintf(2, "DH-2d-%x-%x-%x-%x,%x",//DH-2d-
      TopOfStack,//63462fe0
      STACK_SIZE,//0
      gGdt.Base,//20000
      gGdt.Limit,//634db020
      PageTables//47
    );
    mde_9_delay_s(2);

    Status = PeiServicesInstallPpi (&gEndOfPeiSignalPpi);
    
    mde_9_vga_sprintf(2, "DH-2e-%x-%x-%x-%x,%x",//DH-2e-
      TopOfStack,//63462fe0
      STACK_SIZE,//0
      gGdt.Base,//20000
      gGdt.Limit,//634db020
      PageTables//47
    );
    mde_9_delay_s(2);

    PERF_EVENT_SIGNAL_END (gEndOfPeiSignalPpi.Guid);
    ASSERT_EFI_ERROR (Status);

    //
    // Paging might be already enabled. To avoid conflict configuration,
    // disable paging first anyway.
    //
    AsmWriteCr0 (AsmReadCr0 () & (~BIT31));
    
    mde_9_vga_sprintf(2, "DH-2f-%x-%x-%x-%x,%x",//DH-2f-
      TopOfStack,//63462fe0
      STACK_SIZE,//0
      gGdt.Base,//20000
      gGdt.Limit,//634db020
      PageTables//47
    );
    mde_9_delay_s(2);

    AsmWriteCr3 (PageTables);

    mde_9_vga_sprintf(2, "DH-2g-%x-%x-%x-%x,%x",//DH-2g-%
      TopOfStack,//63462fe0
      STACK_SIZE,//0
      gGdt.Base,//20000
      gGdt.Limit,//634db020
      PageTables//47
    );
    mde_9_delay_s(2);

    //
    // Update the contents of BSP stack HOB to reflect the real stack info passed to DxeCore.
    //
    UpdateStackHob (BaseOfStack, STACK_SIZE);

    mde_9_vga_sprintf(2, "DI-2c-%x-%x-%x-%x,%x",//DI-2c-
      TopOfStack,//63462fe0
      STACK_SIZE,//0
      gGdt.Base,//20000
      gGdt.Limit,//634db020
      PageTables//47
    );
    mde_9_delay_s(2);

    SizeOfTemplate = AsmGetVectorTemplatInfo (&TemplateBase);

    mde_9_vga_sprintf(2, "DJ-2c-%x-%x",//DJ-2c-
      SizeOfTemplate,//a
      TemplateBase//634d96d0//634d8c40
    );
    mde_9_delay_s(2);

    Status = PeiServicesAllocatePages (
               EfiBootServicesData,
               EFI_SIZE_TO_PAGES (sizeof (X64_IDT_TABLE) + SizeOfTemplate * IDT_ENTRY_COUNT),
               &VectorAddress
               );
               
    mde_9_vga_sprintf(3, "DJ-3a-%x-%x",//DJ-3a-
      Status,//0
      VectorAddress//62fff000
    );
    mde_9_delay_s(2);

    ASSERT_EFI_ERROR (Status);

    //
    // Store EFI_PEI_SERVICES** in the 4 bytes immediately preceding IDT to avoid that
    // it may not be gotten correctly after IDT register is re-written.
    //
    IdtTableForX64             = (X64_IDT_TABLE *)(UINTN)VectorAddress;
    IdtTableForX64->PeiService = GetPeiServicesTablePointer ();

    VectorAddress = (EFI_PHYSICAL_ADDRESS)(UINTN)(IdtTableForX64 + 1);
    IdtTable      = IdtTableForX64->IdtTable;
    for (Index = 0; Index < IDT_ENTRY_COUNT; Index++) {
      IdtTable[Index].Ia32IdtEntry.Bits.GateType   =  0x8e;
      IdtTable[Index].Ia32IdtEntry.Bits.Reserved_0 =  0;
      IdtTable[Index].Ia32IdtEntry.Bits.Selector   =  SYS_CODE64_SEL;

      IdtTable[Index].Ia32IdtEntry.Bits.OffsetLow  = (UINT16)VectorAddress;
      IdtTable[Index].Ia32IdtEntry.Bits.OffsetHigh = (UINT16)(RShiftU64 (VectorAddress, 16));
      IdtTable[Index].Offset32To63                 = (UINT32)(RShiftU64 (VectorAddress, 32));
      IdtTable[Index].Reserved                     = 0;

      
      mde_9_vga_sprintf(3, "DJ-3b-%x-%x,%x",//DJ-3b-
        Status,       //0         
        VectorAddress,////62fff33e
        Index         //0
      );
      mde_9_delay_s(1);
      
      CopyMem ((VOID *)(UINTN)VectorAddress, TemplateBase, SizeOfTemplate);
      AsmVectorFixup ((VOID *)(UINTN)VectorAddress, (UINT8)Index);

      VectorAddress += SizeOfTemplate;
    }

    gLidtDescriptor.Base = (UINTN)IdtTable;

    mde_9_vga_sprintf(4, "DJ-4a-%x",//DJ-4a-
      gLidtDescriptor.Base//62fff008
    );
    mde_9_delay_s(2);
    
    //
    // Disable interrupt of Debug timer, since new IDT table cannot handle it.
    //
    SaveAndSetDebugTimerInterrupt (FALSE);

    mde_9_vga_sprintf(4, "DJ-4b-%x",
      gLidtDescriptor.Base//62fff008
    );
    mde_9_delay_s(2);
    
    AsmWriteIdtr (&gLidtDescriptor);

    mde_9_vga_sprintf(4, "DJ-4c-%x-%x,%x-%x",//DJ-4c-
      gLidtDescriptor.Base,//62fff008
      gLidtDescriptor.Limit,//1ff
      BaseOfStack,//63443000
      STACK_SIZE//0
    );
    mde_9_delay_s(2);
    
    DEBUG ((
      DEBUG_INFO,
      "%a() Stack Base: 0x%lx, Stack Size: 0x%x\n",
      __func__,
      BaseOfStack,
      STACK_SIZE
      ));

    mde_9_vga_clear();


/*
*/
    mde_9_vga_hex_dump((const unsigned char*)(UINTN)DxeCoreEntryPoint, 128, 0);
    mde_9_delay_s(3);

    UINT32 *Ptr = (UINT32 *)(UINTN)DxeCoreEntryPoint;
    mde_9_vga_sprintf(0, "D4k-%x-%x-%x-%x-%x-%x-%x",//D4k-
        Ptr[0], //ca894855
        Ptr[1], //41e58948
        Ptr[2], //41564157
        Ptr[3], //57544155
        Ptr[4], //81485356
        Ptr[5], //148ec
        Ptr[6]  //d894800
    );
    mde_9_vga_sprintf(1, "D4k-%x-%x-%x-%x-%x-%x-%x",//D4k-
      ((UINT32 *)(UINTN)DxeCoreEntryPoint)[7],//186ea
      ((UINT32 *)(UINTN)DxeCoreEntryPoint)[8],//e88d8948
      ((UINT32 *)(UINTN)DxeCoreEntryPoint)[9],//48fffffe
      ((UINT32 *)(UINTN)DxeCoreEntryPoint)[10],//8840d8d
      ((UINT32 *)(UINTN)DxeCoreEntryPoint)[11],//40e80001
      ((UINT32 *)(UINTN)DxeCoreEntryPoint)[12],//48ffffda
      ((UINT32 *)(UINTN)DxeCoreEntryPoint)[13]//8548c389
    );
    mde_9_vga_sprintf(2, "D4l-%x-%x-%x-%x-%x-%x",//D4l-
      ((UINT32 *)(UINTN)HobList.Raw)[0],//?
      ((UINT32 *)(UINTN)HobList.Raw)[1],//?
      ((UINT32 *)(UINTN)HobList.Raw)[2],//?
      ((UINT32 *)(UINTN)HobList.Raw)[3],//?
      ((UINT32 *)(UINTN)HobList.Raw)[4],//?
      ((UINT32 *)(UINTN)HobList.Raw)[5] //?
    );
    mde_9_vga_sprintf(3, "D4l-%x-%x-%x-%x-%x-%x",//D4l-
      ((UINT32 *)(UINTN)HobList.Raw)[6], //5f500000
      ((UINT32 *)(UINTN)HobList.Raw)[7], //0
      ((UINT32 *)(UINTN)HobList.Raw)[8], //62fff000
      ((UINT32 *)(UINTN)HobList.Raw)[9], //0
      ((UINT32 *)(UINTN)HobList.Raw)[10],//5f521d60
      ((UINT32 *)(UINTN)HobList.Raw)[11] //0
    );
    mde_9_vga_sprintf(4, "D4m-%x-%x",//D4m
      (UINTN)TopOfStack,//63462fe0?
      (UINTN)HobList.Raw,//?
      (UINTN)STACK_SIZE//0?
    );
    mde_9_delay_s(3); 

    mde_9_vga_clear();
    mde_9_vga_hex_dump((const unsigned char*)(void*)(UINTN)DxeCoreEntryPoint, 128, 0);
    mde_9_vga_hex_dump((const unsigned char*)(UINTN)HobList.Raw, 128, 9);
    mde_9_vga_sprintf(18, "D4k-%x-%x-%x",//D4k-
      DxeCoreEntryPoint,//6347e3ee?
      (UINTN)HobList.Raw,//0?
      (UINTN)TopOfStack//5f520000?
    );
    mde_9_delay_s(3); 
    //
    // Go to Long Mode and transfer control to DxeCore.
    // Interrupts will not get turned on until the CPU AP is loaded.
    // Call x64 drivers passing in single argument, a pointer to the HOBs.
    //
    AsmEnablePaging64 (
      SYS_CODE64_SEL,
      DxeCoreEntryPoint,
      (EFI_PHYSICAL_ADDRESS)(UINTN)(HobList.Raw),
      0,
      TopOfStack
      );
      
    mde_9_vga_sprintf(4, "DJ-4d-%x-%x,%x-%x,%x",
      gLidtDescriptor.Base,
      gLidtDescriptor.Limit,
      BaseOfStack,
      STACK_SIZE,
      DxeCoreEntryPoint
    );
    mde_9_delay_s(2);
  } else {
    
    mde_9_vga_sprintf(5, "DJ-5a-%x",
      0
    );
    mde_9_delay_s(2);
    //
    // Get Vector Hand-off Info PPI and build Guided HOB
    //
    Status = PeiServicesLocatePpi (
               &gEfiVectorHandoffInfoPpiGuid,
               0,
               NULL,
               (VOID **)&VectorHandoffInfoPpi
               );
               
    mde_9_vga_sprintf(5, "DJ-5b-%x,%x-%x-%x-%x,-%x-%x-%x",
      Status,
      gEfiVectorHandoffInfoPpiGuid.Data1,
      gEfiVectorHandoffInfoPpiGuid.Data2,
      gEfiVectorHandoffInfoPpiGuid.Data3,
      gEfiVectorHandoffInfoPpiGuid.Data4,
      VectorHandoffInfoPpi->Info->Attribute,
      VectorHandoffInfoPpi->Info->Owner.Data1,
      VectorHandoffInfoPpi->Info->VectorNumber
    );
    mde_9_delay_s(2);

    if (Status == EFI_SUCCESS) {
      DEBUG ((DEBUG_INFO, "Vector Hand-off Info PPI is gotten, GUIDed HOB is created!\n"));
      VectorInfo = VectorHandoffInfoPpi->Info;
      Index      = 1;
      while (VectorInfo->Attribute != EFI_VECTOR_HANDOFF_LAST_ENTRY) {
        VectorInfo++;
        Index++;
      }
           
      mde_9_vga_sprintf(6, "DJ-6a-%x",
        Index
      );
      mde_9_delay_s(2);
      
      BuildGuidDataHob (
        &gEfiVectorHandoffInfoPpiGuid,
        VectorHandoffInfoPpi->Info,
        sizeof (EFI_VECTOR_HANDOFF_INFO) * Index
        );
        
      mde_9_vga_sprintf(6, "DJ-6b-%x,-%x-%x-%x-%x,-%x-%x",
        Index,
        gEfiVectorHandoffInfoPpiGuid.Data1,
        gEfiVectorHandoffInfoPpiGuid.Data2,
        gEfiVectorHandoffInfoPpiGuid.Data3,
        gEfiVectorHandoffInfoPpiGuid.Data4,
        VectorHandoffInfoPpi->Info->Attribute,
        VectorHandoffInfoPpi->Info->VectorNumber
      );
      mde_9_delay_s(2);
      
    }

    //
    // Compute the top of the stack we were allocated. Pre-allocate a UINTN
    // for safety.
    //
    TopOfStack = BaseOfStack + EFI_SIZE_TO_PAGES (STACK_SIZE) * EFI_PAGE_SIZE - CPU_STACK_ALIGNMENT;
    TopOfStack = (EFI_PHYSICAL_ADDRESS)(UINTN)ALIGN_POINTER (TopOfStack, CPU_STACK_ALIGNMENT);

    PageTables             = 0;
    BuildPageTablesIa32Pae = ToBuildPageTable ();
    if (BuildPageTablesIa32Pae) {
      PageTables = Create4GPageTablesIa32Pae (BaseOfStack, STACK_SIZE);
      if (IsEnableNonExecNeeded ()) {
        EnableExecuteDisableBit ();
      }
    }

    mde_9_vga_sprintf(7, "DJ-7a-%x,-%x-%x",
      PageTables,
      BuildPageTablesIa32Pae,
      TopOfStack
    );
    mde_9_delay_s(2);
      
    //
    // End of PEI phase signal
    //
    PERF_EVENT_SIGNAL_BEGIN (gEndOfPeiSignalPpi.Guid);
    Status = PeiServicesInstallPpi (&gEndOfPeiSignalPpi);
    
    mde_9_vga_sprintf(7, "DJ-7b-%x-%x,-%x-%x",
      Status,
      PageTables,
      BuildPageTablesIa32Pae,
      TopOfStack
    );
    mde_9_delay_s(2);

    PERF_EVENT_SIGNAL_END (gEndOfPeiSignalPpi.Guid);
    ASSERT_EFI_ERROR (Status);

    if (BuildPageTablesIa32Pae) {
      //
      // Paging might be already enabled. To avoid conflict configuration,
      // disable paging first anyway.
      //
      AsmWriteCr0 (AsmReadCr0 () & (~BIT31));
      AsmWriteCr3 (PageTables);
      //
      // Set Physical Address Extension (bit 5 of CR4).
      //
      AsmWriteCr4 (AsmReadCr4 () | BIT5);
      
      mde_9_vga_sprintf(7, "DJ-7c-%x-%x,-%x-%x",
        Status,
        PageTables,
        BuildPageTablesIa32Pae,
        TopOfStack
      );
      mde_9_delay_s(2);
    }

    
    mde_9_vga_sprintf(8, "DJ-8a-%x-%x",
      BaseOfStack,
      STACK_SIZE
    );
    mde_9_delay_s(2);
    //
    // Update the contents of BSP stack HOB to reflect the real stack info passed to DxeCore.
    //
    UpdateStackHob (BaseOfStack, STACK_SIZE);

    mde_9_vga_sprintf(8, "DJ-8b-%x-%x",
      BaseOfStack,
      STACK_SIZE
    );
    mde_9_delay_s(2);

    DEBUG ((
      DEBUG_INFO,
      "%a() Stack Base: 0x%lx, Stack Size: 0x%x\n",
      __func__,
      BaseOfStack,
      STACK_SIZE
      ));

    //
    // Transfer the control to the entry point of DxeCore.
    //
    if (BuildPageTablesIa32Pae) {
      
      mde_9_vga_sprintf(9, "DJ-9a-%x",
        0
      );
      mde_9_delay_s(2);

      AsmEnablePaging32 (
        (SWITCH_STACK_ENTRY_POINT)(UINTN)DxeCoreEntryPoint,
        HobList.Raw,
        NULL,
        (VOID *)(UINTN)TopOfStack
        );
        
      mde_9_vga_sprintf(9, "DJ-9b-%x-%x",
        DxeCoreEntryPoint,
        *(UINT32 *)(UINTN)HobList.Raw
      );
      mde_9_delay_s(2);

    } else {
      
      mde_9_vga_sprintf(9, "DJ-9c-%x",
        0
      );
      mde_9_delay_s(2);

      SwitchStack (
        (SWITCH_STACK_ENTRY_POINT)(UINTN)DxeCoreEntryPoint,
        HobList.Raw,
        NULL,
        (VOID *)(UINTN)TopOfStack
        );
        
      mde_9_vga_sprintf(9, "DJ-9d-%x-%x",
        DxeCoreEntryPoint,
        *(UINT32 *)(UINTN)HobList.Raw
      );
      mde_9_delay_s(2);

    }

    mde_9_vga_sprintf(9, "DJ-9z-%x",
      0
    );
    mde_9_delay_s(2);

  }
}
