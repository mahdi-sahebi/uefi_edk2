
#include "edkii_vga.h"
#include <Coreboot.h>


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

typedef struct {
    volatile UINT32 *Base;
    UINT32 Width;
    UINT32 Height;
    UINT32 PixelsPerScanLine;
    UINT64 Size;
} SEC_FRAMEBUFFER;

STATIC SEC_FRAMEBUFFER mSecFramebuffer;

STATIC
BOOLEAN
SecValidCorebootHeader (
  IN CONST struct cb_header *Header
  )
{
  return (BOOLEAN)(Header != NULL && Header->signature == CB_HEADER_SIGNATURE &&
    Header->header_bytes >= sizeof (*Header) && Header->header_bytes <= 4096 &&
    Header->table_bytes <= 1024 * 1024 &&
    Header->table_entries <= Header->table_bytes / sizeof (struct cb_record));
}

void
edkii_vga_configure_from_coreboot (
  unsigned int BootloaderParameter
  )
{
  CONST struct cb_header       *Header;
  CONST struct cb_forward      *Forward;
  CONST struct cb_record       *Record;
  CONST struct cb_framebuffer  *Framebuffer;
  CONST UINT8                  *Current;
  CONST UINT8                  *End;
  UINT32                       Index;

  Header = (CONST struct cb_header *)(UINTN)BootloaderParameter;
  if (!SecValidCorebootHeader (Header)) {
    Header = NULL;
    for (Index = 0x10; Index < 0x1000; Index += 0x10) {
      if (SecValidCorebootHeader ((CONST struct cb_header *)(UINTN)Index)) {
        Header = (CONST struct cb_header *)(UINTN)Index;
        break;
      }
    }
  }

  if (Header == NULL) {
    DEBUG ((DEBUG_ERROR, "[GX-SEC] framebuffer coreboot table not found\n"));
    return;
  }

  Forward = NULL;
  Current = (CONST UINT8 *)Header + Header->header_bytes;
  End = Current + Header->table_bytes;
  for (Index = 0; Index < Header->table_entries && Current + sizeof (*Record) <= End; Index++) {
    Record = (CONST struct cb_record *)Current;
    if (Record->size < sizeof (*Record) || Record->size > (UINTN)(End - Current)) {
      break;
    }
    if (Record->tag == CB_TAG_FORWARD && Record->size >= sizeof (*Forward)) {
      Forward = (CONST struct cb_forward *)Record;
    }
    Current += Record->size;
  }

  if ((Forward != NULL) && (Forward->forward <= MAX_UINT32) &&
      SecValidCorebootHeader ((CONST struct cb_header *)(UINTN)Forward->forward))
  {
    Header = (CONST struct cb_header *)(UINTN)Forward->forward;
  }

  Current = (CONST UINT8 *)Header + Header->header_bytes;
  End = Current + Header->table_bytes;
  for (Index = 0; Index < Header->table_entries && Current + sizeof (*Record) <= End; Index++) {
    Record = (CONST struct cb_record *)Current;
    if (Record->size < sizeof (*Record) || Record->size > (UINTN)(End - Current)) {
      break;
    }
    if ((Record->tag == CB_TAG_FRAMEBUFFER) && (Record->size >= sizeof (*Framebuffer))) {
      Framebuffer = (CONST struct cb_framebuffer *)Record;
      if ((Framebuffer->physical_address != 0) && (Framebuffer->x_resolution >= 640) &&
          (Framebuffer->y_resolution >= 16) && (Framebuffer->bits_per_pixel == 32) &&
          (Framebuffer->bytes_per_line >= Framebuffer->x_resolution * sizeof (UINT32)) &&
          (Framebuffer->bytes_per_line % sizeof (UINT32) == 0))
      {
        mSecFramebuffer.Base = (volatile UINT32 *)(UINTN)Framebuffer->physical_address;
        mSecFramebuffer.Width = Framebuffer->x_resolution;
        mSecFramebuffer.Height = Framebuffer->y_resolution;
        mSecFramebuffer.PixelsPerScanLine = Framebuffer->bytes_per_line / sizeof (UINT32);
        mSecFramebuffer.Size = (UINT64)Framebuffer->bytes_per_line * Framebuffer->y_resolution;
        DEBUG ((DEBUG_ERROR, "[GX-SEC] linear framebuffer base=0x%lx size=0x%lx mode=%ux%u pitch=%u\n",
          Framebuffer->physical_address, mSecFramebuffer.Size, mSecFramebuffer.Width,
          mSecFramebuffer.Height, mSecFramebuffer.PixelsPerScanLine));
      }
      return;
    }
    Current += Record->size;
  }

  DEBUG ((DEBUG_ERROR, "[GX-SEC] framebuffer record not found in coreboot table\n"));
}

STATIC
UINT8
SecGlyphColumn (
  IN CHAR8 Character,
  IN UINTN Column
  )
{
  STATIC CONST UINT8 Font[][5] = {
    { 0x3E, 0x51, 0x49, 0x45, 0x3E }, { 0x00, 0x42, 0x7F, 0x40, 0x00 },
    { 0x42, 0x61, 0x51, 0x49, 0x46 }, { 0x21, 0x41, 0x45, 0x4B, 0x31 },
    { 0x18, 0x14, 0x12, 0x7F, 0x10 }, { 0x27, 0x45, 0x45, 0x45, 0x39 },
    { 0x3C, 0x4A, 0x49, 0x49, 0x30 }, { 0x01, 0x71, 0x09, 0x05, 0x03 },
    { 0x36, 0x49, 0x49, 0x49, 0x36 }, { 0x06, 0x49, 0x49, 0x29, 0x1E },
    { 0x7E, 0x11, 0x11, 0x11, 0x7E }, { 0x7F, 0x49, 0x49, 0x49, 0x36 },
    { 0x3E, 0x41, 0x41, 0x41, 0x22 }, { 0x7F, 0x41, 0x41, 0x22, 0x1C },
    { 0x7F, 0x49, 0x49, 0x49, 0x41 }, { 0x7F, 0x09, 0x09, 0x09, 0x01 },
    { 0x3E, 0x41, 0x49, 0x49, 0x7A }, { 0x7F, 0x08, 0x08, 0x08, 0x7F },
    { 0x00, 0x41, 0x7F, 0x41, 0x00 }, { 0x20, 0x40, 0x41, 0x3F, 0x01 },
    { 0x7F, 0x08, 0x14, 0x22, 0x41 }, { 0x7F, 0x40, 0x40, 0x40, 0x40 },
    { 0x7F, 0x02, 0x0C, 0x02, 0x7F }, { 0x7F, 0x04, 0x08, 0x10, 0x7F },
    { 0x3E, 0x41, 0x41, 0x41, 0x3E }, { 0x7F, 0x09, 0x09, 0x09, 0x06 },
    { 0x3E, 0x41, 0x51, 0x21, 0x5E }, { 0x7F, 0x09, 0x19, 0x29, 0x46 },
    { 0x46, 0x49, 0x49, 0x49, 0x31 }, { 0x01, 0x01, 0x7F, 0x01, 0x01 },
    { 0x3F, 0x40, 0x40, 0x40, 0x3F }, { 0x1F, 0x20, 0x40, 0x20, 0x1F },
    { 0x3F, 0x40, 0x38, 0x40, 0x3F }, { 0x63, 0x14, 0x08, 0x14, 0x63 },
    { 0x07, 0x08, 0x70, 0x08, 0x07 }, { 0x61, 0x51, 0x49, 0x45, 0x43 },
    { 0x08, 0x08, 0x08, 0x08, 0x08 }, { 0x00, 0x36, 0x36, 0x00, 0x00 },
    { 0x00, 0x60, 0x60, 0x00, 0x00 }, { 0x00, 0x41, 0x22, 0x14, 0x08 },
    { 0x00, 0x00, 0x00, 0x00, 0x00 }, { 0x02, 0x01, 0x51, 0x09, 0x06 }
  };
  UINTN Index;

  if (Column >= 5) {
    return 0;
  }
  if ((Character >= '0') && (Character <= '9')) {
    Index = Character - '0';
  } else if ((Character >= 'A') && (Character <= 'Z')) {
    Index = 10 + Character - 'A';
  } else {
    switch (Character) {
      case '-': Index = 36; break;
      case ':': Index = 37; break;
      case '.': Index = 38; break;
      case '/': Index = 39; break;
      case ' ': Index = 40; break;
      default: Index = 41; break;
    }
  }
  return Font[Index][Column];
}

STATIC
VOID
SecFramebufferPrint (
  IN UINTN Row,
  IN CONST CHAR8 *String
  )
{
  UINTN CharacterIndex;
  UINTN GlyphX;
  UINTN GlyphY;
  UINTN X;
  UINTN Y;
  UINTN Scale;
  UINT32 Pixel;

  if ((mSecFramebuffer.Base == NULL) || (String == NULL)) {
    return;
  }
  Scale = 2;
  for (CharacterIndex = 0; String[CharacterIndex] != '\0'; CharacterIndex++) {
    X = CharacterIndex * 12;
    Y = Row * 16;
    if (X + 12 > mSecFramebuffer.Width || Y + 14 > mSecFramebuffer.Height) {
      break;
    }
    for (GlyphY = 0; GlyphY < 14; GlyphY++) {
      for (GlyphX = 0; GlyphX < 12; GlyphX++) {
        Pixel = ((GlyphX < 10) && ((SecGlyphColumn (String[CharacterIndex], GlyphX / Scale) &
          (1U << (GlyphY / Scale))) != 0)) ? 0x00FFFFFF : 0;
        if (((Y + GlyphY) * mSecFramebuffer.PixelsPerScanLine + X + GlyphX) * sizeof (UINT32) < mSecFramebuffer.Size) {
          mSecFramebuffer.Base[(Y + GlyphY) * mSecFramebuffer.PixelsPerScanLine + X + GlyphX] = Pixel;
        }
      }
    }
  }
}

void edkii_vga_write_at_offset(unsigned int line, unsigned int offset, const char *string)
{
#if !defined (GOOXI_G4DEL_LINEAR_GRAPHICS)
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

#else
  (void)line;
  (void)offset;
  (void)string;
#endif
}


static void
log_vga_row(unsigned int line)
{
#if !defined (GOOXI_G4DEL_LINEAR_GRAPHICS)
    volatile const unsigned short *cells;
    char text[81];
    unsigned int i;

    if (line >= 25)
        return;
    cells = (volatile const unsigned short *)0xB8000 + 80 * line;
    for (i = 0; i < 80; i++) {
        unsigned char ch = cells[i] & 0xff;
        text[i] = ch ? (char)ch : ' ';
    }
    while (i > 0 && text[i - 1] == ' ')
        i--;
    text[i] = '\0';
    if (i != 0)
        DEBUG ((DEBUG_ERROR, "[GX-VGA] row=%u %a\n", line, text));

#else
  (void)line;
#endif
}

void edkii_vga_print(unsigned int line, const char *string)
{
    unsigned int i;

    if (mSecFramebuffer.Base != NULL) {
        SecFramebufferPrint (line, string);
        if (string != NULL) {
            DEBUG ((DEBUG_ERROR, "[GX-VGA] row=%u %a\n", line, string));
        }
        return;
    }

    edkii_vga_write_at_offset(line, 0, string);
    if (string == NULL)
        return;
    for (i = 0; string[i] != '\0'; i++) {
        if (string[i] != ' ') {
            #if defined (GOOXI_G4DEL_LINEAR_GRAPHICS)
            DEBUG ((DEBUG_ERROR, "[GX-VGA] row=%u %a\n", line, string));
#else
            log_vga_row(line);
#endif
            break;
        }
    }
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
  
  edkii_vga_print (row, mde_2_g_buffer);
}

void edkii_vga_clear()
{
  if (mSecFramebuffer.Base != NULL) {
    return;
  }
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
        log_vga_row(row);
    }
}



/////////////////////////////////////////////////////
