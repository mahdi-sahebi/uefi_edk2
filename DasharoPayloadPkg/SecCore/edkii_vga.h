#ifndef MN_EDK_II_VGA_H_
#define MN_EDK_II_VGA_H_

#include <stdarg.h> 

#include <Library/IoLib.h>
#include <Library/PrintLib.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/CpuLib.h>
#include <Library/PeCoffGetEntryPointLib.h>
#include <Library/PeCoffExtraActionLib.h>
#include <Library/DebugAgentLib.h>

// Init?
void edkii_vga_clear();
void edkii_vga_print(unsigned int line, const char *string);
void edkii_vga_hex_dump(const unsigned char *addr, unsigned int len, int start_row);
void edkii_vga_sprintf(unsigned int row, const char* format, ...);

#endif /* MN_EDK_II_VGA_H_ */
