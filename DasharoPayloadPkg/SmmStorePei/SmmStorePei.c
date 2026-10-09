/** @file
  This module installs gVariableFlashInfoHobGuid PPI that serves as a source of
  information about variable storage for VariableFlashInfoLib.

Copyright (c) 2025, 3mdeb Sp. z o.o. All rights reserved.<BR>
SPDX-License-Identifier: GPL-2.0-or-later

**/

#include <PiPei.h>

#include <Coreboot.h>
#include <Guid/VariableFlashInfo.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BlParseLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/PeiServicesLib.h>






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

#define mde_2__VGA_FB 0xB8000
#define mde_2__VGA_COLUMNS 80

char mde_2_g_buffer[80];

void mde_5_edkii_vga_write_at_offset(unsigned int line, unsigned int offset, const char *string)
{
	if (!string)
		return;

	unsigned short *p = (unsigned short *)mde_2__VGA_FB + (mde_2__VGA_COLUMNS * line) + offset;
	unsigned int i, len = AsciiStrLen(string);

	for (i = 0; i < (mde_2__VGA_COLUMNS - offset); i++) {
		if (i < len)
			p[i] = 0x0F00 | (unsigned char)string[i];
		else
			p[i] = 0x0F00;
	}
}


void mde_5_edkii_vga_print(unsigned int line, const char *string)
{
	mde_5_edkii_vga_write_at_offset(line, 0, string);
}

void mde_5_edkii_vga_sprintf(
  unsigned int row,
  const char* format,
  ...)
{
  VA_LIST  marker;
  
  VA_START (marker, format);
  AsciiVSPrint(mde_2_g_buffer, sizeof(mde_2_g_buffer), format, marker);
  VA_END (marker);
  
  mde_5_edkii_vga_print (row, mde_2_g_buffer);
}

void mde_5_edkii_vga_clear()
{
  mde_5_edkii_vga_print(0, "                                                                                                    ");
  mde_5_edkii_vga_print(1, "                                                                                                    ");
  mde_5_edkii_vga_print(2, "                                                                                                    ");
  mde_5_edkii_vga_print(3, "                                                                                                    ");
  mde_5_edkii_vga_print(4, "                                                                                                    ");
  mde_5_edkii_vga_print(5, "                                                                                                    ");
  mde_5_edkii_vga_print(6, "                                                                                                    ");
  mde_5_edkii_vga_print(7, "                                                                                                    ");
  mde_5_edkii_vga_print(8, "                                                                                                    ");
  mde_5_edkii_vga_print(9, "                                                                                                    ");
  mde_5_edkii_vga_print(10, "                                                                                                    ");
  mde_5_edkii_vga_print(11, "                                                                                                    ");
  mde_5_edkii_vga_print(12, "                                                                                                    ");
  mde_5_edkii_vga_print(13, "                                                                                                    ");
  mde_5_edkii_vga_print(14, "                                                                                                    ");
  mde_5_edkii_vga_print(15, "                                                                                                    ");
  mde_5_edkii_vga_print(16, "                                                                                                    ");
  mde_5_edkii_vga_print(17, "                                                                                                    ");
  mde_5_edkii_vga_print(18, "                                                                                                    ");
  mde_5_edkii_vga_print(19, "                                                                                                    ");
  mde_5_edkii_vga_print(20, "                                                                                                    ");
  mde_5_edkii_vga_print(21, "                                                                                                    ");
  mde_5_edkii_vga_print(22, "                                                                                                    ");
  mde_5_edkii_vga_print(23, "                                                                                                    ");
  mde_5_edkii_vga_print(24, "                                                                                                    ");
}

void mde_5_edkii_vga_hex_dump(const unsigned char *addr, unsigned int len, int start_row) 
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
            mde_5_edkii_vga_write_at_offset(row, offset_pos++, buf);
        }
        {
            char last_nibble = ptr_val & 0x0F;
            char c = (last_nibble < 10) ? ('0' + last_nibble) : ('A' + last_nibble - 10);
            char buf[2] = {c, '\0'};
            mde_5_edkii_vga_write_at_offset(row, offset_pos++, buf);
        }
        mde_5_edkii_vga_write_at_offset(row, offset_pos++, ": ");
        
        // Write hex bytes
        for (j = 0; j < 16 && (i + j < len); j++) {
            unsigned char byte = addr[i + j];
            // Write high nibble
            char high = (byte >> 4) & 0x0F;
            char c1 = (high < 10) ? ('0' + high) : ('A' + high - 10);
            char buf1[2] = {c1, '\0'};
            mde_5_edkii_vga_write_at_offset(row, offset_pos++, buf1);
            // Write low nibble
            char low = byte & 0x0F;
            char c2 = (low < 10) ? ('0' + low) : ('A' + low - 10);
            char buf2[2] = {c2, '\0'};
            mde_5_edkii_vga_write_at_offset(row, offset_pos++, buf2);
            // Write space
            mde_5_edkii_vga_write_at_offset(row, offset_pos++, " ");
        }
        
        // Pad remaining hex spaces
        for (; j < 16; j++) {
            mde_5_edkii_vga_write_at_offset(row, offset_pos++, "   ");
        }
        
        // Write ASCII representation
        mde_5_edkii_vga_write_at_offset(row, offset_pos++, "  ");
        
        for (j = 0; j < 16 && (i + j < len); j++) {
            unsigned char byte = addr[i + j];
            char c = (byte >= 0x20 && byte <= 0x7e) ? (char)byte : '.';
            char buf[2] = {c, '\0'};
            mde_5_edkii_vga_write_at_offset(row, offset_pos + j, buf);
        }
    }
}



/////////////////////////////////////////////////////






EFI_PEI_PPI_DESCRIPTOR  mPpiListVariable = {
  (EFI_PEI_PPI_DESCRIPTOR_PPI | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST),
  &gVariableFlashInfoHobGuid,
  NULL
};

/**
  Main entry for SMMSTORE PEIM.

  @param[in]  FileHandle              Handle of the file being invoked.
  @param[in]  PeiServices             Pointer to PEI Services table.

  @retval EFI_SUCCESS  On success.
  @retval Others       On trouble with SMMSTORE information or PPI.

**/
EFI_STATUS
EFIAPI
SmmStorePeiInitialize (
  IN       EFI_PEI_FILE_HANDLE  FileHandle,
  IN CONST EFI_PEI_SERVICES     **PeiServices
  )
{
  EFI_STATUS           Status;
  SMMSTORE_INFO        SmmStoreInfo;
  VARIABLE_FLASH_INFO  VariableFlashInfo;
  UINT32               NvStorageBase;
  UINT32               NvStorageSize;
  UINT32               NvVariableSize;
  UINT32               FtwWorkingSize;
  UINT32               FtwSpareSize;


  mde_5_edkii_vga_clear();
  mde_5_edkii_vga_print(0, "SmmStore");


  Status = ParseSMMSTOREInfo (&SmmStoreInfo);
  
  mde_5_edkii_vga_sprintf(0, "SMM-%x-%x-%x-%x,-%x-%x-%x",
    Status,//0
    SmmStoreInfo.ComBuffer,//63562000
    SmmStoreInfo.ComBufferSize,//0
    SmmStoreInfo.NumBlocks,//10000
    SmmStoreInfo.BlockSize,//4
    SmmStoreInfo.MmioAddress,//10000
    SmmStoreInfo.ApmCmd//ff000000
  );

  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "SmmStorePei: ParseSMMSTOREInfo() failed: %r.\n",
      Status
      ));
    return Status;
  }

  NvStorageSize = SmmStoreInfo.NumBlocks * SmmStoreInfo.BlockSize;
  NvStorageBase = SmmStoreInfo.MmioAddress;
  DEBUG ((
    DEBUG_INFO,
    "SmmStorePei: NvStorageBase: 0x%x, NvStorageSize: 0x%x\n",
    NvStorageBase,
    NvStorageSize
    ));

  FtwSpareSize   = (SmmStoreInfo.NumBlocks / 2) * SmmStoreInfo.BlockSize;
  FtwWorkingSize = SmmStoreInfo.BlockSize;
  NvVariableSize = NvStorageSize - FtwSpareSize - FtwWorkingSize;
  
  mde_5_edkii_vga_sprintf(1, "1a-%x-%x-%x-%x,%x",
    NvStorageSize,
    NvStorageBase,
    FtwSpareSize,
    FtwWorkingSize,
    NvVariableSize
  );

  if (NvVariableSize >= 0x80000000) {
    DEBUG ((
      DEBUG_ERROR,
      "SmmStorePei: NvStorageSize is too large: 0x%x > 0x80000000.\n",
      NvVariableSize
      ));
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (&VariableFlashInfo, sizeof (VariableFlashInfo));

  mde_5_edkii_vga_sprintf(1, "1b-%x-%x-%x-%x,%x",
    NvStorageSize,//40000
    NvStorageBase,//ff000000
    FtwSpareSize,//20000
    FtwWorkingSize,//10000
    NvVariableSize//10000
  );

  VariableFlashInfo.Version               = VARIABLE_FLASH_INFO_HOB_VERSION;
  VariableFlashInfo.NvVariableBaseAddress = NvStorageBase;
  VariableFlashInfo.NvVariableLength      = NvVariableSize;
  VariableFlashInfo.FtwSpareBaseAddress   = NvStorageBase + NvVariableSize + FtwWorkingSize;
  VariableFlashInfo.FtwSpareLength        = FtwSpareSize;
  VariableFlashInfo.FtwWorkingBaseAddress = NvStorageBase + NvVariableSize;
  VariableFlashInfo.FtwWorkingLength      = FtwWorkingSize;

  
  mde_5_edkii_vga_sprintf(2, "2a-%x-%x-%x-%x,%x,%x",
    VariableFlashInfo.NvVariableBaseAddress,
    VariableFlashInfo.NvVariableLength,
    VariableFlashInfo.FtwSpareBaseAddress,
    VariableFlashInfo.FtwSpareLength ,
    VariableFlashInfo.FtwWorkingBaseAddress,
    VariableFlashInfo.FtwWorkingLength
  );

  BuildGuidDataHob (&gVariableFlashInfoHobGuid, &VariableFlashInfo, sizeof (VariableFlashInfo));
  
  
  mde_5_edkii_vga_sprintf(2, "2b-%x-%x-%x-%x,%x,%x",
    VariableFlashInfo.NvVariableBaseAddress,//ff000000
    VariableFlashInfo.NvVariableLength,//0
    VariableFlashInfo.FtwSpareBaseAddress,//10000
    VariableFlashInfo.FtwSpareLength ,//0
    VariableFlashInfo.FtwWorkingBaseAddress,//ff020000
    VariableFlashInfo.FtwWorkingLength//0
  );

  
  mde_5_edkii_vga_sprintf(3, "3a-%x",
    gVariableFlashInfoHobGuid//5d11c653
  );

  EFI_STATUS res = PeiServicesInstallPpi (&mPpiListVariable);

  mde_5_edkii_vga_sprintf(4, "4a-%x-%x-%x-%x",
    res,//0
    mPpiListVariable.Flags,//80000010
    mPpiListVariable.Guid,//82700c
    mPpiListVariable.Ppi//0
  );


   // while (1) {};
  return res;
}
