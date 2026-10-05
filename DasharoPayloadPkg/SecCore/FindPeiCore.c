/** @file
  Locate the entry point for the PEI Core

Copyright (c) 2013, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiPei.h>
#include <Library/BaseLib.h>
#include <Library/PeCoffGetEntryPointLib.h>

#include "SecMain.h"
#include "edkii_vga.h"

/**
  Find core image base.

  @param   BootFirmwareVolumePtr    Point to the boot firmware volume.
  @param   SecCoreImageBase         The base address of the SEC core image.
  @param   PeiCoreImageBase         The base address of the PEI core image.

**/
EFI_STATUS
EFIAPI
FindImageBase (
  IN  EFI_FIRMWARE_VOLUME_HEADER       *BootFirmwareVolumePtr,
  OUT EFI_PHYSICAL_ADDRESS             *SecCoreImageBase,
  OUT EFI_PHYSICAL_ADDRESS             *PeiCoreImageBase
  )
{
  // char buf[78];

  EFI_PHYSICAL_ADDRESS        CurrentAddress;
  EFI_PHYSICAL_ADDRESS        EndOfFirmwareVolume;
  EFI_FFS_FILE_HEADER         *File;
  UINT32                      Size;
  EFI_PHYSICAL_ADDRESS        EndOfFile;
  EFI_COMMON_SECTION_HEADER   *Section;
  EFI_PHYSICAL_ADDRESS        EndOfSection;

  *SecCoreImageBase = 0;
  *PeiCoreImageBase = 0;

  CurrentAddress = (EFI_PHYSICAL_ADDRESS)(UINTN) BootFirmwareVolumePtr;
  EndOfFirmwareVolume = CurrentAddress + BootFirmwareVolumePtr->FvLength;

  edkii_vga_sprintf(1, "FI-%x,%x,%x,%x,%x",
    BootFirmwareVolumePtr,
    BootFirmwareVolumePtr->FvLength,
    EndOfFirmwareVolume,
    BootFirmwareVolumePtr->HeaderLength,
    CurrentAddress + BootFirmwareVolumePtr->HeaderLength);

  // edkii_vga_hex_dump((unsigned char*)BootFirmwareVolumePtr, 32, 6);


  int ip = 0;
  //
  // Loop through the FFS files in the Boot Firmware Volume
  //
  for (EndOfFile = CurrentAddress + BootFirmwareVolumePtr->HeaderLength; ; ) {
    ip++;
    CurrentAddress = (EndOfFile + 7) & 0xfffffffffffffff8ULL;
    if (CurrentAddress > EndOfFirmwareVolume) {
      edkii_vga_sprintf(2, "EFI_NOT_FOUND-0: %X,%x,%X,%X", 
        ip,
        CurrentAddress,
        EndOfFirmwareVolume
      );
      return EFI_NOT_FOUND;
    }

    File = (EFI_FFS_FILE_HEADER*)(UINTN) CurrentAddress;
    if (IS_FFS_FILE2 (File)) {
      Size = FFS_FILE2_SIZE (File);
      if (Size <= 0x00FFFFFF) {
        edkii_vga_sprintf(2, "EFI_NOT_FOUND-1: %X,%X, %X", 
          ip,
          Size,
          CurrentAddress
        );
        // edkii_vga_print(4, buf);
        return EFI_NOT_FOUND;
      }
    } else {
      Size = FFS_FILE_SIZE (File);
      if (Size < sizeof (EFI_FFS_FILE_HEADER)) {
        edkii_vga_sprintf(2, "EFI_NOT_FOUND-2: %X,%X, %X", 
          ip,
          Size,
          sizeof (EFI_FFS_FILE_HEADER)
        );
        // edkii_vga_print(4, buf);
        return EFI_NOT_FOUND;
      }
    }

    EndOfFile = CurrentAddress + Size;
    if (EndOfFile > EndOfFirmwareVolume) {
        edkii_vga_sprintf(2, "EFI_NOT_FOUND-3: %X,%X, %X", 
          ip,
          EndOfFile,
          EndOfFirmwareVolume
        );
        // edkii_vga_print(4, buf);
      return EFI_NOT_FOUND;
    }

    //
    // Look for SEC Core / PEI Core files
    //
    if (File->Type != EFI_FV_FILETYPE_SECURITY_CORE &&
        File->Type != EFI_FV_FILETYPE_PEI_CORE) {
      continue;
    }

    //
    // Loop through the FFS file sections within the FFS file
    //
    if (IS_FFS_FILE2 (File)) {
      EndOfSection = (EFI_PHYSICAL_ADDRESS) (UINTN) ((UINT8 *) File + sizeof (EFI_FFS_FILE_HEADER2));
    } else {
      EndOfSection = (EFI_PHYSICAL_ADDRESS) (UINTN) ((UINT8 *) File + sizeof (EFI_FFS_FILE_HEADER));
    }
    for (;;) {
      CurrentAddress = (EndOfSection + 3) & 0xfffffffffffffffcULL;
      Section = (EFI_COMMON_SECTION_HEADER*)(UINTN) CurrentAddress;

      if (IS_SECTION2 (Section)) {
        Size = SECTION2_SIZE (Section);
        if (Size <= 0x00FFFFFF) {
          
          edkii_vga_sprintf(2, "EFI_NOT_FOUND-4: %X,%X,%X,%X", 
            ip,
            Size,
            CurrentAddress,
            Section
          );
          // edkii_vga_print(4, buf);
          return EFI_NOT_FOUND;
        }
      } else {
        Size = SECTION_SIZE (Section);
        if (Size < sizeof (EFI_COMMON_SECTION_HEADER)) {

          edkii_vga_sprintf(2, "EFI_NOT_FOUND-5: %X,%X,%X,%X,%X", 
            ip,
            Size,
            sizeof (EFI_COMMON_SECTION_HEADER),
            CurrentAddress,
            Section
          );
          // edkii_vga_print(4, buf);
          return EFI_NOT_FOUND;
        }
      }

      EndOfSection = CurrentAddress + Size;
      if (EndOfSection > EndOfFile) {
          edkii_vga_sprintf(2, "EFI_NOT_FOUND-5: %X,%X, %X, %X", 
            ip,
            Size,
            EndOfSection,
            EndOfFile
          );
          // edkii_vga_print(4, buf);

        return EFI_NOT_FOUND;
      }

      //
      // Look for executable sections
      //
      if (Section->Type == EFI_SECTION_PE32 || Section->Type == EFI_SECTION_TE) {
        if (File->Type == EFI_FV_FILETYPE_SECURITY_CORE) {
          if (IS_SECTION2 (Section)) {
            *SecCoreImageBase = (PHYSICAL_ADDRESS) (UINTN) ((UINT8 *) Section + sizeof (EFI_COMMON_SECTION_HEADER2));
          } else {
            *SecCoreImageBase = (PHYSICAL_ADDRESS) (UINTN) ((UINT8 *) Section + sizeof (EFI_COMMON_SECTION_HEADER));
          }
        } else {
          if (IS_SECTION2 (Section)) {
            *PeiCoreImageBase = (PHYSICAL_ADDRESS) (UINTN) ((UINT8 *) Section + sizeof (EFI_COMMON_SECTION_HEADER2));
          } else {
            *PeiCoreImageBase = (PHYSICAL_ADDRESS) (UINTN) ((UINT8 *) Section + sizeof (EFI_COMMON_SECTION_HEADER));
          }
        }
        break;
      }
    }

    edkii_vga_sprintf(3, "SP-%X,%x,%x-%x,%x",
      ip,
      SecCoreImageBase,
      *SecCoreImageBase,
      PeiCoreImageBase,
      *PeiCoreImageBase
    );
    //
    // Both SEC Core and PEI Core images found
    //
    if (*SecCoreImageBase != 0 && *PeiCoreImageBase != 0) {
          // AsciiSPrint(buf, sizeof(buf), "7: 0x%X, 0x%X", 
          //   (UINT32)(UINTN)*SecCoreImageBase,
          //   (UINT32)(UINTN)*PeiCoreImageBase);
          // edkii_vga_print(5, buf);

      return EFI_SUCCESS;
    }
  }
  
  edkii_vga_sprintf(4, "EndFindImageBase-%X,%x,%x-%x,%x",
    ip,
    SecCoreImageBase,
    *SecCoreImageBase,
    PeiCoreImageBase,
    *PeiCoreImageBase
  );
}

/**
  Find and return Pei Core entry point.

  It also find SEC and PEI Core file debug information. It will report them if
  remote debug is enabled.

  @param   BootFirmwareVolumePtr    Point to the boot firmware volume.
  @param   PeiCoreEntryPoint        The entry point of the PEI core.

**/
VOID
EFIAPI
FindAndReportEntryPoints (
  IN  EFI_FIRMWARE_VOLUME_HEADER       *BootFirmwareVolumePtr,
  OUT EFI_PEI_CORE_ENTRY_POINT         *PeiCoreEntryPoint
  )
{
  EFI_STATUS                       Status;
  EFI_PHYSICAL_ADDRESS             SecCoreImageBase;
  EFI_PHYSICAL_ADDRESS             PeiCoreImageBase;
  PE_COFF_LOADER_IMAGE_CONTEXT     ImageContext;

  // char buf[70];
  // AsciiSPrint(buf, sizeof(buf), "FindEntry-BFV 0x%08X, PCE 0x%08X", 
  //   (UINT32)(UINTN)BootFirmwareVolumePtr,
  //   (UINT32)(UINTN)PeiCoreEntryPoint);
  edkii_vga_sprintf(0, "FPa-%x-%x,%x-%x",
    BootFirmwareVolumePtr,
    *BootFirmwareVolumePtr,
    PeiCoreEntryPoint,
    *PeiCoreEntryPoint
  );


  //
  // Find SEC Core and PEI Core image base
  //
  Status = FindImageBase (BootFirmwareVolumePtr, &SecCoreImageBase, &PeiCoreImageBase);
  edkii_vga_sprintf(5, "FEa-%x", 
    Status//,
    // (UINT32)(UINTN)SecCoreImageBase,
    // (UINT32)(UINTN)PeiCoreImageBase);
  // edkii_vga_print(5, buf);
  );
  // ASSERT_EFI_ERROR (Status);
  ASSERT_EFI_ERROR (Status);

  ZeroMem ((VOID *) &ImageContext, sizeof (PE_COFF_LOADER_IMAGE_CONTEXT));
  //
  // Report SEC Core debug information when remote debug is enabled
  //
  // char* iptr_sec = 0;
  // if (0 != SecCoreImageBase) {
    ImageContext.ImageAddress = SecCoreImageBase;
    ImageContext.PdbPointer = PeCoffLoaderGetPdbPointer ((VOID*) (UINTN) ImageContext.ImageAddress);
    
    edkii_vga_sprintf(5, "FEb-%x,%x",
      ImageContext.ImageAddress,
      ImageContext.PdbPointer
    );
    PeCoffLoaderRelocateImageExtraAction (&ImageContext);
  // }

  //
  // Report PEI Core debug information when remote debug is enabled
  //
  // char* iptr_pei = 0;
  // if (0 != PeiCoreImageBase) {
    ImageContext.ImageAddress = PeiCoreImageBase;
    ImageContext.PdbPointer = PeCoffLoaderGetPdbPointer ((VOID*) (UINTN) ImageContext.ImageAddress);
    
    edkii_vga_sprintf(6, "FEc-%x,%x",
      ImageContext.ImageAddress,
      ImageContext.PdbPointer
    );
    PeCoffLoaderRelocateImageExtraAction (&ImageContext);
  // }

  //
  // Find PEI Core entry point
  //
  edkii_vga_print(8, "FEd-!");
  Status = PeCoffLoaderGetEntryPoint ((VOID *) (UINTN) PeiCoreImageBase, (VOID**) PeiCoreEntryPoint);
  edkii_vga_sprintf(8, "FEd-%x", 
    // (UINT32)(UINTN)iptr_sec,
    // (UINT32)(UINTN)iptr_pei,
    // PeiCoreEntryPoint,
    // *PeiCoreEntryPoint,
    Status);
  if (EFI_ERROR (Status)) {
    // edkii_vga_print(8, "FE - 8");
    *PeiCoreEntryPoint = 0;
  }
  edkii_vga_sprintf(8, "FEe-%x", 
    // (UINT32)(UINTN)iptr_sec,
    // (UINT32)(UINTN)iptr_pei,
    // PeiCoreEntryPoint,
    // *PeiCoreEntryPoint,
    Status);
    
  return;
}

