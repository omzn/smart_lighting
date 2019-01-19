#ifndef UTIL_H
#define UTIL_H

//#define Serial SerialUSB
#define HOME_X 14
#define HOME_Y 104

#define TIME_BACKHOME 10000
#define TIME_SLEEP    40000
#define TIME_TFTSLEEP 60000

#define SIZE_QUEUE 30
/*
    RGB565 -> BGR565 conversion
*/
inline uint16_t fixColor(uint16_t color) {
  return (color);
//  return (color & 0xf800) >> 11 | (color & 0x07e0) | (color & 0x001f) << 11;
}

#endif