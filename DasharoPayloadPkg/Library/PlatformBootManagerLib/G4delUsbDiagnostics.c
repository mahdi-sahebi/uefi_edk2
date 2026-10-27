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
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/SimpleTextIn.h>
#include <Protocol/Usb2HostController.h>
#include <Protocol/UsbIo.h>

#define GX_USB_TEXT_CHARS      112
#define GX_USB_PAGE_SECONDS    10

typedef struct {
  CHAR16  Text[GX_USB_TEXT_CHARS];
} GX_USB_INVENTORY_ROW;

typedef struct {
  UINTN  BlockIoCount;
  UINTN  UsbBlockIoCount;
  UINTN  UsbRemovableCount;
  UINTN  UsbMediaPresentCount;
  UINTN  SimpleFileSystemCount;
  UINTN  UsbSimpleFileSystemCount;
  UINTN  UsbVolumeOpenCount;
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
  EFI_HANDLE                  *Handles;
  UINTN                       Count;
  UINTN                       Index;
  UINTN                       Stored;
  EFI_USB_IO_PROTOCOL         *UsbIo;
  EFI_USB_DEVICE_DESCRIPTOR   DeviceDescriptor;
  EFI_USB_CONFIG_DESCRIPTOR   ConfigDescriptor;
  EFI_USB_INTERFACE_DESCRIPTOR InterfaceDescriptor;
  EFI_DEVICE_PATH_PROTOCOL    *DevicePath;
  CHAR16                      *DevicePathText;
  CHAR16                      Manufacturer[40];
  CHAR16                      Product[40];
  CHAR16                      Serial[32];

  Handles = NULL;
  Count   = 0;
  Stored  = 0;
  *Rows   = NULL;
  *MassStorageInterfaceCount = 0;
  *EnumerationStatus = EFI_NOT_FOUND;
  Status  = gBS->LocateHandleBuffer (ByProtocol, &gEfiUsbIoProtocolGuid, NULL, &Count, &Handles);
  *EnumerationStatus = Status;
  DEBUG ((DEBUG_ERROR, "[GX-USB] device-interface enumeration status=%r handles=%u\n", Status, Count));
  if (EFI_ERROR (Status)) {
    return 0;
  }

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

    Status = gBS->HandleProtocol (Handles[Index], &gEfiDevicePathProtocolGuid, (VOID **)&DevicePath);
    if (!EFI_ERROR (Status) && (DevicePath != NULL)) {
      DevicePathText = ConvertDevicePathToText (DevicePath, FALSE, FALSE);
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
      GxDevicePathHasUsbNode (DevicePath),
      (DevicePathText != NULL) ? DevicePathText : L"<unavailable>"));

    UnicodeSPrint ((*Rows)[Stored].Text, sizeof ((*Rows)[Stored].Text),
      L"%03u %s %04x:%04x if%02u %02x/%02x/%02x %s %s",
      Index,
      EFI_ERROR (DeviceStatus) ? L"ERROR" : L"READY",
      DeviceDescriptor.IdVendor,
      DeviceDescriptor.IdProduct,
      InterfaceDescriptor.InterfaceNumber,
      InterfaceDescriptor.InterfaceClass,
      InterfaceDescriptor.InterfaceSubClass,
      InterfaceDescriptor.InterfaceProtocol,
      Manufacturer,
      Product);
    Stored++;

    if (DevicePathText != NULL) {
      FreePool (DevicePathText);
    }
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
        }
        Root->Close (Root);
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
    "simple_fs=%u usb_simple_fs=%u usb_volume_open=%u\n",
    Summary->BlockIoCount, Summary->UsbBlockIoCount, Summary->UsbRemovableCount,
    Summary->UsbMediaPresentCount, Summary->SimpleFileSystemCount,
    Summary->UsbSimpleFileSystemCount, Summary->UsbVolumeOpenCount));
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
  IN UINTN                 MassStorageInterfaceCount,
  IN GX_USB_STORAGE_SUMMARY *StorageSummary,
  IN EFI_STATUS            EnumerationStatus
  )
{
  UINTN  Row;
  UINTN  LastRow;

  gST->ConOut->ClearScreen (gST->ConOut);
  Print (L"Gooxi G4DEL USB inventory: %u device interfaces; host controllers READY %u/%u\r\n",
         RowCount, ReadyControllerCount, ControllerCount);
  Print (L"Mass-storage interfaces: %u; USB BlockIo: %u (%u removable, %u media present)\r\n",
         MassStorageInterfaceCount, StorageSummary->UsbBlockIoCount,
         StorageSummary->UsbRemovableCount, StorageSummary->UsbMediaPresentCount);
  Print (L"USB filesystems: %u; root opened: %u\r\n",
         StorageSummary->UsbSimpleFileSystemCount, StorageSummary->UsbVolumeOpenCount);
  Print (L"EFI_USB_IO READY means the USB bus driver enumerated the device.\r\n");
  Print (L"Per-device negotiated speed is not exposed by EFI_USB_IO_PROTOCOL.\r\n\r\n");

  LastRow = FirstRow + RowsPerPage;
  if (LastRow > RowCount) {
    LastRow = RowCount;
  }

  if (RowCount == 0 && EnumerationStatus == EFI_NOT_FOUND) {
    Print (L"No USB device interfaces were enumerated.\r\n");
  } else if (RowCount == 0) {
    Print (L"USB inventory unavailable: %r\r\n", EnumerationStatus);
  } else {
    for (Row = FirstRow; Row < LastRow; Row++) {
      Print (L"%s\r\n", Rows[Row].Text);
    }
  }

  Print (L"\r\nRows %u-%u of %u. Up/Down or PgUp/PgDn scroll; Enter/Esc continues to UEFI Shell.\r\n",
         (RowCount == 0) ? 0 : FirstRow + 1, LastRow, RowCount);
  Print (L"Continuing to UEFI Shell in %u seconds.\r\n", GX_USB_PAGE_SECONDS);
}

VOID
GxDisplayUsbInventory (
  VOID
  )
{
  EFI_STATUS             Status;
  EFI_STATUS             EnumerationStatus;
  EFI_EVENT              TimerEvent;
  EFI_EVENT              Events[2];
  UINTN                  WaitIndex;
  UINTN                  ControllerCount;
  UINTN                  ReadyControllerCount;
  UINTN                  EventCount;
  UINTN                  RowCount;
  UINTN                  MassStorageInterfaceCount;
  UINTN                  FirstRow;
  UINTN                  RowsPerPage;
  UINTN                  Columns;
  UINTN                  RowsOnScreen;
  GX_USB_INVENTORY_ROW   *Inventory;
  GX_USB_STORAGE_SUMMARY StorageSummary;
  EFI_INPUT_KEY          Key;

  GxDxeDiagnosticsLog ("USB INVENTORY ENUMERATION BEGIN");
  DEBUG ((DEBUG_ERROR, "[GX-USB] inventory enumeration begin\n"));
  GxLogUsbHostControllers (&ControllerCount, &ReadyControllerCount);
  RowCount = GxCollectUsbInventory (&Inventory, &EnumerationStatus, &MassStorageInterfaceCount);
  GxLogUsbStorage (&StorageSummary);
  DEBUG ((DEBUG_ERROR,
    "[GX-USB] readiness summary host_controllers=%u/%u usb_io_devices=%u mass_storage_interfaces=%u enumeration_status=%r\n",
    ReadyControllerCount, ControllerCount, RowCount, MassStorageInterfaceCount, EnumerationStatus));
  GxDxeDiagnosticsLog ("USB INVENTORY ENUMERATION RETURNED");

  if ((gST == NULL) || (gST->ConOut == NULL)) {
    DEBUG ((DEBUG_ERROR, "[GX-USB] VGA inventory page skipped ConOut=%p ConIn=%p; serial inventory completed\n",
      (gST != NULL) ? gST->ConOut : NULL, (gST != NULL) ? gST->ConIn : NULL));
    if (Inventory != NULL) {
      FreePool (Inventory);
    }
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

  RowsPerPage = (RowsOnScreen > 6) ? RowsOnScreen - 6 : 1;
  FirstRow    = 0;
  TimerEvent  = NULL;
  Status      = gBS->CreateEvent (EVT_TIMER, TPL_CALLBACK, NULL, NULL, &TimerEvent);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "[GX-USB] inventory timer creation failed status=%r\n", Status));
    if (Inventory != NULL) {
      FreePool (Inventory);
    }
    return;
  }

  Status = gBS->SetTimer (TimerEvent, TimerRelative, GX_USB_PAGE_SECONDS * 10 * 1000 * 1000);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "[GX-USB] inventory timer start failed status=%r\n", Status));
    gBS->CloseEvent (TimerEvent);
    if (Inventory != NULL) {
      FreePool (Inventory);
    }
    return;
  }

  DEBUG ((DEBUG_ERROR,
    "[GX-USB] VGA inventory page begin rows=%u controllers=%u mass_storage=%u timeout=%u seconds\n",
    RowCount, ControllerCount, MassStorageInterfaceCount, GX_USB_PAGE_SECONDS));
  GxRenderUsbPage (Inventory, RowCount, FirstRow, RowsPerPage, ControllerCount,
                   ReadyControllerCount, MassStorageInterfaceCount, &StorageSummary, EnumerationStatus);

  Events[1] = TimerEvent;
  if ((gST->ConIn == NULL) || (gST->ConIn->WaitForKey == NULL)) {
    DEBUG ((DEBUG_ERROR, "[GX-USB] inventory page has no key input; wait for timeout\n"));
    Events[0] = TimerEvent;
    EventCount = 1;
  } else {
    Events[0] = gST->ConIn->WaitForKey;
    EventCount = ARRAY_SIZE (Events);
  }
  while (TRUE) {
    Status = gBS->WaitForEvent (EventCount, Events, &WaitIndex);
    if (EFI_ERROR (Status) || (WaitIndex == 1) || (Events[0] == TimerEvent)) {
      break;
    }

    if (gST->ConIn == NULL) {
      break;
    }

    Status = gST->ConIn->ReadKeyStroke (gST->ConIn, &Key);
    if (EFI_ERROR (Status)) {
      continue;
    }

    if (Key.UnicodeChar == CHAR_CARRIAGE_RETURN || Key.ScanCode == SCAN_ESC) {
      break;
    } else if ((Key.ScanCode == SCAN_UP) && (FirstRow > 0)) {
      FirstRow--;
    } else if ((Key.ScanCode == SCAN_DOWN) && (FirstRow + RowsPerPage < RowCount)) {
      FirstRow++;
    } else if ((Key.ScanCode == SCAN_PAGE_UP) && (FirstRow > 0)) {
      FirstRow = (FirstRow > RowsPerPage) ? FirstRow - RowsPerPage : 0;
    } else if ((Key.ScanCode == SCAN_PAGE_DOWN) && (FirstRow + RowsPerPage < RowCount)) {
      FirstRow += RowsPerPage;
      if (FirstRow + RowsPerPage > RowCount) {
        FirstRow = RowCount - RowsPerPage;
      }
    } else {
      continue;
    }

    GxRenderUsbPage (Inventory, RowCount, FirstRow, RowsPerPage, ControllerCount,
                     ReadyControllerCount, MassStorageInterfaceCount, &StorageSummary, EnumerationStatus);
  }

  DEBUG ((DEBUG_ERROR, "[GX-USB] VGA inventory page return status=%r wait_index=%u\n", Status, WaitIndex));
  gBS->CloseEvent (TimerEvent);
  if (Inventory != NULL) {
    FreePool (Inventory);
  }
}
