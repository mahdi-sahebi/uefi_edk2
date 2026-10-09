#!/usr/bin/env python3
"""Run the actual boot guard functions with host stubs, without board hardware."""
import pathlib
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[2]


def function(path, name):
    text = (ROOT / path).read_text()
    start = text.index("\n" + name + " (") + 1
    opening = text.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


ftw = "MdeModulePkg/Universal/FaultTolerantWritePei/FaultTolerantWritePei.c"
timer = "DasharoPayloadPkg/Library/AcpiTimerLib/AcpiTimerLib.c"
dxe = "MdeModulePkg/Core/Dxe/DxeMain/DxeMain.c"
source = r'''
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#define IN
#define CONST const
#define VOID void
#define EFIAPI
#define BOOLEAN int
#define TRUE 1
#define FALSE 0
#define EFI_SUCCESS 0
#define DEBUG_ERROR 0x80000000
#define RETURN_STATUS int
#define EFI_PHYSICAL_ADDRESS uint64_t
#define UINT16 uint16_t
#define UINT8 uint8_t
#define CHAR8 char
#ifdef TEST_IA32
typedef uint32_t UINTN;
#define MAX_UINTN UINT32_MAX
#else
typedef uintptr_t UINTN;
#define MAX_UINTN UINTPTR_MAX
#endif
typedef struct { unsigned char bytes[16]; } EFI_GUID;
typedef struct { unsigned char bytes[32]; } EFI_FAULT_TOLERANT_WORKING_BLOCK_HEADER;
static EFI_GUID gEdkiiWorkingBlockSignatureGuid;
static uintptr_t scan_start, scan_end, match;
static unsigned probes;
static int CompareGuid(const EFI_GUID *a, const EFI_GUID *b) {
  (void)a;
  uintptr_t address = (uintptr_t)b;
  assert(address >= scan_start && address <= scan_end);
  assert(++probes <= 100);
  return address == match;
}
typedef struct { uintptr_t PmTimerRegBase; } ACPI_BOARD_INFO;
typedef struct { ACPI_BOARD_INFO info; } EFI_HOB_GUID_TYPE;
static EFI_HOB_GUID_TYPE hob = {{0x1808}};
static int have_list, have_hob, hob_lookups;
static EFI_GUID gUefiAcpiBoardInfoGuid;
static UINTN mPmTimerReg;
static void *GetHobList(void) { return have_list ? &hob : NULL; }
static EFI_HOB_GUID_TYPE *GetFirstGuidHob(const EFI_GUID *guid) {
  (void)guid;
  assert(have_list);
  ++hob_lookups;
  return have_hob ? &hob : NULL;
}
#define GET_GUID_HOB_DATA(h) (&(h)->info)
static uint16_t vga[25 * 80];
#define GX_DXE_VGA_FB ((uintptr_t)vga)
#define GX_DXE_VGA_COLUMNS 80
#define GX_DXE_VGA_ROWS 25
static BOOLEAN mGxDxeLibrariesReady;
static unsigned delays, messages;
#define DEBUG(args) (++messages)
static void MicroSecondDelay(UINTN delay) {
  assert(mGxDxeLibrariesReady && delay == 500000);
  ++delays;
}
'''
source += "\nBOOLEAN\n" + function(ftw, "FtwGeometryIsValid")
source += "\nEFI_PHYSICAL_ADDRESS\n" + function(ftw, "FtwFindSpareWorkspace")
source += "\nRETURN_STATUS\n" + function(timer, "AcpiTimerLibConstructor")
source += "\nVOID\n" + function(dxe, "GxDxeCheckpoint")
source += r'''
int main(void) {
  assert(FtwGeometryIsValid(0xff010000, 0x10000, 0xff020000, 0x20000));
  assert(!FtwGeometryIsValid(0, 0, 0, 0));
  assert(!FtwGeometryIsValid(0x1000, 0, 0x2000, 64));
  assert(!FtwGeometryIsValid(0x1000, 31, 0x2000, 64));
  assert(!FtwGeometryIsValid(0x1000, 64, 0x2000, 32));
  assert(!FtwGeometryIsValid(MAX_UINTN - 15, 32, 0x2000, 64));
  assert(!FtwGeometryIsValid(0x1000, 32, MAX_UINTN - 15, 64));
  assert(FtwGeometryIsValid(0x1000, 32, MAX_UINTN - 63, 64));
#ifdef TEST_IA32
  assert(!FtwGeometryIsValid(UINT64_C(0x100000000), 32, 0x2000, 64));
#endif
  scan_start = 0x1000; scan_end = 0x1040; match = 0; probes = 0;
  assert(FtwFindSpareWorkspace(scan_start, 96, 32) == 0 && probes == 5);
  match = scan_start; probes = 0;
  assert(FtwFindSpareWorkspace(scan_start, 96, 32) == match && probes == 5);
  match = scan_end; probes = 0;
  assert(FtwFindSpareWorkspace(scan_start, 96, 32) == match && probes == 1);
  scan_start = 8; scan_end = 8; match = 0; probes = 0;
  assert(FtwFindSpareWorkspace(scan_start, 32, 32) == 0 && probes == 1);
  scan_start = 8; scan_end = 15; probes = 0;
  assert(FtwFindSpareWorkspace(scan_start, 39, 32) == 0 && probes == 1);

  assert(AcpiTimerLibConstructor() == 0 && !mPmTimerReg && !hob_lookups);
  have_list = 1;
  assert(AcpiTimerLibConstructor() == 0 && !mPmTimerReg && hob_lookups == 1);
  have_hob = 1;
  assert(AcpiTimerLibConstructor() == 0 && mPmTimerReg == 0x1808);
#ifndef TEST_IA32
  GxDxeCheckpoint(0, "entry");
  GxDxeCheckpoint(3, "before constructors");
  assert(delays == 0 && messages == 2);
  mGxDxeLibrariesReady = TRUE;
  GxDxeCheckpoint(8, "libraries ready");
  assert(delays == 1 && messages == 3);
  GxDxeCheckpoint(25, "wrap");
  assert(vga[0] == (0x0f00 | 'w'));
#endif
  puts("PASS: FTW geometry/bounded scan, deferred ACPI HOB, DXE checkpoint ordering");
  return 0;
}
'''

with tempfile.TemporaryDirectory(prefix="gx-boot-test-") as temp:
    path = pathlib.Path(temp)
    (path / "test.c").write_text(source)
    for arch, flags in (("x64", []), ("ia32-geometry", ["-DTEST_IA32"])):
        subprocess.run(["cc", "-std=c11", "-fsanitize=undefined",
                        "-fno-sanitize-recover=all", "-Wno-int-to-pointer-cast",
                        *flags, str(path / "test.c"), "-o", str(path / arch)], check=True)
        subprocess.run([str(path / arch)], check=True, timeout=10)
