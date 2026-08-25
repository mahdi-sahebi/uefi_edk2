/** @file
The Dasharo Firmware Update Mode setup menu implementation.

Registers a "Firmware Update Mode" entry on the EDK2 setup front page.
Selecting it enables FUM for the next boot and reboots.

Copyright (c) 2026, 3mdeb Sp. z o.o. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause

**/

#include "DasharoFumDxeHii.h"

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/HiiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiHiiServicesLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Protocol/DevicePath.h>
#include <Protocol/HiiConfigAccess.h>
#include <Protocol/SimpleTextIn.h>
#include <DasharoOptions.h>

#define DASHARO_FUM_PRIVATE_DATA_SIGNATURE  SIGNATURE_32 ('D', 'F', 'U', 'p')

///
/// HII specific Vendor Device Path definition.
///
typedef struct {
  VENDOR_DEVICE_PATH             VendorDevicePath;
  EFI_DEVICE_PATH_PROTOCOL       End;
} HII_VENDOR_DEVICE_PATH;

typedef struct {
  UINTN                           Signature;

  ///
  /// HII relative handle
  ///
  EFI_HII_HANDLE                  HiiHandle;

  EFI_HANDLE                      DriverHandle;

  ///
  /// Produced protocols
  ///
  EFI_HII_CONFIG_ACCESS_PROTOCOL  ConfigAccess;
} DASHARO_FUM_PRIVATE_DATA;

#define DASHARO_FUM_PRIVATE_DATA_FROM_THIS(a) \
  CR (a, \
      DASHARO_FUM_PRIVATE_DATA, \
      ConfigAccess, \
      DASHARO_FUM_PRIVATE_DATA_SIGNATURE \
      )

//
// These are the VFR compiler generated data representing our VFR data.
//
extern UINT8  DasharoFumDxeVfrBin[];
extern UINT8  DasharoFumDxeStrings[];

EFI_STATUS
EFIAPI
DasharoFumExtractConfig (
  IN  CONST EFI_HII_CONFIG_ACCESS_PROTOCOL   *This,
  IN  CONST EFI_STRING                       Request,
  OUT EFI_STRING                             *Progress,
  OUT EFI_STRING                             *Results
  );

EFI_STATUS
EFIAPI
DasharoFumRouteConfig (
  IN  CONST EFI_HII_CONFIG_ACCESS_PROTOCOL   *This,
  IN  CONST EFI_STRING                       Configuration,
  OUT EFI_STRING                             *Progress
  );

EFI_STATUS
EFIAPI
DasharoFumCallback (
  IN  CONST EFI_HII_CONFIG_ACCESS_PROTOCOL   *This,
  IN  EFI_BROWSER_ACTION                     Action,
  IN  EFI_QUESTION_ID                        QuestionId,
  IN  UINT8                                  Type,
  IN  EFI_IFR_TYPE_VALUE                     *Value,
  OUT EFI_BROWSER_ACTION_REQUEST             *ActionRequest
  );

STATIC DASHARO_FUM_PRIVATE_DATA  mDasharoFumPrivate = {
  DASHARO_FUM_PRIVATE_DATA_SIGNATURE,
  NULL,
  NULL,
  {
    DasharoFumExtractConfig,
    DasharoFumRouteConfig,
    DasharoFumCallback
  }
};

STATIC EFI_GUID  mDasharoFumFormsetGuid = DASHARO_FUM_FORMSET_GUID;

STATIC HII_VENDOR_DEVICE_PATH  mDasharoFumHiiVendorDevicePath = {
  {
    {
      HARDWARE_DEVICE_PATH,
      HW_VENDOR_DP,
      {
        (UINT8) (sizeof (VENDOR_DEVICE_PATH)),
        (UINT8) ((sizeof (VENDOR_DEVICE_PATH)) >> 8)
      }
    },
    DASHARO_FUM_FORMSET_GUID
  },
  {
    END_DEVICE_PATH_TYPE,
    END_ENTIRE_DEVICE_PATH_SUBTYPE,
    {
      (UINT8) (END_DEVICE_PATH_LENGTH),
      (UINT8) ((END_DEVICE_PATH_LENGTH) >> 8)
    }
  }
};

/**
  Enable firmware update mode (FUM) for the next boot.

  @retval EFI_SUCCESS   FUM was successfully enabled.
  @retval Other         Error enabling FUM.

**/
STATIC
EFI_STATUS
EnableFirmwareUpdateMode (
  VOID
  )
{
  BOOLEAN  Enable;

  Enable = TRUE;
  return gRT->SetVariable (
                DASHARO_VAR_FIRMWARE_UPDATE_MODE_REQUEST,
                &gDasharoSystemFeaturesGuid,
                EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                sizeof (Enable),
                &Enable
                );
}

/**
  This function allows a caller to extract the current configuration for one
  or more named elements from the target driver. This formset has no varstore,
  so there is nothing to extract.

**/
EFI_STATUS
EFIAPI
DasharoFumExtractConfig (
  IN  CONST EFI_HII_CONFIG_ACCESS_PROTOCOL   *This,
  IN  CONST EFI_STRING                       Request,
  OUT EFI_STRING                             *Progress,
  OUT EFI_STRING                             *Results
  )
{
  if ((Request == NULL) || (Progress == NULL) || (Results == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  *Progress = Request;
  return EFI_NOT_FOUND;
}

/**
  This function processes the results of changes in configuration. This
  formset has no varstore, so there is nothing to route.

**/
EFI_STATUS
EFIAPI
DasharoFumRouteConfig (
  IN  CONST EFI_HII_CONFIG_ACCESS_PROTOCOL   *This,
  IN  CONST EFI_STRING                       Configuration,
  OUT EFI_STRING                             *Progress
  )
{
  if ((Configuration == NULL) || (Progress == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  *Progress = Configuration;
  return EFI_NOT_FOUND;
}

/**
  This function is invoked if user selected an interactive opcode from the
  Firmware Update Mode formset. It asks for confirmation, enables FUM for the
  next boot and reboots the system.

**/
EFI_STATUS
EFIAPI
DasharoFumCallback (
  IN  CONST EFI_HII_CONFIG_ACCESS_PROTOCOL   *This,
  IN  EFI_BROWSER_ACTION                     Action,
  IN  EFI_QUESTION_ID                        QuestionId,
  IN  UINT8                                  Type,
  IN  EFI_IFR_TYPE_VALUE                     *Value,
  OUT EFI_BROWSER_ACTION_REQUEST             *ActionRequest
  )
{
  EFI_STATUS                         Status;
  EFI_INPUT_KEY                      Key;
  CONST CHAR16                       *PressEnterMsg;
  CONST CHAR16                       *VariableLines[3];
  DASHARO_FUM_PRIVATE_DATA           *Private;

  Status  = EFI_SUCCESS;
  Private = DASHARO_FUM_PRIVATE_DATA_FROM_THIS (This);

  switch (Action) {
  case EFI_BROWSER_ACTION_CHANGED:
    {
      if (QuestionId == FIRMWARE_UPDATE_MODE_QUESTION_ID) {
        PressEnterMsg = L"Press ENTER to continue and reboot or ESC to cancel...";
        if (FixedPcdGetBool (PcdFumAutoIpxeBoot)) {
          VariableLines[0] = L"DTS will be started automatically through iPXE, please";
          VariableLines[1] = L"make sure an Ethernet cable is connected before continuing.";
          VariableLines[2] = L"";
        } else {
          VariableLines[0] = PressEnterMsg;
          VariableLines[1] = L"";
          /* This terminates list of lines. */
          VariableLines[2] = NULL;
        }

        do {
          CreatePopUp (
            EFI_BLACK | EFI_BACKGROUND_RED,
            &Key,
            L"",
            L"You are about to enable Firmware Update Mode.",
            L"This will turn off all flash protection mechanisms",
            L"for the duration of the next boot.",
            L"",
            VariableLines[0],
            VariableLines[1],
            VariableLines[2],
            PressEnterMsg,
            L"",
            NULL
            );
        } while ((Key.ScanCode != SCAN_ESC) && (Key.UnicodeChar != CHAR_CARRIAGE_RETURN));

        if (Key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
          Status = EnableFirmwareUpdateMode ();
          if (EFI_ERROR (Status)) {
            return Status;
          }
          gRT->ResetSystem (EfiResetCold, EFI_SUCCESS, 0, NULL);
        }
      } else {
        Status = EFI_UNSUPPORTED;
      }
    }
    break;
  default:
    Status = EFI_UNSUPPORTED;
    break;
  }

  return Status;
}

/**
  Install Dasharo Firmware Update Mode menu driver.

  @param ImageHandle          The image handle.
  @param SystemTable          The system table.

  @retval  EFI_SUCCESS        Installed Dasharo Firmware Update Mode menu.
  @retval  Other              Error.

**/
EFI_STATUS
EFIAPI
DasharoFumDxeEntryPoint (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  EFI_STATUS  Status;

  //
  // FUM is part of the Dasharo system features UI; register it only when the
  // UI itself and the FUM option are enabled.
  //
  if (!FixedPcdGetBool (PcdShowMenu) || !FixedPcdGetBool (PcdShowFum)) {
    return EFI_SUCCESS;
  }

  mDasharoFumPrivate.DriverHandle = NULL;
  Status = gBS->InstallMultipleProtocolInterfaces (
      &mDasharoFumPrivate.DriverHandle,
      &gEfiDevicePathProtocolGuid,
      &mDasharoFumHiiVendorDevicePath,
      &gEfiHiiConfigAccessProtocolGuid,
      &mDasharoFumPrivate.ConfigAccess,
      NULL
      );
  ASSERT_EFI_ERROR (Status);

  //
  // Publish our HII data.
  //
  mDasharoFumPrivate.HiiHandle = HiiAddPackages (
      &mDasharoFumFormsetGuid,
      mDasharoFumPrivate.DriverHandle,
      DasharoFumDxeVfrBin,
      DasharoFumDxeStrings,
      NULL
      );
  ASSERT (mDasharoFumPrivate.HiiHandle != NULL);

  return EFI_SUCCESS;
}

/**
  Uninstall Dasharo Firmware Update Mode menu driver.

  @param ImageHandle          The image handle.

  @retval  EFI_SUCCESS        Uninstalled Dasharo Firmware Update Mode menu.

**/
EFI_STATUS
EFIAPI
DasharoFumDxeUnload (
  IN EFI_HANDLE         ImageHandle
  )
{
  if (mDasharoFumPrivate.HiiHandle != NULL) {
    HiiRemovePackages (mDasharoFumPrivate.HiiHandle);
    mDasharoFumPrivate.HiiHandle = NULL;
  }

  return EFI_SUCCESS;
}
