/** @file
  This driver installs gEdkiiFaultTolerantWriteGuid PPI to inform
  the check for FTW last write data has been done.

Copyright (c) 2013 - 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiPei.h>

#include <Guid/SystemNvDataGuid.h>
#include <Guid/FaultTolerantWrite.h>
#include <Library/PeiServicesLib.h>
#include <Library/PcdLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/HobLib.h>
#include <Library/SafeIntLib.h>
#include <Library/VariableFlashInfoLib.h>
#include <Guid/VariableFlashInfo.h>
// #include <Library/TimerLib.h>




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

void mde_4_edkii_vga_write_at_offset(unsigned int line, unsigned int offset, const char *string)
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


void mde_4_edkii_vga_print(unsigned int line, const char *string)
{
	mde_4_edkii_vga_write_at_offset(line, 0, string);
}

void mde_4_edkii_vga_sprintf(
  unsigned int row,
  const char* format,
  ...)
{
  VA_LIST  marker;
  
  VA_START (marker, format);
  AsciiVSPrint(mde_2_g_buffer, sizeof(mde_2_g_buffer), format, marker);
  VA_END (marker);
  
  mde_4_edkii_vga_print (row, mde_2_g_buffer);
}

void mde_4_edkii_vga_clear()
{
  mde_4_edkii_vga_print(0, "                                                                                                    ");
  mde_4_edkii_vga_print(1, "                                                                                                    ");
  mde_4_edkii_vga_print(2, "                                                                                                    ");
  mde_4_edkii_vga_print(3, "                                                                                                    ");
  mde_4_edkii_vga_print(4, "                                                                                                    ");
  mde_4_edkii_vga_print(5, "                                                                                                    ");
  mde_4_edkii_vga_print(6, "                                                                                                    ");
  mde_4_edkii_vga_print(7, "                                                                                                    ");
  mde_4_edkii_vga_print(8, "                                                                                                    ");
  mde_4_edkii_vga_print(9, "                                                                                                    ");
  mde_4_edkii_vga_print(10, "                                                                                                    ");
  mde_4_edkii_vga_print(11, "                                                                                                    ");
  mde_4_edkii_vga_print(12, "                                                                                                    ");
  mde_4_edkii_vga_print(13, "                                                                                                    ");
  mde_4_edkii_vga_print(14, "                                                                                                    ");
  mde_4_edkii_vga_print(15, "                                                                                                    ");
  mde_4_edkii_vga_print(16, "                                                                                                    ");
  mde_4_edkii_vga_print(17, "                                                                                                    ");
  mde_4_edkii_vga_print(18, "                                                                                                    ");
  mde_4_edkii_vga_print(19, "                                                                                                    ");
  mde_4_edkii_vga_print(20, "                                                                                                    ");
  mde_4_edkii_vga_print(21, "                                                                                                    ");
  mde_4_edkii_vga_print(22, "                                                                                                    ");
  mde_4_edkii_vga_print(23, "                                                                                                    ");
  mde_4_edkii_vga_print(24, "                                                                                                    ");
}

void mde_4_edkii_vga_hex_dump(const unsigned char *addr, unsigned int len, int start_row) 
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
            mde_4_edkii_vga_write_at_offset(row, offset_pos++, buf);
        }
        {
            char last_nibble = ptr_val & 0x0F;
            char c = (last_nibble < 10) ? ('0' + last_nibble) : ('A' + last_nibble - 10);
            char buf[2] = {c, '\0'};
            mde_4_edkii_vga_write_at_offset(row, offset_pos++, buf);
        }
        mde_4_edkii_vga_write_at_offset(row, offset_pos++, ": ");
        
        // Write hex bytes
        for (j = 0; j < 16 && (i + j < len); j++) {
            unsigned char byte = addr[i + j];
            // Write high nibble
            char high = (byte >> 4) & 0x0F;
            char c1 = (high < 10) ? ('0' + high) : ('A' + high - 10);
            char buf1[2] = {c1, '\0'};
            mde_4_edkii_vga_write_at_offset(row, offset_pos++, buf1);
            // Write low nibble
            char low = byte & 0x0F;
            char c2 = (low < 10) ? ('0' + low) : ('A' + low - 10);
            char buf2[2] = {c2, '\0'};
            mde_4_edkii_vga_write_at_offset(row, offset_pos++, buf2);
            // Write space
            mde_4_edkii_vga_write_at_offset(row, offset_pos++, " ");
        }
        
        // Pad remaining hex spaces
        for (; j < 16; j++) {
            mde_4_edkii_vga_write_at_offset(row, offset_pos++, "   ");
        }
        
        // Write ASCII representation
        mde_4_edkii_vga_write_at_offset(row, offset_pos++, "  ");
        
        for (j = 0; j < 16 && (i + j < len); j++) {
            unsigned char byte = addr[i + j];
            char c = (byte >= 0x20 && byte <= 0x7e) ? (char)byte : '.';
            char buf[2] = {c, '\0'};
            mde_4_edkii_vga_write_at_offset(row, offset_pos + j, buf);
        }
    }
}



/////////////////////////////////////////////////////





EFI_PEI_PPI_DESCRIPTOR  mPpiListVariable = {
  (EFI_PEI_PPI_DESCRIPTOR_PPI | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST),
  &gEdkiiFaultTolerantWriteGuid,
  NULL
};

/**
  Get the last Write Header pointer.
  The last write header is the header whose 'complete' state hasn't been set.
  After all, this header may be a EMPTY header entry for next Allocate.


  @param FtwWorkSpaceHeader Pointer of the working block header
  @param FtwWorkSpaceSize   Size of the work space
  @param FtwWriteHeader     Pointer to retrieve the last write header

  @retval  EFI_SUCCESS      Get the last write record successfully
  @retval  EFI_ABORTED      The FTW work space is damaged

**/
EFI_STATUS
FtwGetLastWriteHeader (
  IN EFI_FAULT_TOLERANT_WORKING_BLOCK_HEADER  *FtwWorkSpaceHeader,
  IN UINTN                                    FtwWorkSpaceSize,
  OUT EFI_FAULT_TOLERANT_WRITE_HEADER         **FtwWriteHeader
  )
{
  UINTN                            Offset;
  EFI_FAULT_TOLERANT_WRITE_HEADER  *FtwHeader;

  *FtwWriteHeader = NULL;
  FtwHeader       = (EFI_FAULT_TOLERANT_WRITE_HEADER *)(FtwWorkSpaceHeader + 1);
  Offset          = sizeof (EFI_FAULT_TOLERANT_WORKING_BLOCK_HEADER);

  while (FtwHeader->Complete == FTW_VALID_STATE) {
    Offset += FTW_WRITE_TOTAL_SIZE (FtwHeader->NumberOfWrites, FtwHeader->PrivateDataSize);
    //
    // If Offset exceed the FTW work space boudary, return error.
    //
    if (Offset >= FtwWorkSpaceSize) {
      *FtwWriteHeader = FtwHeader;
      return EFI_ABORTED;
    }

    FtwHeader = (EFI_FAULT_TOLERANT_WRITE_HEADER *)((UINT8 *)FtwWorkSpaceHeader + Offset);
  }

  //
  // Last write header is found
  //
  *FtwWriteHeader = FtwHeader;

  return EFI_SUCCESS;
}

/**
  Get the last Write Record pointer. The last write Record is the Record
  whose DestinationCompleted state hasn't been set. After all, this Record
  may be a EMPTY record entry for next write.


  @param FtwWriteHeader  Pointer to the write record header
  @param FtwWriteRecord  Pointer to retrieve the last write record

  @retval EFI_SUCCESS        Get the last write record successfully
  @retval EFI_ABORTED        The FTW work space is damaged

**/
EFI_STATUS
FtwGetLastWriteRecord (
  IN EFI_FAULT_TOLERANT_WRITE_HEADER   *FtwWriteHeader,
  OUT EFI_FAULT_TOLERANT_WRITE_RECORD  **FtwWriteRecord
  )
{
  UINTN                            Index;
  EFI_FAULT_TOLERANT_WRITE_RECORD  *FtwRecord;

  *FtwWriteRecord = NULL;
  FtwRecord       = (EFI_FAULT_TOLERANT_WRITE_RECORD *)(FtwWriteHeader + 1);

  //
  // Try to find the last write record "that has not completed"
  //
  for (Index = 0; Index < FtwWriteHeader->NumberOfWrites; Index += 1) {
    if (FtwRecord->DestinationComplete != FTW_VALID_STATE) {
      //
      // The last write record is found
      //
      *FtwWriteRecord = FtwRecord;
      return EFI_SUCCESS;
    }

    FtwRecord++;

    if (FtwWriteHeader->PrivateDataSize != 0) {
      FtwRecord = (EFI_FAULT_TOLERANT_WRITE_RECORD *)((UINTN)FtwRecord + (UINTN)FtwWriteHeader->PrivateDataSize);
    }
  }

  //
  //  if Index == NumberOfWrites, then
  //  the last record has been written successfully,
  //  but the Header->Complete Flag has not been set.
  //  also return the last record.
  //
  if (Index == FtwWriteHeader->NumberOfWrites) {
    *FtwWriteRecord = (EFI_FAULT_TOLERANT_WRITE_RECORD *)((UINTN)FtwRecord - FTW_RECORD_SIZE (FtwWriteHeader->PrivateDataSize));
    return EFI_SUCCESS;
  }

  return EFI_ABORTED;
}

/**
  Check to see if it is a valid work space.


  @param WorkingHeader   Pointer of working block header
  @param WorkingLength   Working block length

  @retval TRUE          The work space is valid.
  @retval FALSE         The work space is invalid.

**/
BOOLEAN
IsValidWorkSpace (
  IN EFI_FAULT_TOLERANT_WORKING_BLOCK_HEADER  *WorkingHeader,
  IN UINTN                                    WorkingLength
  )
{
  UINT8  Data;

  if (WorkingHeader == NULL) {
    return FALSE;
  }

  if ((WorkingHeader->WorkingBlockValid != FTW_VALID_STATE) || (WorkingHeader->WorkingBlockInvalid == FTW_VALID_STATE)) {
    DEBUG ((DEBUG_ERROR, "FtwPei: Work block header valid bit check error\n"));
    return FALSE;
  }

  if (WorkingHeader->WriteQueueSize != (WorkingLength - sizeof (EFI_FAULT_TOLERANT_WORKING_BLOCK_HEADER))) {
    DEBUG ((DEBUG_ERROR, "FtwPei: Work block header WriteQueueSize check error\n"));
    return FALSE;
  }

  //
  // Check signature with gEdkiiWorkingBlockSignatureGuid
  //
  if (!CompareGuid (&gEdkiiWorkingBlockSignatureGuid, &WorkingHeader->Signature)) {
    DEBUG ((DEBUG_ERROR, "FtwPei: Work block header signature check error, it should be gEdkiiWorkingBlockSignatureGuid\n"));
    //
    // To be compatible with old signature gEfiSystemNvDataFvGuid.
    //
    if (!CompareGuid (&gEfiSystemNvDataFvGuid, &WorkingHeader->Signature)) {
      return FALSE;
    } else {
      Data = *(UINT8 *)(WorkingHeader + 1);
      if (Data != 0xff) {
        DEBUG ((DEBUG_ERROR, "FtwPei: Old format FTW structure can't be handled\n"));
        ASSERT (FALSE);
        return FALSE;
      }
    }
  }

  return TRUE;
}

unsigned long del(unsigned long n)
{
  unsigned long x = n;
  char buf1[10];

  while (x--) {
    for (int i = 0; i < sizeof(buf1); i++) {
      buf1[i] = x;
    }
  }

  
    for (int i = 0; i < sizeof(buf1); i++) {
      x += buf1[i];
    }
    return x;
}

/**
  Main entry for Fault Tolerant Write PEIM.

  @param[in]  FileHandle              Handle of the file being invoked.
  @param[in]  PeiServices             Pointer to PEI Services table.

  @retval EFI_SUCCESS  If the interface could be successfully installed
  @retval Others       Returned from PeiServicesInstallPpi()

**/
EFI_STATUS
EFIAPI
PeimFaultTolerantWriteInitialize (
  IN       EFI_PEI_FILE_HANDLE  FileHandle,
  IN CONST EFI_PEI_SERVICES     **PeiServices
  )
{
  EFI_STATUS                               Status;
  EFI_FAULT_TOLERANT_WORKING_BLOCK_HEADER  *FtwWorkingBlockHeader;
  EFI_FAULT_TOLERANT_WRITE_HEADER          *FtwLastWriteHeader;
  EFI_FAULT_TOLERANT_WRITE_RECORD          *FtwLastWriteRecord;
  EFI_PHYSICAL_ADDRESS                     WorkSpaceAddress;
  UINTN                                    WorkSpaceLength;
  EFI_PHYSICAL_ADDRESS                     SpareAreaAddress;
  UINTN                                    SpareAreaLength;
  EFI_PHYSICAL_ADDRESS                     WorkSpaceInSpareArea;
  UINT64                                   Size;
  FAULT_TOLERANT_WRITE_LAST_WRITE_DATA     FtwLastWrite;

  FtwWorkingBlockHeader = NULL;
  FtwLastWriteHeader    = NULL;
  FtwLastWriteRecord    = NULL;

  SpareAreaAddress = 0;
  SpareAreaLength  = 0;
  WorkSpaceAddress = 0;
  WorkSpaceLength  = 0;

  mde_4_edkii_vga_clear();

  mde_4_edkii_vga_sprintf(0, "FTWa-%x", 0);
  del(1000000);
  mde_4_edkii_vga_sprintf(0, "FTWa-%x", 1);
  del(1000000);
  mde_4_edkii_vga_sprintf(0, "FTWa-%x", 2);
  del(10000000);
  mde_4_edkii_vga_sprintf(0, "FTWa-%x", 3);
  del(10000000);



  //
  // Direct HOB probe - bypasses VariableFlashInfoLib entirely so we can
  // tell, independent of what the library/PCD-fallback reports, whether
  // gVariableFlashInfoHobGuid actually exists in the HOB list at the
  // moment FtwPei dispatches. If this prints "NOTFOUND", SmmStorePei's
  // HOB genuinely is not visible here (real dispatch-order or HOB-list
  // problem). If it prints a non-null pointer, the HOB exists and the
  // bug is specifically inside VariableFlashInfoLib's consumption of it.
  //
  {
    EFI_HOB_GUID_TYPE  *DirectHob;
    extern EFI_GUID     gVariableFlashInfoHobGuid;

    DirectHob = GetFirstGuidHob (&gVariableFlashInfoHobGuid);
    if (DirectHob == NULL) {
      mde_4_edkii_vga_sprintf(1, "HOBraw-NOTFOUND");
    } else {
      VARIABLE_FLASH_INFO  *DirectInfo = (VARIABLE_FLASH_INFO *)GET_GUID_HOB_DATA (DirectHob);
      mde_4_edkii_vga_sprintf(1, "HOBraw-FOUND-%x", (UINTN)DirectHob);
      mde_4_edkii_vga_sprintf(2, "HOBval-Var%x,%x",
        (UINTN)DirectInfo->NvVariableBaseAddress,
        (UINTN)DirectInfo->NvVariableLength);
      mde_4_edkii_vga_sprintf(2, "HOBval-Wrk%x-Spr%x",
        (UINTN)DirectInfo->FtwWorkingBaseAddress,
        (UINTN)DirectInfo->FtwSpareBaseAddress);
    }
  }

  del(10000000);
  // MicroSecondDelay(3000000);

  Status = GetVariableFlashFtwWorkingInfo (&WorkSpaceAddress, &Size);
  mde_4_edkii_vga_sprintf(3, "FTWb-%x-%x-%x",
    Status,//0
    WorkSpaceAddress,//0
    Size);//0

  // ASSERT_EFI_ERROR (Status);

  Status = SafeUint64ToUintn (Size, &WorkSpaceLength);
  // This driver currently assumes the size will be UINTN so assert the value is safe for now.
  mde_4_edkii_vga_sprintf(4, "4a-%x-%x",
    Status,//0
    WorkSpaceLength);//0
  // ASSERT_EFI_ERROR (Status);

  Status = GetVariableFlashFtwSpareInfo (&SpareAreaAddress, &Size);
  mde_4_edkii_vga_sprintf(5, "5a-%x-%x-%x",
    Status,//0
    SpareAreaAddress,//0
    Size);//0
  // ASSERT_EFI_ERROR (Status);

  Status = SafeUint64ToUintn (Size, &SpareAreaLength);
  mde_4_edkii_vga_sprintf(6, "6a",
    Status,//0
    SpareAreaLength,//0
    Size);//0
  // This driver currently assumes the size will be UINTN so assert the value is safe for now.
  // ASSERT_EFI_ERROR (Status);

  //
  // The address of FTW working base and spare base must not be 0.
  //
  ASSERT ((WorkSpaceAddress != 0) && (SpareAreaAddress != 0));

  
  mde_4_edkii_vga_sprintf(7, "7a-%x-%x-%x,%x,%x",
    Status,//0
    SpareAreaLength,//0
    Size,
    WorkSpaceAddress,
    SpareAreaAddress
  );//0



  FtwWorkingBlockHeader = (EFI_FAULT_TOLERANT_WORKING_BLOCK_HEADER *)(UINTN)WorkSpaceAddress;
  if (IsValidWorkSpace (FtwWorkingBlockHeader, WorkSpaceLength)) {
    
    mde_4_edkii_vga_sprintf(8, "8a-%x-%x",
      FtwWorkingBlockHeader,
      WorkSpaceLength);
    Status = FtwGetLastWriteHeader (
               FtwWorkingBlockHeader,
               WorkSpaceLength,
               &FtwLastWriteHeader
               );
               
    mde_4_edkii_vga_sprintf(8, "8b-%x-%x-%x-%x",
      Status,
      FtwWorkingBlockHeader,
      WorkSpaceLength,
      FtwLastWriteHeader);

    if (!EFI_ERROR (Status)) {
      
      mde_4_edkii_vga_sprintf(8, "8c-%x-%x-%x-%x",
        Status,
        FtwWorkingBlockHeader,
        WorkSpaceLength,
        FtwLastWriteHeader);

      Status = FtwGetLastWriteRecord (
                 FtwLastWriteHeader,
                 &FtwLastWriteRecord
                 );
                 
      mde_4_edkii_vga_sprintf(8, "8d-%x-%x-%x-%x-%x",
        Status,
        FtwWorkingBlockHeader,
        WorkSpaceLength,
        FtwLastWriteHeader,
        FtwLastWriteRecord);

    }


    mde_4_edkii_vga_sprintf(9, "9a-%x-%x",
      Status,
      FtwLastWriteRecord);

    if (!EFI_ERROR (Status)) {
      ASSERT (FtwLastWriteRecord != NULL);


      mde_4_edkii_vga_sprintf(9, "9b-%x-%x-%x-%x",
        Status,
        FtwLastWriteRecord,
        FtwLastWriteRecord->SpareComplete,
        FtwLastWriteRecord->DestinationComplete);

      if ((FtwLastWriteRecord->SpareComplete == FTW_VALID_STATE) && (FtwLastWriteRecord->DestinationComplete != FTW_VALID_STATE)) {
        //
        // If FTW last write was still in progress with SpareComplete set and DestinationComplete not set.
        // It means the target buffer has been backed up in spare block, then target block has been erased,
        // but the target buffer has not been writen in target block from spare block, we need to build
        // FAULT_TOLERANT_WRITE_LAST_WRITE_DATA GUID hob to hold the FTW last write data.
        //
        FtwLastWrite.TargetAddress = (EFI_PHYSICAL_ADDRESS)(UINTN)((INT64)SpareAreaAddress + FtwLastWriteRecord->RelativeOffset);
        FtwLastWrite.SpareAddress  = SpareAreaAddress;
        FtwLastWrite.Length        = SpareAreaLength;
        DEBUG ((
          DEBUG_INFO,
          "FtwPei last write data: TargetAddress - 0x%x SpareAddress - 0x%x Length - 0x%x\n",
          (UINTN)FtwLastWrite.TargetAddress,
          (UINTN)FtwLastWrite.SpareAddress,
          (UINTN)FtwLastWrite.Length
          ));
          

        mde_4_edkii_vga_sprintf(9, "9c-%x-%x-%x-%x,%x-%x-%x",
          Status,
          FtwLastWriteRecord,
          FtwLastWriteRecord->SpareComplete,
          FtwLastWriteRecord->DestinationComplete,
          FtwLastWrite.TargetAddress,
          FtwLastWrite.SpareAddress,
          FtwLastWrite.Length);
          
        BuildGuidDataHob (&gEdkiiFaultTolerantWriteGuid, (VOID *)&FtwLastWrite, sizeof (FAULT_TOLERANT_WRITE_LAST_WRITE_DATA));
        

        mde_4_edkii_vga_sprintf(10, "a-%x-%x-%x-%x,%x-%x-%x",
          gEdkiiFaultTolerantWriteGuid.Data1,
          gEdkiiFaultTolerantWriteGuid.Data2,
          gEdkiiFaultTolerantWriteGuid.Data3,
          gEdkiiFaultTolerantWriteGuid.Data4,
          FtwLastWrite.TargetAddress,
          FtwLastWrite.SpareAddress,
          FtwLastWrite.Length
          );
      }
    }
  } else {
    FtwWorkingBlockHeader = NULL;
    //
    // If the working block workspace is not valid, try to find workspace in the spare block.
    //
    WorkSpaceInSpareArea = SpareAreaAddress + SpareAreaLength - WorkSpaceLength;
    while (WorkSpaceInSpareArea >= SpareAreaAddress) {
      if (CompareGuid (&gEdkiiWorkingBlockSignatureGuid, (EFI_GUID *)(UINTN)WorkSpaceInSpareArea)) {
        //
        // Found the workspace.
        //
        DEBUG ((DEBUG_INFO, "FtwPei: workspace in spare block is at 0x%x.\n", (UINTN)WorkSpaceInSpareArea));
        FtwWorkingBlockHeader = (EFI_FAULT_TOLERANT_WORKING_BLOCK_HEADER *)(UINTN)WorkSpaceInSpareArea;
        break;
      }

      WorkSpaceInSpareArea = WorkSpaceInSpareArea - sizeof (EFI_GUID);
    }

    if ((FtwWorkingBlockHeader != NULL) && IsValidWorkSpace (FtwWorkingBlockHeader, WorkSpaceLength)) {
      //
      // It was workspace self reclaim, build FAULT_TOLERANT_WRITE_LAST_WRITE_DATA GUID hob for it.
      //
      FtwLastWrite.TargetAddress = WorkSpaceAddress - (WorkSpaceInSpareArea - SpareAreaAddress);
      FtwLastWrite.SpareAddress  = SpareAreaAddress;
      FtwLastWrite.Length        = SpareAreaLength;
      DEBUG ((
        DEBUG_INFO,
        "FtwPei last write data: TargetAddress - 0x%x SpareAddress - 0x%x Length - 0x%x\n",
        (UINTN)FtwLastWrite.TargetAddress,
        (UINTN)FtwLastWrite.SpareAddress,
        (UINTN)FtwLastWrite.Length
        ));
      BuildGuidDataHob (&gEdkiiFaultTolerantWriteGuid, (VOID *)&FtwLastWrite, sizeof (FAULT_TOLERANT_WRITE_LAST_WRITE_DATA));
    } else {
      //
      // Both are invalid.
      //
      DEBUG ((DEBUG_ERROR, "FtwPei: Both working and spare block are invalid.\n"));
    }
  }


  mde_4_edkii_vga_sprintf(11, "11-%x-%x-%x-%x,%x-%x-%x",
    mPpiListVariable.Flags,
    mPpiListVariable.Guid->Data1,
    mPpiListVariable.Guid->Data2,
    mPpiListVariable.Guid->Data3,
    mPpiListVariable.Guid->Data4,
    mPpiListVariable.Ppi,
    *(unsigned int*)(mPpiListVariable.Ppi)
  );


  // MicroSecondDelay(3000000);
  //
  // Install gEdkiiFaultTolerantWriteGuid PPI to inform the check for FTW last write data has been done.
  //
  int res2 = PeiServicesInstallPpi (&mPpiListVariable);
  
  mde_4_edkii_vga_sprintf(11, "11-%x,%x-%x-%x-%x,%x-%x-%x",
    res2,
    mPpiListVariable.Flags,
    mPpiListVariable.Guid->Data1,
    mPpiListVariable.Guid->Data2,
    mPpiListVariable.Guid->Data3,
    mPpiListVariable.Guid->Data4,
    mPpiListVariable.Ppi,
    *(unsigned int*)(mPpiListVariable.Ppi)
  );

  // int i = 10;
  while (1) {
    // i = 20;
  }
  
  return res2;
}