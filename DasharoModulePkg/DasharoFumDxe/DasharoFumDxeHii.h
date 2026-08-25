/** @file
The Dasharo Firmware Update Mode setup menu HII definitions

Copyright (c) 2026, 3mdeb Sp. z o.o. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause

**/

#ifndef _DASHARO_FUM_DXE_HII_H_
#define _DASHARO_FUM_DXE_HII_H_

#define DASHARO_FUM_FORMSET_GUID \
  { 0x92228f92, 0x53f9, 0x4298, { 0xa8, 0x98, 0xd4, 0xc8, 0x56, 0xd9, 0xf7, 0x53 } }

#define DASHARO_FUM_FORM_ID             0x0001

//
// Question IDs are used in VFR file to let the code in
// DasharoFumCallback() know what form element caused
// invocation of the callback.
//

#define FIRMWARE_UPDATE_MODE_QUESTION_ID  0x0001

#endif
