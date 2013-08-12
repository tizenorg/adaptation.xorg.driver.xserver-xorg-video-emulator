#ifndef _VIGS_VIDEO_TYPES_H_
#define _VIGS_VIDEO_TYPES_H_

#include "vigs_config.h"
#include "xf86.h"
#include <fourcc.h>

#define VIGS_B(c,s)              ((((unsigned int)(c)) & 0xff) << (s))
#define VIGS_FOURCC(a,b,c,d)     (VIGS_B(d,24) | VIGS_B(c,16) | VIGS_B(b,8) | VIGS_B(a,0))

/*
 * http://www.fourcc.org/yuv.php
 * http://en.wikipedia.org/wiki/YUV
 */

#define VIGS_FOURCC_RGB565 VIGS_FOURCC('R','G','B','P')
#define VIGS_XVIMAGE_RGB565 \
   { \
    VIGS_FOURCC_RGB565, \
    XvRGB, \
    LSBFirst, \
    {'R','G','B','P', \
        0x00,0x00,0x00,0x10,0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71}, \
    16, \
    XvPacked, \
    1, \
    16, 0x0000F800, 0x000007E0, 0x0000001F, \
    0, 0, 0, 0, 0, 0, 0, 0, 0, \
    {'R','G','B',0, \
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, \
    XvTopToBottom \
   }

#define VIGS_FOURCC_RGB32 VIGS_FOURCC('R','G','B','4')
#define VIGS_XVIMAGE_RGB32 \
   { \
    VIGS_FOURCC_RGB32, \
    XvRGB, \
    LSBFirst, \
    {'R','G','B',0, \
        0x00,0x00,0x00,0x10,0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71}, \
    32, \
    XvPacked, \
    1, \
    24, 0x00FF0000, 0x0000FF00, 0x000000FF, \
    0, 0, 0, 0, 0, 0, 0, 0, 0, \
    {'X','R','G','B', \
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, \
    XvTopToBottom \
   }

#define VIGS_FOURCC_SN12 VIGS_FOURCC('S','N','1','2')
#define VIGS_XVIMAGE_SN12 \
   { \
    VIGS_FOURCC_SN12, \
    XvYUV, \
    LSBFirst, \
    {'S','N','1','2', \
      0x00,0x00,0x00,0x10,0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71}, \
    12, \
    XvPlanar, \
    3, \
    0, 0, 0, 0, \
    8, 8, 8, \
    1, 2, 2, \
    1, 2, 2, \
    {'Y','U','V', \
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, \
    XvTopToBottom \
   }

#define VIGS_FOURCC_ST12 VIGS_FOURCC('S','T','1','2')
#define VIGS_XVIMAGE_ST12 \
   { \
    VIGS_FOURCC_ST12, \
    XvYUV, \
    LSBFirst, \
    {'S','T','1','2', \
      0x00,0x00,0x00,0x10,0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71}, \
    12, \
    XvPlanar, \
    3, \
    0, 0, 0, 0, \
    8, 8, 8, \
    1, 2, 2, \
    1, 2, 2, \
    {'Y','U','V', \
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, \
    XvTopToBottom \
   }

#define VIGS_FOURCC_RGB24 VIGS_FOURCC('R','G','B','3')
#define VIGS_XVIMAGE_RGB24 \
   { \
    VIGS_FOURCC_RGB24, \
    XvRGB, \
    LSBFirst, \
    {'R','G','B',0, \
        0x00,0x00,0x00,0x10,0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71}, \
    24, \
    XvPacked, \
    1, \
    24, 0x00FF0000, 0x0000FF00, 0x000000FF, \
    0, 0, 0, 0, 0, 0, 0, 0, 0, \
    {'R','G','B',0, \
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, \
    XvTopToBottom \
   }

#define VIGS_FOURCC_S420 VIGS_FOURCC('S','4','2','0')
#define VIGS_XVIMAGE_S420 \
   { \
    VIGS_FOURCC_S420, \
    XvYUV, \
    LSBFirst, \
    {'S','4','2','0', \
      0x00,0x00,0x00,0x10,0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71}, \
    12, \
    XvPlanar, \
    3, \
    0, 0, 0, 0, \
    8, 8, 8, \
    1, 2, 2, \
    1, 2, 2, \
    {'Y','U','V', \
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, \
    XvTopToBottom \
   }

#define VIGS_FOURCC_SUYV VIGS_FOURCC('S','U','Y','V')
#define VIGS_XVIMAGE_SUYV \
   { \
    VIGS_FOURCC_SUYV, \
    XvYUV, \
    LSBFirst, \
    {'S','U','Y','V', \
      0x00,0x00,0x00,0x10,0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71}, \
    16, \
    XvPacked, \
    1, \
    0, 0, 0, 0, \
    8, 8, 8, \
    1, 2, 2, \
    1, 1, 1, \
    {'Y','U','Y','V', \
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, \
    XvTopToBottom \
   }

#define VIGS_FOURCC_NV12 VIGS_FOURCC('N','V','1','2')
#define VIGS_XVIMAGE_NV12 \
   { \
    VIGS_FOURCC_NV12, \
    XvYUV, \
    LSBFirst, \
    {'N','V','1','2', \
      0x00,0x00,0x00,0x10,0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71}, \
    12, \
    XvPacked, \
    1, \
    0, 0, 0, 0, \
    8, 8, 8, \
    1, 2, 2, \
    1, 2, 2, \
    {'Y','U','V', \
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, \
    XvTopToBottom \
   }

#endif
