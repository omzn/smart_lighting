#include "character.h"
#include "util.h"

Character::Character(Arduino_ST7789 *display) {
  tft = display;
  width = 32;
  height = 32;
  pos_x = home_x = HOME_X;
  pos_y = home_y = HOME_Y;
  pattern = 0;
  frame = 0;
  speed = 4;
  orient = ORIENT_FRONT;
  status = STATUS_STOP;
  move_diff = 1;
  move_queue_idx = 0;
  move_queue_last = 0;
}

void Character::start(uint8_t o) {
  setOrient(o);
  setStatus(STATUS_WAIT);
}

void Character::start(uint16_t x, uint16_t y, uint8_t o) {
  pos_x = x;
  pos_y = y;
  setOrient(o);
  setStatus(STATUS_WAIT);
}

void Character::stop(uint8_t o) {
  setOrient(o);
  setStatus(STATUS_STOP);
}

void Character::sleep() {
  if (getOrientTimer() > TIME_SLEEP) {
    setOrient(ORIENT_SLEEP);
    setSpeed(8);
  }
}

void Character::setSpeed(uint8_t s) {
  if (s > 0) {
    speed = s;
  }
}

void Character::incSpeed() { speed++; }

void Character::decSpeed() {
  if (speed > 1) {
    speed--;
  }
}

void Character::setOrient(uint8_t o) {
  if (orient != o) {
    orient = o;
    orient_timer[orient] = millis();
  }
}

uint32_t Character::getSleepTime() {
  if (orient == ORIENT_SLEEP) {
    return getOrientTimer();
  } else {
    return 0;
  }
}

uint32_t Character::getOrientTimer() {
  return (millis() - orient_timer[orient]);
}

uint8_t Character::getStatus() { return status; }

void Character::setStatus(uint8_t s) {
  if (status != s) {
    status = s;
  }
}

void Character::moveDist(uint8_t d, int16_t distance) {
  if (status == STATUS_MOVE) {
    return;
  }
  status = STATUS_MOVE;
  direction = d;
  if (direction == MOVE_UP) {
    target_x = pos_x;
    target_y = pos_y - distance >= -31 ? pos_y - distance : -31;
  } else if (direction == MOVE_DOWN) {
    target_x = pos_x;
    target_y = pos_y + distance < tft->height() - 1 ? (pos_y + distance)
                                                    : (tft->height() - 1);
  } else if (direction == MOVE_LEFT) {
    target_x = pos_x - distance >= -31 ? pos_x - distance : -31;
    target_y = pos_y;
  } else if (direction == MOVE_RIGHT) {
    target_x = pos_x + distance < tft->width() - 1 ? (pos_x + distance)
                                                   : (tft->width() - 1);
    target_y = pos_y;
  } else if (direction == MOVE_LEFTBACK) {
    target_x = pos_x - distance >= -31 ? pos_x - distance : -31;
    target_y = pos_y;
  } else if (direction == MOVE_RIGHTBACK) {
    target_x = pos_x + distance < tft->width() - 1 ? (pos_x + distance)
                                                   : (tft->width() - 1);
    target_y = pos_y;
  }
}

void Character::moveTo(uint16_t to_x, uint16_t to_y) {
  if (status == STATUS_MOVE) {
    return;
  }
  status = STATUS_MOVE;
  //  direction = d;
  target_x =
      to_x >= 0 ? (to_x < tft->width() - 32 ? to_x : (tft->width() - 32)) : 0;
  target_y =
      to_y >= 0 ? (to_y < tft->height() - 32 ? to_y : (tft->height() - 32)) : 0;
  if (target_x > pos_x) {
    direction = MOVE_RIGHT;
  } else if (target_x < pos_x) {
    direction = MOVE_LEFT;
  } else if (target_y > pos_y) {
    direction = MOVE_DOWN;
  } else if (target_y < pos_y) {
    direction = MOVE_UP;
  }
  /*
  if (direction == MOVE_UP) {
    target_x = to_x;
    target_y = to_y >= 0 ? to_y : 0;
  } else if (direction == MOVE_DOWN) {
    target_x = to_x;
    target_y = to_y < tft->height() - 32 ? to_y : (tft->height() - 32);
  } else if (direction == MOVE_LEFT) {
    target_x = to_x >= 0 ? to_x : 0;
    target_y = to_y;
  } else if (direction == MOVE_RIGHT) {
    target_x = to_x < tft->width() - 32 ? to_x : (tft->width() - 32);
    target_y = to_y;
  } else if (direction == MOVE_LEFTBACK) {
    target_x = to_x >= 0 ? to_x : 0;
    target_y = to_y;
  } else if (direction == MOVE_RIGHTBACK) {
    target_x = to_x < tft->width() - 32 ? to_x : (tft->width() - 32);
    target_y = to_y;
  }
  */
}

void Character::queueAction(uint8_t s, uint16_t p1, uint16_t p2, uint16_t p3,
                            uint16_t p4) {
  move_queue[move_queue_last][0] = s;
  move_queue[move_queue_last][1] = p1;
  move_queue[move_queue_last][2] = p2;
  move_queue[move_queue_last][3] = p3;
  move_queue[move_queue_last][4] = p4;
  move_queue_last++;
  move_queue_last %= SIZE_QUEUE;
}

void Character::queueMoveTo(uint16_t to_x, uint16_t to_y, uint16_t speed,
                            uint16_t hurry) {
  move_queue[move_queue_last][0] = STATUS_MOVE;
  move_queue[move_queue_last][1] = to_x;
  move_queue[move_queue_last][2] = to_y;
  move_queue[move_queue_last][3] = speed;
  move_queue[move_queue_last][4] = hurry;
  move_queue_last++;
  move_queue_last %= SIZE_QUEUE;
}

void Character::dequeueAction() {
  if (move_queue_idx == move_queue_last)
    return;
  if (getOrientTimer() < wait_timer) {
    return;
  } else {
    wait_timer = 0;
  }
  if (move_queue[move_queue_idx][0] == STATUS_MOVE) {
    setSpeed(move_queue[move_queue_idx][3]);
    move_diff = move_queue[move_queue_idx][4];
    moveTo(move_queue[move_queue_idx][1], move_queue[move_queue_idx][2]);
    //    Serial.println("move:" + String(move_queue[move_queue_idx][1]) + "," +
    //    String(move_queue[move_queue_idx][2]) );
  } else if (move_queue[move_queue_idx][0] == STATUS_WAIT) {
    setOrient(move_queue[move_queue_idx][1]);
    wait_timer = move_queue[move_queue_idx][2];
    //    Serial.println("wait:" + String(move_queue[move_queue_idx][1]) + "," +
    //    String(move_queue[move_queue_idx][2]) );
  } else if (move_queue[move_queue_idx][0] == STATUS_LIGHT) {
    setOrient(ORIENT_JUMP);
    wait_timer = 5000;
    uint8_t jj = move_queue[move_queue_idx][1];
    uint16_t color = move_queue[move_queue_idx][2];
    uint16_t br = move_queue[move_queue_idx][3];
    tft->fillCircle(30 + 60 * (jj % 4), 30 + (60 * (3 * (jj / 4))), 29, color);
    tft->drawCircle(30 + 60 * (jj % 4), 30 + (60 * (3 * (jj / 4))), 29, WHITE);
    tft->setCursor(30 + 60 * (jj % 4) - (br < 10 ? 9 : (br > 99 ? 27 : 18)),
                   30 + (60 * (3 * (jj / 4))) + 9);
    tft->setTextColor(br > 50 ? BLACK : WHITE);
    tft->print(br);
    setSpeed(4);
    move_diff = 1;
  }
  move_queue_idx++;
  move_queue_idx %= SIZE_QUEUE;
}

void Character::dequeueMoveTo() {
  if (move_queue_idx == move_queue_last)
    return;
  moveTo(move_queue[move_queue_idx][0], move_queue[move_queue_idx][1]);
  move_queue_idx++;
  move_queue_idx %= SIZE_QUEUE;
}

void Character::clearActionQueue() { move_queue_idx = move_queue_last = 0; }

uint32_t Character::update() {
  uint32_t e = 0;
  if (status != STATUS_STOP) {
    if (status == STATUS_MOVE) {
      if (direction == MOVE_UP) {
        tft->fillRect(pos_x, pos_y + 32 - move_diff, 32, move_diff, BLACK);
        pos_y -= move_diff;
        pos_y = pos_y < target_y ? target_y : pos_y;
        setOrient(ORIENT_BACK);
      } else if (direction == MOVE_DOWN) {
        tft->fillRect(pos_x, pos_y, 32, move_diff, BLACK);
        pos_y += move_diff;
        pos_y = pos_y > target_y ? target_y : pos_y;
        setOrient(ORIENT_FRONT);
      } else if (direction == MOVE_LEFT) {
        tft->fillRect(pos_x + 32 - move_diff, pos_y, move_diff, 32, BLACK);
        pos_x -= move_diff;
        pos_x = pos_x < target_x ? target_x : pos_x;
        setOrient(ORIENT_LEFT);
      } else if (direction == MOVE_RIGHT) {
        tft->fillRect(pos_x, pos_y, move_diff, 32, BLACK);
        pos_x += move_diff;
        pos_x = pos_x > target_x ? target_x : pos_x;
        setOrient(ORIENT_RIGHT);
      } else if (direction == MOVE_LEFTBACK) {
        tft->fillRect(pos_x + 32 - move_diff, pos_y, move_diff, 32, BLACK);
        pos_x -= move_diff;
        pos_x = pos_x < target_x ? target_x : pos_x;
        setOrient(ORIENT_RIGHT);
      } else if (direction == MOVE_RIGHTBACK) {
        tft->fillRect(pos_x, pos_y, move_diff, 32, BLACK);
        pos_x += move_diff;
        pos_x = pos_x > target_x ? target_x : pos_x;
        setOrient(ORIENT_LEFT);
      }
      e = drawBitmap16(aqua_bmp[orient][pattern], pos_x, pos_y, width, height);
      if (frame == 0) {
        pattern++;
        pattern %= 4;
      }
      if (pos_x == target_x) {
        if (pos_y < target_y) {
          direction = MOVE_DOWN;
          setOrient(ORIENT_FRONT);
        } else if (pos_y > target_y) {
          direction = MOVE_UP;
          setOrient(ORIENT_BACK);
        }
      }
      if (pos_y == target_y) {
        if (pos_x < target_x) {
          direction = MOVE_RIGHT;
          setOrient(ORIENT_RIGHT);
        } else if (pos_x > target_x) {
          direction = MOVE_LEFT;
          setOrient(ORIENT_LEFT);
        }
      }
      if (pos_x == target_x && pos_y == target_y) {
        setStatus(STATUS_WAIT);
      }
    } else {
      if (frame == 0) {
        e = drawBitmap16(aqua_bmp[orient][pattern], pos_x, pos_y, width,
                         height);
        pattern++;
        pattern %= 4;
      }
    }
    frame++;
    frame %= speed;
  }
  uint32_t d = (1000 / FPS) - e;
  return d > 0 ? d : 0;
}

uint32_t Character::drawBitmap16(const unsigned char *data, int16_t x,
                                 int16_t y, uint16_t w, uint16_t h) {
  uint16_t row, col, buffidx = 0;
  uint32_t startTime = millis();

  if ((x >= tft->width()) || (y >= tft->height())) {
    return 0;
  }
  uint16_t b = 0;
  uint16_t cnt = 0;
  for (col = 0; col < w; col++) { // For each scanline...
    for (row = 0; row < h; row++) {
      uint16_t c = pgm_read_word(data + buffidx);
      c = ((c >> 8) & 0x00ff) | ((c << 8) & 0xff00); // swap back and fore
      if (col + x >= 0 && row + y >= 0) {
        tft->drawPixel(col + x, row + y, c);
      }
      buffidx += 2;
    } // end pixel
  }
  uint32_t etime = millis() - startTime;
  return etime;
}
