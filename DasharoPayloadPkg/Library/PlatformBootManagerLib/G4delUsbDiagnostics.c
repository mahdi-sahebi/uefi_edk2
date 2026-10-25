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
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Protocol/DevicePath.h>
#include <Protocol/SimpleTextIn.h>
#include <Protocol/Usb2HostController.h>
#include <Protocol/UsbIo.h>

#define GX_USB_TEXT_CHARS      112
#define GX_USB_PAGE_SECONDS    10

typedef struct {
  CHAR16  Text[GX_USB_TEXT_CHARS];
} GX_USB_INVENTORY_ROW;

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
  OUT UINTN  *ControllerCount
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
      DEBUG ((DEBUG_ERROR, "[GX-USB] host-controller[%u] state status=%r state=%u\n", Index, Status, State));
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
  OUT EFI_STATUS            *EnumerationStatus
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
      "[GX-USB] interface[%u] handle=%p device_status=%r config_status=%r interface_status=%r "
      "vid=%04x pid=%04x usb=%04x release=%04x max_packet=%u configs=%u "
      "dev_class=%02x/%02x/%02x config_value=%u interfaces=%u attributes=%02x max_power=%u "
      "if=%u if_class=%02x/%02x/%02x "
      "mfg_status=%r mfg='%s' product_status=%r product='%s' serial_status=%r serial='%s' "
      "speed=not-exposed-by-EFI_USB_IO path='%s'\n",
      Index, Handles[Index], DeviceStatus, ConfigStatus, InterfaceStatus,
      DeviceDescriptor.IdVendor, DeviceDescriptor.IdProduct,
      DeviceDescriptor.BcdUSB, DeviceDescriptor.BcdDevice,
      DeviceDescriptor.MaxPacketSize0, DeviceDescriptor.NumConfigurations,
      DeviceDescriptor.DeviceClass, DeviceDescriptor.DeviceSubClass, DeviceDescriptor.DeviceProtocol,
      ConfigDescriptor.ConfigurationValue, ConfigDescriptor.NumInterfaces,
      ConfigDescriptor.Attributes, ConfigDescriptor.MaxPower,
      InterfaceDescriptor.InterfaceNumber, InterfaceDescriptor.InterfaceClass,
      InterfaceDescriptor.InterfaceSubClass, InterfaceDescriptor.InterfaceProtocol,
      ManufacturerStatus, Manufacturer, ProductStatus, Product, SerialStatus, Serial,
      (DevicePathText != NULL) ? DevicePathText : L"<unavailable>"));

    UnicodeSPrint ((*Rows)[Stored].Text, sizeof ((*Rows)[Stored].Text),
      L"%03u %04x:%04x if%02u %02x/%02x/%02x %s %s",
      Index,
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
GxRenderUsbPage (
  IN GX_USB_INVENTORY_ROW  *Rows,
  IN UINTN                 RowCount,
  IN UINTN                 FirstRow,
  IN UINTN                 RowsPerPage,
  IN UINTN                 ControllerCount,
  IN EFI_STATUS            EnumerationStatus
  )
{
  UINTN  Row;
  UINTN  LastRow;

  gST->ConOut->ClearScreen (gST->ConOut);
  Print (L"Gooxi G4DEL USB inventory: %u interface handles, %u host controllers\r\n",
         RowCount, ControllerCount);
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

  Print (L"\r\nRows %u-%u of %u. Up/Down or PgUp/PgDn scroll; Enter/Esc continues.\r\n",
         (RowCount == 0) ? 0 : FirstRow + 1, LastRow, RowCount);
  Print (L"Continuing to the EDK2 menu in %u seconds.\r\n", GX_USB_PAGE_SECONDS);
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
  UINTN                  EventCount;
  UINTN                  RowCount;
  UINTN                  FirstRow;
  UINTN                  RowsPerPage;
  UINTN                  Columns;
  UINTN                  RowsOnScreen;
  GX_USB_INVENTORY_ROW   *Inventory;
  EFI_INPUT_KEY          Key;

  if ((gST == NULL) || (gST->ConOut == NULL)) {
    DEBUG ((DEBUG_ERROR, "[GX-USB] inventory page skipped ConOut=%p ConIn=%p\n",
      (gST != NULL) ? gST->ConOut : NULL, (gST != NULL) ? gST->ConIn : NULL));
    return;
  }

  GxLogUsbHostControllers (&ControllerCount);
  RowCount = GxCollectUsbInventory (&Inventory, &EnumerationStatus);

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

  DEBUG ((DEBUG_ERROR, "[GX-USB] VGA inventory page begin rows=%u controllers=%u timeout=%u seconds\n",
    RowCount, ControllerCount, GX_USB_PAGE_SECONDS));
  GxRenderUsbPage (Inventory, RowCount, FirstRow, RowsPerPage, ControllerCount, EnumerationStatus);

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

    GxRenderUsbPage (Inventory, RowCount, FirstRow, RowsPerPage, ControllerCount, EnumerationStatus);
  }

  DEBUG ((DEBUG_ERROR, "[GX-USB] VGA inventory page return status=%r wait_index=%u\n", Status, WaitIndex));
  gBS->CloseEvent (TimerEvent);
  if (Inventory != NULL) {
    FreePool (Inventory);
  }
}
