/** @file
The Dasharo Firmware Update Mode setup menu implementation.

Registers a "Firmware Update Mode" entry on the EDK2 setup front page.
Selecting it enables FUM for the next boot and reboots.

Copyright (c) 2026, 3mdeb Sp. z o.o. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause

**/

#include <Uefi.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/HiiLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiHiiServicesLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Protocol/DevicePath.h>
#include <Protocol/HiiConfigAccess.h>
#include <DasharoOptions.h>

#include "DasharoFumDxeHii.h"

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
  or more named elements from the target driver. The only varstore in this
  formset is an efivarstore, which the HII database manages directly, so
  there is nothing for this driver to extract.

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
  This function processes the results of changes in configuration. The only
  varstore in this formset is an efivarstore, which the HII database manages
  directly, so there is nothing for this driver to route.

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
  Firmware Update Mode formset. The form itself presents the FUM warnings
  (as non-interactive text), so selecting the interactive option means the
  user has accepted them: enable FUM for the next boot and reboot the
  system.

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
  EFI_STATUS  Status;

  Status = EFI_SUCCESS;

  switch (Action) {
  case EFI_BROWSER_ACTION_CHANGED:
    if (QuestionId == FIRMWARE_UPDATE_MODE_QUESTION_ID) {
      Status = EnableFirmwareUpdateMode ();
      if (EFI_ERROR (Status)) {
        return Status;
      }

      gRT->ResetSystem (EfiResetCold, EFI_SUCCESS, 0, NULL);
    } else {
      Status = EFI_UNSUPPORTED;
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

  //
  // VFR cannot read PCDs directly, so publish the build-time FUM options
  // as a boot-service EFI variable. The form binds to it with an
  // efivarstore (see DasharoFumDxeVfr.Vfr) and the HII database reads it
  // when evaluating form conditions.
  //
  DASHARO_FUM_INFO  FumInfo;

  FumInfo.IpxeAutoBoot = (UINT8) (FixedPcdGetBool (PcdFumAutoIpxeBoot) ? 1 : 0);

  Status = gRT->SetVariable (
                  DASHARO_FUM_INFO_VARIABLE_NAME,
                  &mDasharoFumFormsetGuid,
                  EFI_VARIABLE_BOOTSERVICE_ACCESS,
                  sizeof (FumInfo),
                  &FumInfo
                  );
  if (EFI_ERROR (Status)) {
    DEBUG (
      (
        DEBUG_ERROR,
        "DasharoFumDxe: failed to publish FUM options variable: %r\n",
        Status
        )
      );
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
