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
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/HobLib.h>
#include <Library/SafeIntLib.h>
#include <Library/VariableFlashInfoLib.h>
#include <Guid/VariableFlashInfo.h>

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
  EFI_HOB_GUID_TYPE                       *GuidHob;
  VARIABLE_FLASH_INFO                      *FlashInfo;
  EFI_FAULT_TOLERANT_WORKING_BLOCK_HEADER  *FtwWorkingBlockHeader;
  EFI_FAULT_TOLERANT_WRITE_HEADER          *FtwLastWriteHeader;
  EFI_FAULT_TOLERANT_WRITE_RECORD          *FtwLastWriteRecord;
  EFI_PHYSICAL_ADDRESS                     WorkSpaceAddress;
  EFI_PHYSICAL_ADDRESS                     SpareAreaAddress;
  EFI_PHYSICAL_ADDRESS                     WorkSpaceInSpareArea;
  UINTN                                    WorkSpaceLength;
  UINTN                                    SpareAreaLength;
  UINT64                                   Size;
  FAULT_TOLERANT_WRITE_LAST_WRITE_DATA     FtwLastWrite;

  DEBUG ((DEBUG_ERROR, "[EDK2-DIAG] FTW entry\n"));

  WorkSpaceAddress = 0;
  SpareAreaAddress = 0;
  Size             = 0;
  GuidHob = GetFirstGuidHob (&gVariableFlashInfoHobGuid);
  if ((GuidHob == NULL) || (GET_GUID_HOB_DATA_SIZE (GuidHob) < sizeof (VARIABLE_FLASH_INFO))) {
    DEBUG ((DEBUG_ERROR, "[EDK2-DIAG] FTW variable flash HOB missing or short hob=%p\n", GuidHob));
    return EFI_NOT_FOUND;
  }

  FlashInfo = (VARIABLE_FLASH_INFO *)GET_GUID_HOB_DATA (GuidHob);
  DEBUG ((DEBUG_ERROR, "[EDK2-DIAG] FTW HOB=%p version=%u nv=0x%lx+0x%lx working=0x%lx+0x%lx spare=0x%lx+0x%lx\n",
    GuidHob,
    FlashInfo->Version,
    FlashInfo->NvVariableBaseAddress,
    FlashInfo->NvVariableLength,
    FlashInfo->FtwWorkingBaseAddress,
    FlashInfo->FtwWorkingLength,
    FlashInfo->FtwSpareBaseAddress,
    FlashInfo->FtwSpareLength));
  if (FlashInfo->Version != VARIABLE_FLASH_INFO_HOB_VERSION) {
    DEBUG ((DEBUG_ERROR, "[EDK2-DIAG] FTW unsupported variable flash HOB version=%u\n", FlashInfo->Version));
    return EFI_UNSUPPORTED;
  }

  Status = GetVariableFlashFtwWorkingInfo (&WorkSpaceAddress, &Size);
  DEBUG ((DEBUG_ERROR, "[EDK2-DIAG] FTW working info status=%r base=0x%lx size=0x%lx\n",
    Status, WorkSpaceAddress, Size));
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = SafeUint64ToUintn (Size, &WorkSpaceLength);
  if (EFI_ERROR (Status) || (WorkSpaceAddress == 0) ||
      (WorkSpaceLength < sizeof (EFI_FAULT_TOLERANT_WORKING_BLOCK_HEADER)) ||
      (WorkSpaceAddress > MAX_UINTN - WorkSpaceLength))
  {
    DEBUG ((DEBUG_ERROR, "[EDK2-DIAG] FTW invalid working range status=%r base=0x%lx size=0x%lx\n",
      Status, WorkSpaceAddress, Size));
    return EFI_INVALID_PARAMETER;
  }

  Status = GetVariableFlashFtwSpareInfo (&SpareAreaAddress, &Size);
  DEBUG ((DEBUG_ERROR, "[EDK2-DIAG] FTW spare info status=%r base=0x%lx size=0x%lx\n",
    Status, SpareAreaAddress, Size));
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = SafeUint64ToUintn (Size, &SpareAreaLength);
  if (EFI_ERROR (Status) || (SpareAreaAddress == 0) ||
      (SpareAreaLength < WorkSpaceLength) ||
      (SpareAreaAddress > MAX_UINTN - SpareAreaLength))
  {
    DEBUG ((DEBUG_ERROR, "[EDK2-DIAG] FTW invalid spare range status=%r base=0x%lx size=0x%lx\n",
      Status, SpareAreaAddress, Size));
    return EFI_INVALID_PARAMETER;
  }

  FtwWorkingBlockHeader = (EFI_FAULT_TOLERANT_WORKING_BLOCK_HEADER *)(UINTN)WorkSpaceAddress;
  DEBUG ((DEBUG_ERROR, "[EDK2-DIAG] FTW validate working workspace base=0x%lx size=0x%lx\n",
    WorkSpaceAddress, (UINT64)WorkSpaceLength));
  if (IsValidWorkSpace (FtwWorkingBlockHeader, WorkSpaceLength)) {
    FtwLastWriteHeader = NULL;
    FtwLastWriteRecord = NULL;
    Status = FtwGetLastWriteHeader (
               FtwWorkingBlockHeader,
               WorkSpaceLength,
               &FtwLastWriteHeader
               );
    DEBUG ((DEBUG_ERROR, "[EDK2-DIAG] FTW last header status=%r header=%p\n",
      Status, FtwLastWriteHeader));
    if (!EFI_ERROR (Status) && (FtwLastWriteHeader != NULL)) {
      Status = FtwGetLastWriteRecord (FtwLastWriteHeader, &FtwLastWriteRecord);
      DEBUG ((DEBUG_ERROR, "[EDK2-DIAG] FTW last record status=%r record=%p\n",
        Status, FtwLastWriteRecord));
      if (!EFI_ERROR (Status) && (FtwLastWriteRecord != NULL) &&
          (FtwLastWriteRecord->SpareComplete == FTW_VALID_STATE) &&
          (FtwLastWriteRecord->DestinationComplete != FTW_VALID_STATE))
      {
        FtwLastWrite.TargetAddress = (EFI_PHYSICAL_ADDRESS)(UINTN)(
                                       (INT64)SpareAreaAddress + FtwLastWriteRecord->RelativeOffset
                                       );
        FtwLastWrite.SpareAddress = SpareAreaAddress;
        FtwLastWrite.Length = SpareAreaLength;
        if (BuildGuidDataHob (&gEdkiiFaultTolerantWriteGuid, &FtwLastWrite, sizeof (FtwLastWrite)) == NULL) {
          DEBUG ((DEBUG_ERROR, "[EDK2-DIAG] FTW recovery HOB allocation failed\n"));
          return EFI_OUT_OF_RESOURCES;
        }

        DEBUG ((DEBUG_ERROR, "[EDK2-DIAG] FTW recovery target=0x%lx spare=0x%lx size=0x%lx\n",
          FtwLastWrite.TargetAddress, FtwLastWrite.SpareAddress, FtwLastWrite.Length));
      }
    }
  } else {
    DEBUG ((DEBUG_ERROR, "[EDK2-DIAG] FTW working workspace invalid; scan spare=0x%lx+0x%lx\n",
      SpareAreaAddress, (UINT64)SpareAreaLength));
    FtwWorkingBlockHeader = NULL;
    WorkSpaceInSpareArea = SpareAreaAddress + SpareAreaLength - WorkSpaceLength;
    while (TRUE) {
      if (CompareGuid (&gEdkiiWorkingBlockSignatureGuid, (EFI_GUID *)(UINTN)WorkSpaceInSpareArea)) {
        FtwWorkingBlockHeader = (EFI_FAULT_TOLERANT_WORKING_BLOCK_HEADER *)(UINTN)WorkSpaceInSpareArea;
        break;
      }

      if (WorkSpaceInSpareArea - SpareAreaAddress < sizeof (EFI_GUID)) {
        break;
      }

      WorkSpaceInSpareArea -= sizeof (EFI_GUID);
    }

    DEBUG ((DEBUG_ERROR, "[EDK2-DIAG] FTW spare scan result workspace=%p\n", FtwWorkingBlockHeader));
    if ((FtwWorkingBlockHeader != NULL) &&
        IsValidWorkSpace (FtwWorkingBlockHeader, WorkSpaceLength))
    {
      FtwLastWrite.TargetAddress = WorkSpaceAddress - (WorkSpaceInSpareArea - SpareAreaAddress);
      FtwLastWrite.SpareAddress = SpareAreaAddress;
      FtwLastWrite.Length = SpareAreaLength;
      if (BuildGuidDataHob (&gEdkiiFaultTolerantWriteGuid, &FtwLastWrite, sizeof (FtwLastWrite)) == NULL) {
        DEBUG ((DEBUG_ERROR, "[EDK2-DIAG] FTW recovery HOB allocation failed\n"));
        return EFI_OUT_OF_RESOURCES;
      }

      DEBUG ((DEBUG_ERROR, "[EDK2-DIAG] FTW spare recovery target=0x%lx spare=0x%lx size=0x%lx\n",
        FtwLastWrite.TargetAddress, FtwLastWrite.SpareAddress, FtwLastWrite.Length));
    } else {
      DEBUG ((DEBUG_ERROR, "[EDK2-DIAG] FTW both working and spare workspaces invalid\n"));
    }
  }

  DEBUG ((DEBUG_ERROR, "[EDK2-DIAG] FTW install completion PPI begin\n"));
  Status = PeiServicesInstallPpi (&mPpiListVariable);
  DEBUG ((DEBUG_ERROR, "[EDK2-DIAG] FTW install completion PPI status=%r\n", Status));
  return Status;
}
