#ifndef CHARACTER_H
#define CHARACTER_H

#include "Arduino.h"
#include "bitmaps.h"
#include "util.h"
#include <pgmspace.h>
#include <Adafruit_GFX.h>
#include <Arduino_ST7789.h>

// 20 frames per second
#define FPS 10

enum {
  ORIENT_FRONT = 0,
  ORIENT_LEFT,
  ORIENT_RIGHT,
  ORIENT_BACK,
  ORIENT_JUMP,
  ORIENT_SLEEP
};
enum { STATUS_WAIT = 0, STATUS_STOP, STATUS_MOVE, STATUS_LIGHT };
enum {
  MOVE_UP = 0,
  MOVE_LEFT,
  MOVE_RIGHT,
  MOVE_DOWN,
  MOVE_LEFTBACK,
  MOVE_RIGHTBACK
};

class Character {
public:
  Character(Arduino_ST7789 *display);
  void start(uint8_t o);
  void start(uint16_t x, uint16_t y, uint8_t o);
  void stop(uint8_t o);
  void sleep();
  void setSpeed(uint8_t s);
  void incSpeed();
  void decSpeed();
  void setOrient(uint8_t o);
  uint32_t getOrientTimer();
  //void setOrientByAccel(int32_t ax);
  void moveDist(uint8_t d, int16_t dist);
  void moveTo(uint16_t to_x, uint16_t to_y);
  void queueMoveTo(uint16_t to_x, uint16_t to_y, uint16_t s = 4, uint16_t d = 1);
  void queueAction(uint8_t s, uint16_t p1, uint16_t p2, uint16_t p3 = 0, uint16_t p4 = 0);
  void queueLight(uint16_t light);
  void dequeueMoveTo();
  void dequeueAction();
  void clearActionQueue();
  void setStatus(uint8_t status);
  uint8_t getStatus();
  uint32_t getSleepTime();
  uint32_t update(); // returns millisecs to wait
protected:
  Arduino_ST7789 *tft;
  int16_t move_queue[SIZE_QUEUE][5];
  int16_t move_queue_idx;
  int16_t move_queue_last;
  int16_t home_x, home_y;
  int16_t pos_x, pos_y, width, height, target_x, target_y;
  uint8_t status;
  uint8_t speed;   // speed (1-) drawing interval (5 is slow, 1 is fast)
  uint8_t pattern; // character pattern (0-4)
  uint8_t frame;
  uint8_t orient;
  uint8_t direction;
  uint8_t move_diff;
  uint32_t orient_timer[10];
  uint16_t wait_timer;
  uint32_t drawBitmap16(const unsigned char *data, int16_t x, int16_t y, uint16_t w,
                   uint16_t h);
};

#endif
