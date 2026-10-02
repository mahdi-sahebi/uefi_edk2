/** @file
  x64-specific functionality for DxeLoad.

Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "DxeIpl.h"
#include "X64/VirtualMemory.h"

VOID
HandOffToDxeCore (
  IN EFI_PHYSICAL_ADDRESS  DxeCoreEntryPoint,
  IN EFI_PEI_HOB_POINTERS HobList
  )
{
  VOID                             *BaseOfStack;
  VOID                             *TopOfStack;
  EFI_STATUS                       Status;
  UINTN                            PageTables;
  UINT32                           Index;
  EFI_VECTOR_HANDOFF_INFO          *VectorInfo;
  EFI_PEI_VECTOR_HANDOFF_INFO_PPI  *VectorHandoffInfoPpi;
  VOID                             *GhcbBase;
  UINTN                            GhcbSize;

  DEBUG ((DEBUG_WARN, "[GX] module=DxeIpl event=handoff-start entry=%p hob=%p\\n", (VOID *)(UINTN)DxeCoreEntryPoint, HobList.Raw));
  if (IsNullDetectionEnabled ()) {
    ClearFirst4KPage (HobList.Raw);
    BuildMemoryAllocationHob (0, EFI_PAGES_TO_SIZE (1), EfiBootServicesData);
  }

  Status = PeiServicesLocatePpi (&gEfiVectorHandoffInfoPpiGuid, 0, NULL, (VOID **)&VectorHandoffInfoPpi);
  if (Status == EFI_SUCCESS) {
    VectorInfo = VectorHandoffInfoPpi->Info;
    Index = 1;
    while (VectorInfo->Attribute != EFI_VECTOR_HANDOFF_LAST_ENTRY) {
      VectorInfo++;
      Index++;
    }
    BuildGuidDataHob (&gEfiVectorHandoffInfoPpiGuid, VectorHandoffInfoPpi->Info, sizeof (EFI_VECTOR_HANDOFF_INFO) * Index);
  }

  BaseOfStack = AllocatePages (EFI_SIZE_TO_PAGES (STACK_SIZE));
  ASSERT (BaseOfStack != NULL);
  if (BaseOfStack == NULL) {
    DEBUG ((DEBUG_ERROR, "[GX] module=DxeIpl event=stack-allocation status=error\\n"));
    return;
  }
  TopOfStack = (VOID *)((UINTN)BaseOfStack + EFI_SIZE_TO_PAGES (STACK_SIZE) * EFI_PAGE_SIZE - CPU_STACK_ALIGNMENT);
  TopOfStack = ALIGN_POINTER (TopOfStack, CPU_STACK_ALIGNMENT);

  GhcbBase = (VOID *)PcdGet64 (PcdGhcbBase);
  GhcbSize = PcdGet64 (PcdGhcbSize);
  PageTables = 0;
  if (FeaturePcdGet (PcdDxeIplBuildPageTables)) {
    PageTables = CreateIdentityMappingPageTables ((EFI_PHYSICAL_ADDRESS)(UINTN)BaseOfStack, STACK_SIZE, (EFI_PHYSICAL_ADDRESS)(UINTN)GhcbBase, GhcbSize);
  } else {
    ASSERT (PcdGetBool (PcdSetNxForStack) == FALSE);
    ASSERT (PcdGetBool (PcdCpuStackGuard) == FALSE);
  }

  Status = PeiServicesInstallPpi (&gEndOfPeiSignalPpi);
  ASSERT_EFI_ERROR (Status);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "[GX] module=DxeIpl event=end-of-pei status=%r\\n", Status));
    return;
  }
  if (FeaturePcdGet (PcdDxeIplBuildPageTables)) {
    AsmWriteCr3 (PageTables);
  }
  UpdateStackHob ((EFI_PHYSICAL_ADDRESS)(UINTN)BaseOfStack, STACK_SIZE);
  DEBUG ((DEBUG_WARN, "[GX] module=DxeIpl event=handoff-ready stack=%p pagetables=%p\\n", BaseOfStack, (VOID *)PageTables));
  SwitchStack ((SWITCH_STACK_ENTRY_POINT)(UINTN)DxeCoreEntryPoint, HobList.Raw, NULL, TopOfStack);
}
