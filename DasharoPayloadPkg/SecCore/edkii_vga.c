
#include "edkii_vga.h"


/////////////////////////////////////////////////////

#include <stdarg.h> 

#include <Library/IoLib.h>
#include <Library/PrintLib.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PcdLib.h>
// #include <Library/CpuLib.h>
// #include <Library/PeCoffGetEntryPointLib.h>
// #include <Library/PeCoffExtraActionLib.h>
#include <Library/DebugAgentLib.h>

#define mde_2__VGA_FB 0xB8000
#define mde_2__VGA_COLUMNS 80

char mde_2_g_buffer[80];

void edkii_vga_write_at_offset(unsigned int line, unsigned int offset, const char *string)
{
	if (!string)
		return;

	unsigned short *p = (unsigned short *)mde_2__VGA_FB + (mde_2__VGA_COLUMNS * line) + offset;
	unsigned int i, len = AsciiStrLen(string);

	for (i = 0; i < (mde_2__VGA_COLUMNS - offset); i++) {
		if (i < len)
			p[i] = 0x0F00 | (unsigned char)string[i];
		else
			p[i] = 0x0F00;
	}
}


void edkii_vga_print(unsigned int line, const char *string)
{
	edkii_vga_write_at_offset(line, 0, string);
}

void edkii_vga_sprintf(
  unsigned int row,
  const char* format,
  ...)
{
  VA_LIST  marker;
  
  VA_START (marker, format);
  AsciiVSPrint(mde_2_g_buffer, sizeof(mde_2_g_buffer), format, marker);
  VA_END (marker);
  DEBUG ((DEBUG_ERROR, "[GX-VGA] row=%u %a\n", row, mde_2_g_buffer));
  edkii_vga_print (row, mde_2_g_buffer);
}

void edkii_vga_clear()
{
  edkii_vga_print(0, "                                                                                                    ");
  edkii_vga_print(1, "                                                                                                    ");
  edkii_vga_print(2, "                                                                                                    ");
  edkii_vga_print(3, "                                                                                                    ");
  edkii_vga_print(4, "                                                                                                    ");
  edkii_vga_print(5, "                                                                                                    ");
  edkii_vga_print(6, "                                                                                                    ");
  edkii_vga_print(7, "                                                                                                    ");
  edkii_vga_print(8, "                                                                                                    ");
  edkii_vga_print(9, "                                                                                                    ");
  edkii_vga_print(10, "                                                                                                    ");
  edkii_vga_print(11, "                                                                                                    ");
  edkii_vga_print(12, "                                                                                                    ");
  edkii_vga_print(13, "                                                                                                    ");
  edkii_vga_print(14, "                                                                                                    ");
  edkii_vga_print(15, "                                                                                                    ");
  edkii_vga_print(16, "                                                                                                    ");
  edkii_vga_print(17, "                                                                                                    ");
  edkii_vga_print(18, "                                                                                                    ");
  edkii_vga_print(19, "                                                                                                    ");
  edkii_vga_print(20, "                                                                                                    ");
  edkii_vga_print(21, "                                                                                                    ");
  edkii_vga_print(22, "                                                                                                    ");
  edkii_vga_print(23, "                                                                                                    ");
  edkii_vga_print(24, "                                                                                                    ");
}

void edkii_vga_hex_dump(const unsigned char *addr, unsigned int len, int start_row) 
{
    unsigned int i;
    
    for (i = 0; i < len; i += 16) {
        unsigned int j;
        int row = start_row + (i / 16);
        int offset_pos = 0;
        
        // Write offset character by character
        unsigned long ptr_val = (unsigned long)(addr + i);
        for (j = 28; j > 0; j -= 4) {
            char nibble = (ptr_val >> j) & 0x0F;
            char c = (nibble < 10) ? ('0' + nibble) : ('A' + nibble - 10);
            char buf[2] = {c, '\0'};
            edkii_vga_write_at_offset(row, offset_pos++, buf);
        }
        {
            char last_nibble = ptr_val & 0x0F;
            char c = (last_nibble < 10) ? ('0' + last_nibble) : ('A' + last_nibble - 10);
            char buf[2] = {c, '\0'};
            edkii_vga_write_at_offset(row, offset_pos++, buf);
        }
        edkii_vga_write_at_offset(row, offset_pos++, ": ");
        
        // Write hex bytes
        for (j = 0; j < 16 && (i + j < len); j++) {
            unsigned char byte = addr[i + j];
            // Write high nibble
            char high = (byte >> 4) & 0x0F;
            char c1 = (high < 10) ? ('0' + high) : ('A' + high - 10);
            char buf1[2] = {c1, '\0'};
            edkii_vga_write_at_offset(row, offset_pos++, buf1);
            // Write low nibble
            char low = byte & 0x0F;
            char c2 = (low < 10) ? ('0' + low) : ('A' + low - 10);
            char buf2[2] = {c2, '\0'};
            edkii_vga_write_at_offset(row, offset_pos++, buf2);
            // Write space
            edkii_vga_write_at_offset(row, offset_pos++, " ");
        }
        
        // Pad remaining hex spaces
        for (; j < 16; j++) {
            edkii_vga_write_at_offset(row, offset_pos++, "   ");
        }
        
        // Write ASCII representation
        edkii_vga_write_at_offset(row, offset_pos++, "  ");
        
        for (j = 0; j < 16 && (i + j < len); j++) {
            unsigned char byte = addr[i + j];
            char c = (byte >= 0x20 && byte <= 0x7e) ? (char)byte : '.';
            char buf[2] = {c, '\0'};
            edkii_vga_write_at_offset(row, offset_pos + j, buf);
        }
    }
}



/////////////////////////////////////////////////////
