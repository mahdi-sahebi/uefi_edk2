/** @file
  G4DEL USB inventory diagnostics for serial and the active text console.

  Copyright (c) 2026, G4DEL bring-up contributors. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/GxDxeDiagnosticsLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Protocol/DevicePath.h>
#include <Protocol/BlockIo.h>
#include <Protocol/GxDxeDiagnostics.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/SimpleTextIn.h>
#include <Protocol/Usb2HostController.h>
#include <Protocol/UsbIo.h>

#define GX_USB_TEXT_CHARS          81
#define GX_USB_PAGE_SECONDS        3
#define GX_USB_PAGE_CLEAR_DELAY_US 1000000

typedef struct {
  CHAR16  Text[GX_USB_TEXT_CHARS];
  EFI_DEVICE_PATH_PROTOCOL  *NormalizedDevicePath;
  UINTN   InterfaceCount;
  UINT16  VendorId;
  UINT16  ProductId;
  UINT8   InterfaceClass;
  BOOLEAN InterfaceClassSet;
  BOOLEAN MixedInterfaceClasses;
  BOOLEAN DescriptorReady;
  BOOLEAN UsbPath;
  BOOLEAN HasMassStorage;
  CHAR16  ClassText[8];
  CHAR16  Product[32];
} GX_USB_INVENTORY_ROW;

typedef struct {
  CONST CHAR8  *Name;
  CONST CHAR16 *DisplayName;
  EFI_GUID     FileGuid;
} GX_USB_DRIVER_INFO;

typedef struct {
  EFI_STATUS             QueryStatus;
  GX_DXE_DRIVER_STATUS   State;
} GX_USB_DRIVER_STATE;

STATIC CONST GX_USB_DRIVER_INFO  mGxUsbDrivers[] = {
  { "UhciDxe", L"UhciDxe", { 0x2FB92EFA, 0x2EE0, 0x4BAE, { 0x9E, 0xB6, 0x74, 0x64, 0x12, 0x5E, 0x1E, 0xF7 } } },
  { "EhciDxe", L"EhciDxe", { 0xBDFE430E, 0x8F2A, 0x4DB0, { 0x99, 0x91, 0x6F, 0x85, 0x65, 0x94, 0x77, 0x7E } } },
  { "XhciDxe", L"XhciDxe", { 0xB7F50E91, 0xA759, 0x412C, { 0xAD, 0xE4, 0xDC, 0xD0, 0x3E, 0x7F, 0x7C, 0x28 } } },
  { "UsbBusDxe", L"UsbBusDxe", { 0x240612B7, 0xA063, 0x11D4, { 0x9A, 0x3A, 0x00, 0x90, 0x27, 0x3F, 0xC1, 0x4D } } },
  { "UsbMassStorageDxe", L"UsbMassStorageDxe", { 0x9FB4B4A7, 0x42C0, 0x4BCD, { 0x85, 0x40, 0x9B, 0xCC, 0x67, 0x11, 0xF8, 0x3E } } },
  { "UsbKbDxe", L"UsbKbDxe", { 0x2D2E62CF, 0x9ECF, 0x43B7, { 0x82, 0x19, 0x94, 0xE7, 0xFC, 0x71, 0x3D, 0xFE } } },
  { "UsbMouseDxe", L"UsbMouseDxe", { 0x2D2E62AA, 0x9ECF, 0x43B7, { 0x82, 0x19, 0x94, 0xE7, 0xFC, 0x71, 0x3D, 0xFE } } },
  { "UsbMouseAbsolutePointerDxe", L"UsbMouseAbsolutePointerDxe", { 0x4EA43463, 0x747C, 0x46EB, { 0x97, 0xFB, 0xB0, 0xE5, 0xC5, 0xF0, 0x53, 0x06 } } }
};

typedef struct {
  UINTN  BlockIoCount;
  UINTN  UsbBlockIoCount;
  UINTN  UsbRemovableCount;
  UINTN  UsbMediaPresentCount;
  UINTN  SimpleFileSystemCount;
  UINTN  UsbSimpleFileSystemCount;
  UINTN  UsbVolumeOpenCount;
  UINTN  UsbVolumeOpenFailCount;
  EFI_STATUS  UsbLastOpenVolumeStatus;
} GX_USB_STORAGE_SUMMARY;

STATIC
BOOLEAN
GxDevicePathHasUsbNode (
  IN EFI_DEVICE_PATH_PROTOCOL  *DevicePath
  )
{
  EFI_DEVICE_PATH_PROTOCOL  *Node;

  if (DevicePath == NULL) {
    return FALSE;
  }

  for (Node = DevicePath; !IsDevicePathEnd (Node); Node = NextDevicePathNode (Node)) {
    if ((DevicePathType (Node) == MESSAGING_DEVICE_PATH) &&
        (DevicePathSubType (Node) == MSG_USB_DP))
    {
      return TRUE;
    }
  }

  return FALSE;
}

STATIC
EFI_DEVICE_PATH_PROTOCOL *
GxDuplicateNormalizedUsbPath (
  IN EFI_DEVICE_PATH_PROTOCOL  *DevicePath
  )
{
  EFI_DEVICE_PATH_PROTOCOL  *Copy;
  EFI_DEVICE_PATH_PROTOCOL  *Node;
  EFI_DEVICE_PATH_PROTOCOL  *LastUsbNode;

  if (DevicePath == NULL) {
    return NULL;
  }

  Copy = DuplicateDevicePath (DevicePath);
  if (Copy == NULL) {
    return NULL;
  }

  LastUsbNode = NULL;
  for (Node = Copy; !IsDevicePathEnd (Node); Node = NextDevicePathNode (Node)) {
    if ((DevicePathType (Node) == MESSAGING_DEVICE_PATH) &&
        (DevicePathSubType (Node) == MSG_USB_DP) &&
        (DevicePathNodeLength (Node) >= sizeof (USB_DEVICE_PATH)))
    {
      LastUsbNode = Node;
    }
  }

  // The final USB node identifies this device's interface. Preserve parent
  // hub/interface nodes so separate downstream branches remain distinct.
  if (LastUsbNode != NULL) {
    ((USB_DEVICE_PATH *)LastUsbNode)->InterfaceNumber = 0;
  }

  return Copy;
}

STATIC
BOOLEAN
GxUsbPathsIdentifySameDevice (
  IN EFI_DEVICE_PATH_PROTOCOL  *Left,
  IN EFI_DEVICE_PATH_PROTOCOL  *Right
  )
{
  UINTN  LeftSize;
  UINTN  RightSize;

  if ((Left == NULL) || (Right == NULL)) {
    return FALSE;
  }

  LeftSize  = GetDevicePathSize (Left);
  RightSize = GetDevicePathSize (Right);
  return (BOOLEAN)((LeftSize == RightSize) && (CompareMem (Left, Right, LeftSize) == 0));
}

STATIC
VOID
GxFreeUsbInventory (
  IN GX_USB_INVENTORY_ROW  *Rows,
  IN UINTN                 RowCount
  )
{
  UINTN  Index;

  if (Rows == NULL) {
    return;
  }

  for (Index = 0; Index < RowCount; Index++) {
    if (Rows[Index].NormalizedDevicePath != NULL) {
      FreePool (Rows[Index].NormalizedDevicePath);
    }
  }

  FreePool (Rows);
}

STATIC
VOID
GxClearUsbPageAndDelay (
  IN CONST CHAR8  *Stage
  )
{
  EFI_STATUS  Status;

  if ((gST == NULL) || (gST->ConOut == NULL)) {
    DEBUG ((DEBUG_ERROR, "[GX-USB-PAGE] %a clear skipped ConOut unavailable\n", Stage));
    return;
  }

  Status = gST->ConOut->ClearScreen (gST->ConOut);
  DEBUG ((DEBUG_ERROR, "[GX-USB-PAGE] %a clear status=%r delay_us=%u\n",
    Stage, Status, GX_USB_PAGE_CLEAR_DELAY_US));
  GxDxeDiagnosticsLog ("USB VGA PAGE CLEARED; ONE-SECOND SETTLE BEGIN");
  gBS->Stall (GX_USB_PAGE_CLEAR_DELAY_US);
  GxDxeDiagnosticsLog ("USB VGA PAGE SETTLE RETURNED");
}

STATIC
VOID
GxCopyUsbString (
  IN EFI_USB_IO_PROTOCOL  *UsbIo,
  IN UINT8                StringId,
  OUT CHAR16              *Destination,
  IN UINTN                DestinationChars,
  OUT EFI_STATUS          *StringStatus
  )
{
  EFI_STATUS  Status;
  UINT16      *Languages;
  UINT16      LanguageBytes;
  CHAR16      *String;

  Destination[0] = L'\0';
  *StringStatus  = EFI_NOT_FOUND;
  if ((StringId == 0) || (UsbIo->UsbGetSupportedLanguages == NULL) ||
      (UsbIo->UsbGetStringDescriptor == NULL))
  {
    return;
  }

  Languages     = NULL;
  LanguageBytes = 0;
  Status        = UsbIo->UsbGetSupportedLanguages (UsbIo, &Languages, &LanguageBytes);
  if (EFI_ERROR (Status) || (Languages == NULL) || (LanguageBytes < sizeof (UINT16))) {
    *StringStatus = Status;
    if (Languages != NULL) {
      FreePool (Languages);
    }
    return;
  }

  String = NULL;
  Status = UsbIo->UsbGetStringDescriptor (UsbIo, Languages[0], StringId, &String);
  FreePool (Languages);
  *StringStatus = Status;
  if (!EFI_ERROR (Status) && (String != NULL)) {
    StrnCpyS (Destination, DestinationChars, String, DestinationChars - 1);
  }

  if (String != NULL) {
    FreePool (String);
  }
}

STATIC
VOID
GxLogUsbHostControllers (
  OUT UINTN  *ControllerCount,
  OUT UINTN  *ReadyControllerCount
  )
{
  EFI_STATUS              Status;
  EFI_HANDLE              *Handles;
  UINTN                   Count;
  UINTN                   Index;
  EFI_USB2_HC_PROTOCOL    *HostController;
  EFI_USB_HC_STATE        State;
  UINT8                   MaxSpeed;
  UINT8                   PortCount;
  UINT8                   Is64BitCapable;

  *ControllerCount = 0;
  *ReadyControllerCount = 0;
  Handles          = NULL;
  Count            = 0;
  Status           = gBS->LocateHandleBuffer (
                         ByProtocol,
                         &gEfiUsb2HcProtocolGuid,
                         NULL,
                         &Count,
                         &Handles
                         );
  DEBUG ((DEBUG_ERROR, "[GX-USB] host-controller enumeration status=%r count=%u\n", Status, Count));
  if (EFI_ERROR (Status)) {
    return;
  }

  *ControllerCount = Count;
  for (Index = 0; Index < Count; Index++) {
    HostController = NULL;
    Status         = gBS->HandleProtocol (Handles[Index], &gEfiUsb2HcProtocolGuid, (VOID **)&HostController);
    if (EFI_ERROR (Status) || (HostController == NULL)) {
      DEBUG ((DEBUG_ERROR, "[GX-USB] host-controller[%u] open status=%r\n", Index, Status));
      continue;
    }

    MaxSpeed       = 0;
    PortCount      = 0;
    Is64BitCapable = 0;
    State          = EfiUsbHcStateMaximum;
    if (HostController->GetCapability == NULL) {
      Status = EFI_UNSUPPORTED;
    } else {
      Status = HostController->GetCapability (
                 HostController,
                 &MaxSpeed,
                 &PortCount,
                 &Is64BitCapable
                 );
    }
    DEBUG ((DEBUG_ERROR,
      "[GX-USB] host-controller[%u] capability status=%r max_speed=%u ports=%u dma64=%u\n",
      Index, Status, MaxSpeed, PortCount, Is64BitCapable));

    if (HostController->GetState != NULL) {
      Status = HostController->GetState (HostController, &State);
      if (!EFI_ERROR (Status) && (State == EfiUsbHcStateOperational)) {
        (*ReadyControllerCount)++;
      }
      DEBUG ((DEBUG_ERROR, "[GX-USB] host-controller[%u] initialized=%a state_status=%r state=%u\n",
        Index, (!EFI_ERROR (Status) && (State == EfiUsbHcStateOperational)) ? "READY" : "NOT_READY",
        Status, State));
    } else {
      DEBUG ((DEBUG_ERROR, "[GX-USB] host-controller[%u] state query unsupported\n", Index));
    }
  }

  FreePool (Handles);
}

STATIC
UINTN
GxCollectUsbInventory (
  OUT GX_USB_INVENTORY_ROW  **Rows,
  OUT EFI_STATUS            *EnumerationStatus,
  OUT UINTN                 *InterfaceCount,
  OUT UINTN                 *UsbDevicePathCount,
  OUT UINTN                 *UnclassifiedInterfaceCount,
  OUT UINTN                 *MassStorageInterfaceCount
  )
{
  EFI_STATUS                  Status;
  EFI_STATUS                  DeviceStatus;
  EFI_STATUS                  ConfigStatus;
  EFI_STATUS                  InterfaceStatus;
  EFI_STATUS                  ManufacturerStatus;
  EFI_STATUS                  ProductStatus;
  EFI_STATUS                  SerialStatus;
  EFI_STATUS                  DevicePathStatus;
  EFI_HANDLE                  *Handles;
  UINTN                       Count;
  UINTN                       Index;
  UINTN                       Stored;
  UINTN                       RowIndex;
  EFI_USB_IO_PROTOCOL         *UsbIo;
  EFI_USB_DEVICE_DESCRIPTOR   DeviceDescriptor;
  EFI_USB_CONFIG_DESCRIPTOR   ConfigDescriptor;
  EFI_USB_INTERFACE_DESCRIPTOR InterfaceDescriptor;
  EFI_DEVICE_PATH_PROTOCOL    *DevicePath;
  EFI_DEVICE_PATH_PROTOCOL    *NormalizedPath;
  CHAR16                      *DevicePathText;
  CHAR16                      Manufacturer[40];
  CHAR16                      Product[40];
  CHAR16                      Serial[32];
  GX_USB_INVENTORY_ROW        *Row;
  BOOLEAN                     UsbPath;

  Handles = NULL;
  Count   = 0;
  Stored  = 0;
  *Rows   = NULL;
  *InterfaceCount = 0;
  *UsbDevicePathCount = 0;
  *UnclassifiedInterfaceCount = 0;
  *MassStorageInterfaceCount = 0;
  *EnumerationStatus = EFI_NOT_FOUND;
  Status  = gBS->LocateHandleBuffer (ByProtocol, &gEfiUsbIoProtocolGuid, NULL, &Count, &Handles);
  *EnumerationStatus = Status;
  DEBUG ((DEBUG_ERROR, "[GX-USB] device-interface enumeration status=%r handles=%u\n", Status, Count));
  if (EFI_ERROR (Status)) {
    return 0;
  }
  *InterfaceCount = Count;

  if (Count == 0) {
    if (Handles != NULL) {
      FreePool (Handles);
    }
    return 0;
  }

  if (Count > (MAX_UINTN / sizeof (**Rows))) {
    *EnumerationStatus = EFI_OUT_OF_RESOURCES;
    FreePool (Handles);
    return 0;
  }

  *Rows = AllocateZeroPool (sizeof (**Rows) * Count);
  if (*Rows == NULL) {
    *EnumerationStatus = EFI_OUT_OF_RESOURCES;
    DEBUG ((DEBUG_ERROR, "[GX-USB] inventory allocation failed handles=%u\n", Count));
    FreePool (Handles);
    return 0;
  }

  for (Index = 0; Index < Count; Index++) {
    UsbIo             = NULL;
    DevicePath        = NULL;
    NormalizedPath    = NULL;
    DevicePathText    = NULL;
    ZeroMem (&DeviceDescriptor, sizeof (DeviceDescriptor));
    ZeroMem (&ConfigDescriptor, sizeof (ConfigDescriptor));
    ZeroMem (&InterfaceDescriptor, sizeof (InterfaceDescriptor));
    Manufacturer[0]   = L'\0';
    Product[0]        = L'\0';
    Serial[0]         = L'\0';

    Status = gBS->HandleProtocol (Handles[Index], &gEfiUsbIoProtocolGuid, (VOID **)&UsbIo);
    DeviceStatus = Status;
    if (!EFI_ERROR (Status) && (UsbIo == NULL)) {
      DeviceStatus = EFI_DEVICE_ERROR;
    }
    if (!EFI_ERROR (Status) && (UsbIo != NULL)) {
      DeviceStatus = (UsbIo->UsbGetDeviceDescriptor != NULL) ?
                     UsbIo->UsbGetDeviceDescriptor (UsbIo, &DeviceDescriptor) : EFI_UNSUPPORTED;
    }

    ConfigStatus = EFI_NOT_FOUND;
    if (!EFI_ERROR (Status) && (UsbIo != NULL)) {
      ConfigStatus = (UsbIo->UsbGetConfigDescriptor != NULL) ?
                     UsbIo->UsbGetConfigDescriptor (UsbIo, &ConfigDescriptor) : EFI_UNSUPPORTED;
    }

    InterfaceStatus = EFI_NOT_FOUND;
    if (!EFI_ERROR (Status) && (UsbIo != NULL)) {
      InterfaceStatus = (UsbIo->UsbGetInterfaceDescriptor != NULL) ?
                        UsbIo->UsbGetInterfaceDescriptor (UsbIo, &InterfaceDescriptor) : EFI_UNSUPPORTED;
    }

    if (!EFI_ERROR (InterfaceStatus) && (InterfaceDescriptor.InterfaceClass == 0x08)) {
      (*MassStorageInterfaceCount)++;
    }

    ManufacturerStatus = EFI_NOT_FOUND;
    ProductStatus      = EFI_NOT_FOUND;
    SerialStatus       = EFI_NOT_FOUND;
    if (!EFI_ERROR (DeviceStatus) && (UsbIo != NULL)) {
      GxCopyUsbString (UsbIo, DeviceDescriptor.StrManufacturer, Manufacturer,
                       ARRAY_SIZE (Manufacturer), &ManufacturerStatus);
      GxCopyUsbString (UsbIo, DeviceDescriptor.StrProduct, Product,
                       ARRAY_SIZE (Product), &ProductStatus);
      GxCopyUsbString (UsbIo, DeviceDescriptor.StrSerialNumber, Serial,
                       ARRAY_SIZE (Serial), &SerialStatus);
    }

    DevicePathStatus = gBS->HandleProtocol (Handles[Index], &gEfiDevicePathProtocolGuid, (VOID **)&DevicePath);
    UsbPath = !EFI_ERROR (DevicePathStatus) && GxDevicePathHasUsbNode (DevicePath);
    if (!EFI_ERROR (DevicePathStatus) && (DevicePath != NULL)) {
      DevicePathText = ConvertDevicePathToText (DevicePath, FALSE, FALSE);
    }
    if (UsbPath) {
      NormalizedPath = GxDuplicateNormalizedUsbPath (DevicePath);
    }

    Row = NULL;
    if (NormalizedPath != NULL) {
      for (RowIndex = 0; RowIndex < Stored; RowIndex++) {
        if (GxUsbPathsIdentifySameDevice ((*Rows)[RowIndex].NormalizedDevicePath, NormalizedPath)) {
          Row = &(*Rows)[RowIndex];
          break;
        }
      }
    }

    if (Row == NULL) {
      Row = &(*Rows)[Stored++];
      Row->NormalizedDevicePath = NormalizedPath;
      NormalizedPath = NULL;
      Row->UsbPath = UsbPath;
      if (UsbPath) {
        (*UsbDevicePathCount)++;
      } else {
        (*UnclassifiedInterfaceCount)++;
      }
    } else if (NormalizedPath != NULL) {
      FreePool (NormalizedPath);
      NormalizedPath = NULL;
    }

    Row->InterfaceCount++;
    if (!EFI_ERROR (InterfaceStatus)) {
      if (!Row->InterfaceClassSet) {
        Row->InterfaceClass = InterfaceDescriptor.InterfaceClass;
        Row->InterfaceClassSet = TRUE;
      } else if (Row->InterfaceClass != InterfaceDescriptor.InterfaceClass) {
        Row->MixedInterfaceClasses = TRUE;
      }
    }
    if (!EFI_ERROR (InterfaceStatus) && (InterfaceDescriptor.InterfaceClass == 0x08)) {
      Row->HasMassStorage = TRUE;
    }
    if (!Row->DescriptorReady && !EFI_ERROR (DeviceStatus)) {
      Row->DescriptorReady = TRUE;
      Row->VendorId        = DeviceDescriptor.IdVendor;
      Row->ProductId       = DeviceDescriptor.IdProduct;
      StrnCpyS (Row->Product, ARRAY_SIZE (Row->Product), Product, ARRAY_SIZE (Product) - 1);
    }

    DEBUG ((DEBUG_ERROR,
      "[GX-USB] interface[%u] usb_io=%a handle=%p device_status=%r config_status=%r interface_status=%r "
      "vid=%04x pid=%04x usb=%04x release=%04x max_packet=%u configs=%u "
      "dev_class=%02x/%02x/%02x config_value=%u interfaces=%u attributes=%02x max_power=%u "
      "if=%u if_class=%02x/%02x/%02x "
      "mfg_status=%r mfg='%s' product_status=%r product='%s' serial_status=%r serial='%s' "
      "usb_path=%u speed=not-exposed-by-EFI_USB_IO path='%s'\n",
      Index, EFI_ERROR (DeviceStatus) ? "NOT_READY" : "READY", Handles[Index],
      DeviceStatus, ConfigStatus, InterfaceStatus,
      DeviceDescriptor.IdVendor, DeviceDescriptor.IdProduct,
      DeviceDescriptor.BcdUSB, DeviceDescriptor.BcdDevice,
      DeviceDescriptor.MaxPacketSize0, DeviceDescriptor.NumConfigurations,
      DeviceDescriptor.DeviceClass, DeviceDescriptor.DeviceSubClass, DeviceDescriptor.DeviceProtocol,
      ConfigDescriptor.ConfigurationValue, ConfigDescriptor.NumInterfaces,
      ConfigDescriptor.Attributes, ConfigDescriptor.MaxPower,
      InterfaceDescriptor.InterfaceNumber, InterfaceDescriptor.InterfaceClass,
      InterfaceDescriptor.InterfaceSubClass, InterfaceDescriptor.InterfaceProtocol,
      ManufacturerStatus, Manufacturer, ProductStatus, Product, SerialStatus, Serial,
      UsbPath,
      (DevicePathText != NULL) ? DevicePathText : L"<unavailable>"));
    DEBUG ((DEBUG_ERROR,
      "[GX-USB-DEVICE] interface=%u unique_row=%u path_status=%r physical_path=%u "
      "row_interfaces=%u vid=%04x pid=%04x descriptor=%a mass_storage=%u\n",
      Index, (UINTN)(Row - *Rows), DevicePathStatus, Row->UsbPath,
      Row->InterfaceCount, Row->VendorId, Row->ProductId,
      Row->DescriptorReady ? "READY" : "ERROR", Row->HasMassStorage));

    if (DevicePathText != NULL) {
      FreePool (DevicePathText);
    }
  }

  for (Index = 0; Index < Stored; Index++) {
    if ((*Rows)[Index].MixedInterfaceClasses) {
      StrCpyS ((*Rows)[Index].ClassText, ARRAY_SIZE ((*Rows)[Index].ClassText), L"MIX");
    } else if ((*Rows)[Index].InterfaceClassSet) {
      UnicodeSPrint ((*Rows)[Index].ClassText, sizeof ((*Rows)[Index].ClassText), L"%02x",
                     (*Rows)[Index].InterfaceClass);
    } else {
      StrCpyS ((*Rows)[Index].ClassText, ARRAY_SIZE ((*Rows)[Index].ClassText), L"N/A");
    }

    UnicodeSPrint ((*Rows)[Index].Text, sizeof ((*Rows)[Index].Text),
      L"%02u %s %04x:%04x IF=%u CLS=%s MSC=%a %s",
      Index + 1,
      (*Rows)[Index].UsbPath ? L"USB" : L"NO-PATH",
      (*Rows)[Index].DescriptorReady ? L"READY" : L"ERROR",
      (*Rows)[Index].VendorId,
      (*Rows)[Index].ProductId,
      (*Rows)[Index].InterfaceCount,
      (*Rows)[Index].ClassText,
      (*Rows)[Index].HasMassStorage ? "Y" : "N",
      (*Rows)[Index].Product);
  }

  FreePool (Handles);
  return Stored;
}

STATIC
VOID
GxLogUsbStorage (
  OUT GX_USB_STORAGE_SUMMARY  *Summary
  )
{
  EFI_STATUS                       Status;
  EFI_HANDLE                       *Handles;
  UINTN                            Count;
  UINTN                            Index;
  EFI_BLOCK_IO_PROTOCOL            *BlockIo;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL  *SimpleFileSystem;
  EFI_FILE_PROTOCOL                *Root;
  EFI_DEVICE_PATH_PROTOCOL         *DevicePath;
  CHAR16                           *DevicePathText;
  BOOLEAN                          UsbPath;

  ZeroMem (Summary, sizeof (*Summary));
  Summary->UsbLastOpenVolumeStatus = EFI_NOT_READY;
  Handles = NULL;
  Count   = 0;
  Status  = gBS->LocateHandleBuffer (ByProtocol, &gEfiBlockIoProtocolGuid, NULL, &Count, &Handles);
  DEBUG ((DEBUG_ERROR, "[GX-USB-STORAGE] BlockIo enumeration status=%r handles=%u\n", Status, Count));
  if (!EFI_ERROR (Status)) {
    Summary->BlockIoCount = Count;
    for (Index = 0; Index < Count; Index++) {
      BlockIo         = NULL;
      DevicePath      = NULL;
      DevicePathText  = NULL;
      UsbPath         = FALSE;
      Status          = gBS->HandleProtocol (Handles[Index], &gEfiBlockIoProtocolGuid, (VOID **)&BlockIo);
      if (!EFI_ERROR (Status)) {
        Status = gBS->HandleProtocol (Handles[Index], &gEfiDevicePathProtocolGuid, (VOID **)&DevicePath);
      }
      if (!EFI_ERROR (Status)) {
        UsbPath = GxDevicePathHasUsbNode (DevicePath);
        if (DevicePath != NULL) {
          DevicePathText = ConvertDevicePathToText (DevicePath, FALSE, FALSE);
        }
      }

      if (UsbPath) {
        Summary->UsbBlockIoCount++;
        if ((BlockIo != NULL) && (BlockIo->Media != NULL)) {
          if (BlockIo->Media->RemovableMedia) {
            Summary->UsbRemovableCount++;
          }
          if (BlockIo->Media->MediaPresent) {
            Summary->UsbMediaPresentCount++;
          }
        }
      }

      DEBUG ((DEBUG_ERROR,
        "[GX-USB-STORAGE] block[%u] status=%r usb_path=%u handle=%p media_id=%u present=%u removable=%u "
        "logical_partition=%u read_only=%u block_size=%u last_lba=0x%lx path='%s'\n",
        Index, Status, UsbPath, Handles[Index],
        ((BlockIo != NULL) && (BlockIo->Media != NULL)) ? BlockIo->Media->MediaId : 0,
        ((BlockIo != NULL) && (BlockIo->Media != NULL)) ? BlockIo->Media->MediaPresent : 0,
        ((BlockIo != NULL) && (BlockIo->Media != NULL)) ? BlockIo->Media->RemovableMedia : 0,
        ((BlockIo != NULL) && (BlockIo->Media != NULL)) ? BlockIo->Media->LogicalPartition : 0,
        ((BlockIo != NULL) && (BlockIo->Media != NULL)) ? BlockIo->Media->ReadOnly : 0,
        ((BlockIo != NULL) && (BlockIo->Media != NULL)) ? BlockIo->Media->BlockSize : 0,
        ((BlockIo != NULL) && (BlockIo->Media != NULL)) ? BlockIo->Media->LastBlock : 0,
        (DevicePathText != NULL) ? DevicePathText : L"<unavailable>"));

      if (DevicePathText != NULL) {
        FreePool (DevicePathText);
      }
    }

    FreePool (Handles);
  }

  Handles = NULL;
  Count   = 0;
  Status  = gBS->LocateHandleBuffer (
                   ByProtocol,
                   &gEfiSimpleFileSystemProtocolGuid,
                   NULL,
                   &Count,
                   &Handles
                   );
  DEBUG ((DEBUG_ERROR, "[GX-USB-STORAGE] SimpleFileSystem enumeration status=%r handles=%u\n", Status, Count));
  if (!EFI_ERROR (Status)) {
    Summary->SimpleFileSystemCount = Count;
    for (Index = 0; Index < Count; Index++) {
      SimpleFileSystem = NULL;
      Root             = NULL;
      DevicePath       = NULL;
      DevicePathText   = NULL;
      UsbPath          = FALSE;
      Status           = gBS->HandleProtocol (
                           Handles[Index],
                           &gEfiSimpleFileSystemProtocolGuid,
                           (VOID **)&SimpleFileSystem
                           );
      if (!EFI_ERROR (Status)) {
        Status = gBS->HandleProtocol (Handles[Index], &gEfiDevicePathProtocolGuid, (VOID **)&DevicePath);
      }
      if (!EFI_ERROR (Status)) {
        UsbPath = GxDevicePathHasUsbNode (DevicePath);
        if (DevicePath != NULL) {
          DevicePathText = ConvertDevicePathToText (DevicePath, FALSE, FALSE);
        }
      }
      if (UsbPath) {
        Summary->UsbSimpleFileSystemCount++;
      }

      if (!EFI_ERROR (Status) && (SimpleFileSystem != NULL) && (SimpleFileSystem->OpenVolume != NULL)) {
        Status = SimpleFileSystem->OpenVolume (SimpleFileSystem, &Root);
      } else if (!EFI_ERROR (Status)) {
        Status = EFI_UNSUPPORTED;
      }
      if (!EFI_ERROR (Status) && (Root != NULL)) {
        if (UsbPath) {
          Summary->UsbVolumeOpenCount++;
          Summary->UsbLastOpenVolumeStatus = EFI_SUCCESS;
        }
        Root->Close (Root);
      } else if (UsbPath) {
        Summary->UsbVolumeOpenFailCount++;
        Summary->UsbLastOpenVolumeStatus = Status;
      }

      DEBUG ((DEBUG_ERROR,
        "[GX-USB-STORAGE] filesystem[%u] usb_path=%u open_volume_status=%r handle=%p path='%s'\n",
        Index, UsbPath, Status, Handles[Index],
        (DevicePathText != NULL) ? DevicePathText : L"<unavailable>"));
      if (DevicePathText != NULL) {
        FreePool (DevicePathText);
      }
    }

    FreePool (Handles);
  }

  DEBUG ((DEBUG_ERROR,
    "[GX-USB-STORAGE] summary block_io=%u usb_block_io=%u usb_removable=%u usb_media_present=%u "
    "simple_fs=%u usb_simple_fs=%u usb_volume_open=%u usb_volume_open_failed=%u last_open_status=%r\n",
    Summary->BlockIoCount, Summary->UsbBlockIoCount, Summary->UsbRemovableCount,
    Summary->UsbMediaPresentCount, Summary->SimpleFileSystemCount,
    Summary->UsbSimpleFileSystemCount, Summary->UsbVolumeOpenCount,
    Summary->UsbVolumeOpenFailCount, Summary->UsbLastOpenVolumeStatus));
}

STATIC
VOID
GxRenderUsbDriverStatusPage (
  IN GX_USB_DRIVER_STATE  *States,
  IN UINTN                DiscoveredCount,
  IN UINTN                InitializedCount,
  IN UINTN                LoadAttemptedCount,
  IN UINTN                LoadedCount,
  IN UINTN                LoadFailedCount,
  IN UINTN                StartAttemptedCount,
  IN UINTN                StartedCount,
  IN UINTN                StartFailedCount,
  IN EFI_STATUS           ProtocolStatus
  )
{
  UINTN   Index;
  CONST CHAR16  *LoadText;
  CONST CHAR16  *ImageText;
  CONST CHAR16  *StartText;
  CONST CHAR16  *DiscoveryText;
  CONST CHAR16  *InitializedText;

  if ((gST == NULL) || (gST->ConOut == NULL)) {
    return;
  }

  Print (L"GOOXI G4DEL - USB DXE DRIVER STATUS (%u configured)\r\n", ARRAY_SIZE (mGxUsbDrivers));
  Print (L"INIT is dispatcher state only; it does not prove hardware binding.\r\n");
  Print (L"Discovered %u  Init-flag %u  Load tried %u  Image loaded %u\r\n",
         DiscoveredCount, InitializedCount, LoadAttemptedCount, LoadedCount);
  Print (L"Load failed %u  Entry tried %u  Entry OK %u  Entry failed %u\r\n\r\n",
         LoadFailedCount, StartAttemptedCount, StartedCount, StartFailedCount);
  Print (L"Dispatcher query status: %r\r\n", ProtocolStatus);
  Print (L"DRIVER                        D I LOAD IMG ENTRY\r\n");
  for (Index = 0; Index < ARRAY_SIZE (mGxUsbDrivers); Index++) {
    DiscoveryText = L"NO";
    InitializedText = L"NO";
    LoadText      = L"N/A";
    ImageText     = L"NO";
    StartText     = L"N/A";
    if (!EFI_ERROR (ProtocolStatus) && !EFI_ERROR (States[Index].QueryStatus)) {
      DiscoveryText = States[Index].State.Discovered ? L"YES" : L"NO";
      InitializedText = States[Index].State.Initialized ? L"YES" : L"NO";
      if (States[Index].State.LoadAttempted) {
        LoadText = EFI_ERROR (States[Index].State.LoadStatus) ? L"FAIL" : L"OK";
      } else {
        LoadText = L"NOT-TRIED";
      }
      ImageText = States[Index].State.ImageLoaded ? L"YES" : L"NO";

      if (States[Index].State.StartAttempted) {
        StartText = EFI_ERROR (States[Index].State.StartStatus) ? L"FAIL" : L"OK";
      } else {
        StartText = L"NOT-RUN";
      }
    }

    Print (L"%s D=%s I=%s L=%s IMG=%s EP=%s\r\n",
      mGxUsbDrivers[Index].DisplayName,
      DiscoveryText,
      InitializedText,
      LoadText,
      ImageText,
      StartText);
  }

  Print (L"\r\nLOAD/START EFI_STATUS values and complete FV GUIDs are in SOL.\r\n");
  Print (L"This page advances in %u seconds; Enter/Esc continues.\r\n", GX_USB_PAGE_SECONDS);
}

STATIC
BOOLEAN
GxWaitForUsbPage (
  OUT EFI_INPUT_KEY  *Key,
  OUT BOOLEAN        *KeyAvailable
  )
{
  EFI_STATUS  Status;
  EFI_EVENT   TimerEvent;
  EFI_EVENT   Events[2];
  UINTN       EventCount;
  UINTN       WaitIndex;

  *KeyAvailable = FALSE;
  ZeroMem (Key, sizeof (*Key));
  TimerEvent = NULL;
  Status = gBS->CreateEvent (EVT_TIMER, TPL_CALLBACK, NULL, NULL, &TimerEvent);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "[GX-USB-PAGE] timer creation failed status=%r; advance immediately\n", Status));
    return FALSE;
  }

  Status = gBS->SetTimer (TimerEvent, TimerRelative, GX_USB_PAGE_SECONDS * 10 * 1000 * 1000);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "[GX-USB-PAGE] timer start failed status=%r; advance immediately\n", Status));
    gBS->CloseEvent (TimerEvent);
    return FALSE;
  }

  Events[1] = TimerEvent;
  if ((gST == NULL) || (gST->ConIn == NULL) || (gST->ConIn->WaitForKey == NULL)) {
    Events[0] = TimerEvent;
    EventCount = 1;
  } else {
    Events[0] = gST->ConIn->WaitForKey;
    EventCount = ARRAY_SIZE (Events);
  }

  Status = gBS->WaitForEvent (EventCount, Events, &WaitIndex);
  if (!EFI_ERROR (Status) && (WaitIndex == 0) && (Events[0] != TimerEvent) &&
      (gST != NULL) && (gST->ConIn != NULL))
  {
    Status = gST->ConIn->ReadKeyStroke (gST->ConIn, Key);
    *KeyAvailable = !EFI_ERROR (Status);
  }

  gBS->CloseEvent (TimerEvent);
  return *KeyAvailable;
}

VOID
GxDisplayUsbDriverStatus (
  VOID
  )
{
  EFI_STATUS                    Status;
  EFI_STATUS                    ProtocolStatus;
  GX_DXE_DIAGNOSTICS_PROTOCOL   *Diagnostics;
  GX_USB_DRIVER_STATE            States[ARRAY_SIZE (mGxUsbDrivers)];
  UINTN                          Index;
  UINTN                          DiscoveredCount;
  UINTN                          InitializedCount;
  UINTN                          LoadAttemptedCount;
  UINTN                          LoadedCount;
  UINTN                          LoadFailedCount;
  UINTN                          StartAttemptedCount;
  UINTN                          StartedCount;
  UINTN                          StartFailedCount;
  UINTN                          PendingCount;
  EFI_INPUT_KEY                  Key;
  BOOLEAN                        KeyAvailable;
  CHAR8                          Message[128];
  GxDxeDiagnosticsLog ("USB DRIVER STATUS PAGE BEGIN");
  DEBUG ((DEBUG_ERROR, "[GX-USB-DRIVER] status query begin configured=%u\n", ARRAY_SIZE (mGxUsbDrivers)));

  Diagnostics = NULL;
  ProtocolStatus = gBS->LocateProtocol (&gGxDxeDiagnosticsProtocolGuid, NULL, (VOID **)&Diagnostics);
  if (!EFI_ERROR (ProtocolStatus) && ((Diagnostics == NULL) || (Diagnostics->GetDriverStatus == NULL))) {
    ProtocolStatus = EFI_UNSUPPORTED;
  }

  DiscoveredCount = 0;
  InitializedCount = 0;
  LoadAttemptedCount = 0;
  LoadedCount = 0;
  LoadFailedCount = 0;
  StartAttemptedCount = 0;
  StartedCount = 0;
  StartFailedCount = 0;
  PendingCount = 0;
  ZeroMem (States, sizeof (States));

  for (Index = 0; Index < ARRAY_SIZE (mGxUsbDrivers); Index++) {
    States[Index].QueryStatus = ProtocolStatus;
    if (!EFI_ERROR (ProtocolStatus)) {
      States[Index].QueryStatus = Diagnostics->GetDriverStatus (&mGxUsbDrivers[Index].FileGuid, &States[Index].State);
    }

    if (!EFI_ERROR (States[Index].QueryStatus)) {
      if (States[Index].State.Discovered) {
        DiscoveredCount++;
      }
      if (States[Index].State.Initialized) {
        InitializedCount++;
      }
      if (States[Index].State.ImageLoaded) {
        LoadedCount++;
      }
      if (States[Index].State.LoadAttempted) {
        LoadAttemptedCount++;
      }
      if (States[Index].State.LoadAttempted && EFI_ERROR (States[Index].State.LoadStatus)) {
        LoadFailedCount++;
      }
      if (States[Index].State.StartAttempted && !EFI_ERROR (States[Index].State.StartStatus)) {
        StartedCount++;
      }
      if (States[Index].State.StartAttempted) {
        StartAttemptedCount++;
      }
      if (States[Index].State.StartAttempted && EFI_ERROR (States[Index].State.StartStatus)) {
        StartFailedCount++;
      }
      if (States[Index].State.Discovered && !States[Index].State.StartAttempted) {
        PendingCount++;
      }
    }

    DEBUG ((DEBUG_ERROR,
      "[GX-USB-DRIVER] index=%u name=%a guid=%g query=%r discovered=%u initialized_flag=%u scheduled=%u dependent=%u "
      "unrequested=%u untrusted=%u depex_error=%u load_attempted=%u image_loaded=%u load_status=%r "
      "start_attempted=%u start_status=%r\n",
      Index, mGxUsbDrivers[Index].Name, &mGxUsbDrivers[Index].FileGuid, States[Index].QueryStatus,
      States[Index].State.Discovered, States[Index].State.Initialized, States[Index].State.Scheduled,
      States[Index].State.Dependent, States[Index].State.Unrequested, States[Index].State.Untrusted,
      States[Index].State.DepexProtocolError, States[Index].State.LoadAttempted, States[Index].State.ImageLoaded,
      States[Index].State.LoadStatus, States[Index].State.StartAttempted, States[Index].State.StartStatus));

    AsciiSPrint (Message, sizeof (Message), "USB %a D%u I%u L%a IMG%u EP%a",
      mGxUsbDrivers[Index].Name,
      States[Index].State.Discovered,
      States[Index].State.Initialized,
      (States[Index].State.LoadAttempted && !EFI_ERROR (States[Index].State.LoadStatus)) ? "OK" :
      (States[Index].State.LoadAttempted ? "FAIL" : "NO"),
      States[Index].State.ImageLoaded,
      (States[Index].State.StartAttempted && !EFI_ERROR (States[Index].State.StartStatus)) ? "OK" :
      (States[Index].State.StartAttempted ? "FAIL" : "NO"));
    GxDxeDiagnosticsLog (Message);
  }

  DEBUG ((DEBUG_ERROR,
    "[GX-USB-DRIVER] summary configured=%u discovered=%u initialized_flag=%u load_attempted=%u loaded=%u load_failed=%u "
    "entry_attempted=%u entry_success=%u entry_failed=%u pending_entry=%u protocol_status=%r\n",
    ARRAY_SIZE (mGxUsbDrivers), DiscoveredCount, InitializedCount, LoadAttemptedCount,
    LoadedCount, LoadFailedCount, StartAttemptedCount, StartedCount, StartFailedCount,
    PendingCount, ProtocolStatus));
  GxDxeDiagnosticsLog ("USB DRIVER STATUS PAGE SUMMARY READY");

  GxClearUsbPageAndDelay ("driver-status");
  GxRenderUsbDriverStatusPage (States, DiscoveredCount, InitializedCount, LoadAttemptedCount,
                               LoadedCount, LoadFailedCount, StartAttemptedCount,
                               StartedCount, StartFailedCount, ProtocolStatus);
  Status = GxWaitForUsbPage (&Key, &KeyAvailable) ? EFI_SUCCESS : EFI_TIMEOUT;
  DEBUG ((DEBUG_ERROR, "[GX-USB-DRIVER] status page return status=%r key_available=%u key=0x%x scan=0x%x\n",
    Status, KeyAvailable, Key.UnicodeChar, Key.ScanCode));
  GxDxeDiagnosticsLog ("USB DRIVER STATUS PAGE RETURNED");
}

STATIC
VOID
GxRenderUsbPage (
  IN GX_USB_INVENTORY_ROW  *Rows,
  IN UINTN                 RowCount,
  IN UINTN                 FirstRow,
  IN UINTN                 RowsPerPage,
  IN UINTN                 ControllerCount,
  IN UINTN                 ReadyControllerCount,
  IN UINTN                 InterfaceCount,
  IN UINTN                 UsbDevicePathCount,
  IN UINTN                 UnclassifiedInterfaceCount,
  IN UINTN                 MassStorageInterfaceCount,
  IN GX_USB_STORAGE_SUMMARY *StorageSummary,
  IN EFI_STATUS            EnumerationStatus
  )
{
  UINTN  Row;
  UINTN  LastRow;

  gST->ConOut->ClearScreen (gST->ConOut);
  Print (L"GOOXI G4DEL USB INVENTORY\r\n");
  Print (L"Unique USB device paths: %u; EFI_USB_IO interfaces: %u; no-path: %u\r\n",
         UsbDevicePathCount, InterfaceCount, UnclassifiedInterfaceCount);
  Print (L"Host controllers READY: %u/%u; mass-storage interfaces: %u\r\n",
         ReadyControllerCount, ControllerCount, MassStorageInterfaceCount);
  Print (L"USB Block I/O: %u (%u removable, %u media present)\r\n",
         StorageSummary->UsbBlockIoCount,
         StorageSummary->UsbRemovableCount, StorageSummary->UsbMediaPresentCount);
  Print (L"USB SimpleFS: %u; OpenVolume OK/FAIL: %u/%u; last=%r\r\n",
         StorageSummary->UsbSimpleFileSystemCount, StorageSummary->UsbVolumeOpenCount,
         StorageSummary->UsbVolumeOpenFailCount, StorageSummary->UsbLastOpenVolumeStatus);
  Print (L"Descriptor-ready USB devices are shown as READY; no-path handles are unclassified.\r\n\r\n");

  LastRow = FirstRow + RowsPerPage;
  if (LastRow > RowCount) {
    LastRow = RowCount;
  }

  if (RowCount == 0 && EnumerationStatus == EFI_NOT_FOUND) {
    Print (L"LocateHandleBuffer found no EFI_USB_IO interfaces (status %r).\r\n", EnumerationStatus);
  } else if (RowCount == 0) {
    Print (L"USB inventory unavailable: %r\r\n", EnumerationStatus);
  } else {
    for (Row = FirstRow; Row < LastRow; Row++) {
      Print (L"%s\r\n", Rows[Row].Text);
    }
  }

  Print (L"\r\nRows %u-%u of %u. PgUp/PgDn pages; Enter/Esc skips to Shell.\r\n",
         (RowCount == 0) ? 0 : FirstRow + 1, LastRow, RowCount);
  Print (L"Next page or Shell handoff in %u seconds.\r\n", GX_USB_PAGE_SECONDS);
}

VOID
GxDisplayUsbInventory (
  VOID
  )
{
  EFI_STATUS             Status;
  EFI_STATUS             EnumerationStatus;
  UINTN                  ControllerCount;
  UINTN                  ReadyControllerCount;
  UINTN                  RowCount;
  UINTN                  InterfaceCount;
  UINTN                  UsbDevicePathCount;
  UINTN                  UnclassifiedInterfaceCount;
  UINTN                  MassStorageInterfaceCount;
  UINTN                  FirstRow;
  UINTN                  RowsPerPage;
  UINTN                  Columns;
  UINTN                  RowsOnScreen;
  UINTN                  PageCount;
  UINTN                  CurrentPage;
  GX_USB_INVENTORY_ROW   *Inventory;
  GX_USB_STORAGE_SUMMARY StorageSummary;
  EFI_INPUT_KEY          Key;
  BOOLEAN                KeyAvailable;
  BOOLEAN                ContinueToShell;
  CHAR8                  Message[128];

  GxDxeDiagnosticsLog ("USB INVENTORY ENUMERATION BEGIN");
  DEBUG ((DEBUG_ERROR, "[GX-USB] inventory enumeration begin\n"));
  GxLogUsbHostControllers (&ControllerCount, &ReadyControllerCount);
  RowCount = GxCollectUsbInventory (&Inventory, &EnumerationStatus, &InterfaceCount,
                                    &UsbDevicePathCount, &UnclassifiedInterfaceCount,
                                    &MassStorageInterfaceCount);
  GxLogUsbStorage (&StorageSummary);
  DEBUG ((DEBUG_ERROR,
    "[GX-USB] inventory summary host_controllers_ready=%u/%u unique_usb_device_paths=%u usb_io_interfaces=%u "
    "unclassified_interfaces=%u mass_storage_interfaces=%u enumeration_status=%r\n",
    ReadyControllerCount, ControllerCount, UsbDevicePathCount, InterfaceCount,
    UnclassifiedInterfaceCount, MassStorageInterfaceCount, EnumerationStatus));
  AsciiSPrint (Message, sizeof (Message), "USB COUNT DEV%u IF%u NO-PATH%u",
    UsbDevicePathCount, InterfaceCount, UnclassifiedInterfaceCount);
  GxDxeDiagnosticsLog (Message);
  for (FirstRow = 0; FirstRow < RowCount; FirstRow++) {
    if (Inventory[FirstRow].MixedInterfaceClasses) {
      AsciiSPrint (Message, sizeof (Message), "USB DEV %02u %a %04x:%04x IF%u CLS=MIX MSC%u",
        FirstRow + 1,
        Inventory[FirstRow].UsbPath ? (Inventory[FirstRow].DescriptorReady ? "READY" : "DESC-ERR") : "NO-PATH",
        Inventory[FirstRow].VendorId, Inventory[FirstRow].ProductId,
        Inventory[FirstRow].InterfaceCount, Inventory[FirstRow].HasMassStorage);
    } else if (Inventory[FirstRow].InterfaceClassSet) {
      AsciiSPrint (Message, sizeof (Message), "USB DEV %02u %a %04x:%04x IF%u CLS=%02x MSC%u",
        FirstRow + 1,
        Inventory[FirstRow].UsbPath ? (Inventory[FirstRow].DescriptorReady ? "READY" : "DESC-ERR") : "NO-PATH",
        Inventory[FirstRow].VendorId, Inventory[FirstRow].ProductId,
        Inventory[FirstRow].InterfaceCount, Inventory[FirstRow].InterfaceClass,
        Inventory[FirstRow].HasMassStorage);
    } else {
      AsciiSPrint (Message, sizeof (Message), "USB DEV %02u %a %04x:%04x IF%u CLS=N/A MSC%u",
        FirstRow + 1,
        Inventory[FirstRow].UsbPath ? (Inventory[FirstRow].DescriptorReady ? "READY" : "DESC-ERR") : "NO-PATH",
        Inventory[FirstRow].VendorId, Inventory[FirstRow].ProductId,
        Inventory[FirstRow].InterfaceCount, Inventory[FirstRow].HasMassStorage);
    }
    GxDxeDiagnosticsLog (Message);
  }
  AsciiSPrint (Message, sizeof (Message), "USB STORAGE BLK%u MEDIA%u FS%u OPEN%u FAIL%u LAST=%r",
    StorageSummary.UsbBlockIoCount, StorageSummary.UsbMediaPresentCount,
    StorageSummary.UsbSimpleFileSystemCount, StorageSummary.UsbVolumeOpenCount,
    StorageSummary.UsbVolumeOpenFailCount, StorageSummary.UsbLastOpenVolumeStatus);
  GxDxeDiagnosticsLog (Message);
  GxDxeDiagnosticsLog ("USB INVENTORY ENUMERATION RETURNED");

  if ((gST == NULL) || (gST->ConOut == NULL)) {
    DEBUG ((DEBUG_ERROR, "[GX-USB] VGA inventory page skipped ConOut=%p ConIn=%p; serial inventory completed\n",
      (gST != NULL) ? gST->ConOut : NULL, (gST != NULL) ? gST->ConIn : NULL));
    GxFreeUsbInventory (Inventory, RowCount);
    return;
  }

  RowsOnScreen = 25;
  Columns      = 80;
  if (gST->ConOut->Mode != NULL) {
    Status = gST->ConOut->QueryMode (gST->ConOut, gST->ConOut->Mode->Mode, &Columns, &RowsOnScreen);
    if (EFI_ERROR (Status)) {
      RowsOnScreen = 25;
    }
  }

  // Leave room for the inventory summary, row list, and navigation footer.
  RowsPerPage = (RowsOnScreen > 11) ? RowsOnScreen - 11 : 1;
  PageCount = (RowCount == 0) ? 1 : (RowCount + RowsPerPage - 1) / RowsPerPage;
  CurrentPage = 0;
  ContinueToShell = FALSE;
  GxClearUsbPageAndDelay ("device-inventory");

  DEBUG ((DEBUG_ERROR,
    "[GX-USB] VGA inventory begin unique_usb_device_paths=%u interfaces=%u rows=%u pages=%u "
    "timeout_per_page=%u seconds\n",
    UsbDevicePathCount, InterfaceCount, RowCount, PageCount, GX_USB_PAGE_SECONDS));
  GxDxeDiagnosticsLog ("USB INVENTORY VGA PAGE BEGIN");

  while (CurrentPage < PageCount) {
    FirstRow = CurrentPage * RowsPerPage;
    GxRenderUsbPage (Inventory, RowCount, FirstRow, RowsPerPage, ControllerCount,
                     ReadyControllerCount, InterfaceCount, UsbDevicePathCount,
                     UnclassifiedInterfaceCount, MassStorageInterfaceCount,
                     &StorageSummary, EnumerationStatus);
    AsciiSPrint (Message, sizeof (Message), "USB INVENTORY PAGE %u/%u", CurrentPage + 1, PageCount);
    GxDxeDiagnosticsLog (Message);

    if (!GxWaitForUsbPage (&Key, &KeyAvailable)) {
      CurrentPage++;
      continue;
    }

    DEBUG ((DEBUG_ERROR, "[GX-USB] inventory key unicode=0x%x scan=0x%x page=%u/%u\n",
      Key.UnicodeChar, Key.ScanCode, CurrentPage + 1, PageCount));
    if ((Key.UnicodeChar == CHAR_CARRIAGE_RETURN) || (Key.ScanCode == SCAN_ESC)) {
      ContinueToShell = TRUE;
      break;
    }
    if ((Key.ScanCode == SCAN_PAGE_DOWN) || (Key.ScanCode == SCAN_DOWN)) {
      if (CurrentPage + 1 < PageCount) {
        CurrentPage++;
      }
    } else if ((Key.ScanCode == SCAN_PAGE_UP) || (Key.ScanCode == SCAN_UP)) {
      if (CurrentPage > 0) {
        CurrentPage--;
      }
    } else {
      // Any other key advances this information page early.
      CurrentPage++;
    }
  }

  DEBUG ((DEBUG_ERROR, "[GX-USB] VGA inventory returned pages=%u/%u key_advance=%u\n",
    CurrentPage + ((CurrentPage < PageCount) ? 1 : 0), PageCount, ContinueToShell));
  GxDxeDiagnosticsLog ("USB INVENTORY VGA PAGE RETURNED");
  GxFreeUsbInventory (Inventory, RowCount);
}
