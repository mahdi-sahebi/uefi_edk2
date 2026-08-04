/** @file
  Base Library CPU Functions for all architectures.
  Modified to include zero-dependency VGA debug trap.

  Copyright (c) 2006 - 2024, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Base.h>
#include <Library/BaseLib.h>

// // Only enable VGA trap on x86 architectures
// #if defined(MDE_CPU_IA32) || defined(MDE_CPU_X64)

// #include <stdarg.h>
// #include <stdint.h>
// #include <stddef.h>

// #define __MN_VGA_FB_ADDR 0xB8000
// #define __MN_VGA_COLS    80
// #define __MN_VGA_ROWS    25
// #define __MN_VGA_ATTR    0x0F00

// static void mn_int_to_hex(char *buf, uint64_t val) {
//     char tmp[20];
//     int i = 0;
//     if (val == 0) { tmp[i++] = '0'; }
//     else {
//         while (val > 0) {
//             int rem = val & 0xF;
//             tmp[i++] = (rem < 10) ? ('0' + rem) : ('A' + rem - 10);
//             val >>= 4;
//         }
//     }
//     int j = 0;
//     while (i > 0) buf[j++] = tmp[--i];
//     buf[j] = '\0';
// }

// static void mn_int_to_dec(char *buf, int64_t val) {
//     char tmp[20];
//     int i = 0;
//     int neg = 0;
//     if (val < 0) { neg = 1; val = -val; }
//     if (val == 0) { tmp[i++] = '0'; }
//     else {
//         while (val > 0) {
//             tmp[i++] = '0' + (val % 10);
//             val /= 10;
//         }
//     }
//     int j = 0;
//     if (neg) buf[j++] = '-';
//     while (i > 0) buf[j++] = tmp[--i];
//     buf[j] = '\0';
// }

// static void mn_vga_print(int row, const char *fmt, ...) {
//     va_list args;
//     va_start(args, fmt);
    
//     volatile uint16_t *vga_row = (volatile uint16_t *)__MN_VGA_FB_ADDR + (row * __MN_VGA_COLS);
//     int col = 0;
    
//     for (int i = 0; i < __MN_VGA_COLS; i++) {
//         vga_row[i] = __MN_VGA_ATTR | ' ';
//     }
    
//     while (*fmt && col < __MN_VGA_COLS) {
//         if (*fmt != '%') {
//             vga_row[col++] = __MN_VGA_ATTR | (uint8_t)*fmt++;
//             continue;
//         }
//         fmt++;
        
//         if (*fmt == 's') {
//             const char *s = va_arg(args, const char *);
//             if (!s) s = "(null)";
//             while (*s && col < __MN_VGA_COLS) vga_row[col++] = __MN_VGA_ATTR | (uint8_t)*s++;
//         } 
//         else if (*fmt == 'd' || *fmt == 'i') {
//             int val = va_arg(args, int);
//             char buf[20];
//             mn_int_to_dec(buf, val);
//             for (int i = 0; buf[i] && col < __MN_VGA_COLS; i++) vga_row[col++] = __MN_VGA_ATTR | buf[i];
//         }
//         else if (*fmt == '%') {
//             vga_row[col++] = __MN_VGA_ATTR | '%';
//         }
//         fmt++;
//     }
//     va_end(args);
// }

// void mn_print_loc(const char* File, const char* Function, int Line) {
//     mn_vga_print(23, "HALT FILE: %s", File); 
//     mn_vga_print(24, "HALT FUNC: %s() LINE: %d", Function, Line); 
// }

// #endif // MDE_CPU_IA32 || MDE_CPU_X64

// Original CpuDeadLoop logic
static volatile UINTN  mDeadLoopComparator = 0;

VOID
EFIAPI
CpuDeadLoop (
  VOID
  )
{
#if defined(MDE_CPU_IA32) || defined(MDE_CPU_X64)
  // If called via macro, mn_print_loc already printed. 
  // If called directly, we still want to halt safely.
#endif

  volatile UINTN  Index;
  for (Index = mDeadLoopComparator; Index == mDeadLoopComparator;) {
    CpuPause ();
  }
}