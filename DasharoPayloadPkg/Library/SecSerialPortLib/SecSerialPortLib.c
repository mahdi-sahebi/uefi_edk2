/** @file
  Minimal transmit-only serial port for SEC, before mutable PCDs are available.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Base.h>
#include <Coreboot.h>
#include <Library/IoLib.h>
#include <Library/SerialPortLib.h>

#define SEC_UART_DEFAULT_BASE  0x3F8
#define SEC_UART_TX_READY      BIT5
#define SEC_UART_WAIT_LIMIT    1000000

STATIC UINTN    mSecUartBase   = SEC_UART_DEFAULT_BASE;
STATIC UINT32   mSecUartStride = 1;
STATIC BOOLEAN  mSecUartMmio   = FALSE;

STATIC
BOOLEAN
ValidCbHeader (
  IN CONST struct cb_header  *Header
  )
{
  return (BOOLEAN)(
    (Header != NULL) &&
    (Header->signature == CB_HEADER_SIGNATURE) &&
    (Header->header_bytes >= sizeof (*Header)) &&
    (Header->header_bytes <= 4096) &&
    (Header->table_bytes <= 1024 * 1024) &&
    (Header->table_entries <= Header->table_bytes / sizeof (struct cb_record))
    );
}

STATIC
CONST struct cb_record *
FindRecord (
  IN CONST struct cb_header  *Header,
  IN UINT32                  Tag
  )
{
  CONST UINT8             *Current;
  CONST UINT8             *End;
  CONST struct cb_record  *Record;
  UINT32                  Index;

  Current = (CONST UINT8 *)Header + Header->header_bytes;
  End     = Current + Header->table_bytes;
  for (Index = 0; Index < Header->table_entries; Index++) {
    if ((UINTN)(End - Current) < sizeof (*Record)) {
      break;
    }

    Record = (CONST struct cb_record *)Current;
    if ((Record->size < sizeof (*Record)) || (Record->size > (UINTN)(End - Current))) {
      break;
    }

    if (Record->tag == Tag) {
      return Record;
    }

    Current += Record->size;
  }

  return NULL;
}

VOID
EFIAPI
SecSerialPortConfigure (
  IN UINT32  BootloaderParameter
  )
{
  CONST struct cb_header   *Header;
  CONST struct cb_forward  *Forward;
  CONST struct cb_serial   *Serial;
  UINTN                    Address;

  Header = (CONST struct cb_header *)(UINTN)BootloaderParameter;
  if (!ValidCbHeader (Header)) {
    Header = NULL;
    for (Address = 0x10; Address < 0x1000; Address += 0x10) {
      if (ValidCbHeader ((CONST struct cb_header *)Address)) {
        Header = (CONST struct cb_header *)Address;
        break;
      }
    }
  }

  if (Header == NULL) {
    return;
  }

  Forward = (CONST struct cb_forward *)FindRecord (Header, CB_TAG_FORWARD);
  if ((Forward != NULL) && (Forward->size >= sizeof (*Forward)) &&
      (Forward->forward <= MAX_UINT32) &&
      ValidCbHeader ((CONST struct cb_header *)(UINTN)Forward->forward))
  {
    Header = (CONST struct cb_header *)(UINTN)Forward->forward;
  }

  Serial = (CONST struct cb_serial *)FindRecord (Header, CB_TAG_SERIAL);
  if ((Serial == NULL) || (Serial->size < sizeof (*Serial)) ||
      (Serial->baseaddr == 0) || (Serial->regwidth == 0))
  {
    return;
  }

  if ((Serial->type != CB_SERIAL_TYPE_IO_MAPPED) &&
      (Serial->type != CB_SERIAL_TYPE_MEMORY_MAPPED))
  {
    return;
  }

  mSecUartBase   = Serial->baseaddr;
  mSecUartStride = Serial->regwidth;
  mSecUartMmio   = (BOOLEAN)(Serial->type == CB_SERIAL_TYPE_MEMORY_MAPPED);
}

STATIC
UINT8
ReadRegister (
  IN UINTN  Offset
  )
{
  UINTN  Address;

  Address = mSecUartBase + Offset * mSecUartStride;
  return mSecUartMmio ? MmioRead8 (Address) : IoRead8 (Address);
}

STATIC
VOID
WriteRegister (
  IN UINTN  Offset,
  IN UINT8  Value
  )
{
  UINTN  Address;

  Address = mSecUartBase + Offset * mSecUartStride;
  if (mSecUartMmio) {
    MmioWrite8 (Address, Value);
  } else {
    IoWrite8 (Address, Value);
  }
}

RETURN_STATUS
EFIAPI
SerialPortInitialize (
  VOID
  )
{
  // Coreboot has already configured the baud rate and line control.
  return RETURN_SUCCESS;
}

UINTN
EFIAPI
SerialPortWrite (
  IN UINT8  *Buffer,
  IN UINTN  NumberOfBytes
  )
{
  UINTN  Index;
  UINTN  Retry;

  if (Buffer == NULL) {
    return 0;
  }

  for (Index = 0; Index < NumberOfBytes; Index++) {
    for (Retry = 0; Retry < SEC_UART_WAIT_LIMIT; Retry++) {
      if ((ReadRegister (5) & SEC_UART_TX_READY) != 0) {
        break;
      }
    }

    if (Retry == SEC_UART_WAIT_LIMIT) {
      break;
    }

    WriteRegister (0, Buffer[Index]);
  }

  return Index;
}

UINTN EFIAPI SerialPortRead (OUT UINT8 *Buffer, IN UINTN NumberOfBytes) { return 0; }
BOOLEAN EFIAPI SerialPortPoll (VOID) { return FALSE; }
RETURN_STATUS EFIAPI SerialPortSetControl (IN UINT32 Control) { return RETURN_UNSUPPORTED; }
RETURN_STATUS EFIAPI SerialPortGetControl (OUT UINT32 *Control) { return RETURN_UNSUPPORTED; }
RETURN_STATUS EFIAPI SerialPortSetAttributes (
  IN OUT UINT64 *BaudRate,
  IN OUT UINT32 *ReceiveFifoDepth,
  IN OUT UINT32 *Timeout,
  IN OUT EFI_PARITY_TYPE *Parity,
  IN OUT UINT8 *DataBits,
  IN OUT EFI_STOP_BITS_TYPE *StopBits
  )
{
  return RETURN_UNSUPPORTED;
}
