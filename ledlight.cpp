#include "ledlight.h"

ledLight::ledLight(uint8_t pin) {
  _dim = 0;
  _pin = pin;
  _reset_flag = 1;
  _status = 0;
  pinMode(_pin, OUTPUT);
}

uint8_t ledLight::status() {
  return _status;
}

int ledLight::schedule() {
  return _enable_schedule;
}


// スケジュールを有効(1)・無効(0)にする．
void ledLight::schedule(int v) {
  _enable_schedule = (v == 0 ? 0 : 1);
}

// スケジュールの点灯・消灯時刻を設定する．
void ledLight::schedule(int on_h, int on_m, int off_h, int off_m) {
  if (on_h >= 0 && on_h <= 23) {
    _on_h = on_h;
  } else {
    _on_h = 0;
  }
  if (on_m >= 0 && on_m <= 59) {
    _on_m = on_m;
  } else {
    _on_m = 0;
  }
  if (off_h >= 0 && off_h <= 23) {
    _off_h = off_h;
  } else {
    _off_h = 0;
  }
  if (off_m >= 0 && off_m <= 59) {
    _off_m = off_m;
  } else {
    _off_m = 0;
  }
}

int ledLight::power() {
  return int(_power);
}

void ledLight::power(int v) {
  if (v > 0 && v < MAX_PWM_VALUE ) {
    _power = v;
  } else if (v >= MAX_PWM_VALUE ) {
    _power = MAX_PWM_VALUE;
  } else if (v <= 0) {
    _power = 0;
  }
  if (_power == MAX_PWM_VALUE) {
    _status = LIGHT_ON;
  } else if (_power == 0) {
    _status = LIGHT_OFF;
  }
  if (_power >= MAX_PWM_VALUE) {
    _power = MAX_PWM_VALUE;
    digitalWrite(_pin, HIGH);
  } else if (_power <= 0) {
    _power = 0;
    digitalWrite(_pin, LOW);
  } else {
    analogWrite(_pin, int(_power));
  }
#ifdef DEBUG
  Serial.print("pwm: ");
  Serial.println(_power);
  Serial.print("status: ");
  Serial.println(_status);
#endif
}

void ledLight::on_h(int v) {
  _on_h = v;
}
void ledLight::on_m(int v) {
  _on_m = v;
}
void ledLight::off_h(int v) {
  _off_h = v;
}
void ledLight::off_m(int v) {
  _off_m = v;
}
int ledLight::on_h() {
  return _on_h;
}
int ledLight::on_m() {
  return _on_m;
}
int ledLight::off_h() {
  return _off_h;
}
int ledLight::off_m() {
  return _off_m;
}

uint16_t ledLight::dim() {
  return _dim;
}

// dimの間隔(秒)を指定する．dimが0の時は，照明は瞬時に点消灯する．
void ledLight::dim(int v) {
  _dim = v;
}


// ライトをvalの強さで光らせる．スケジュールが設定されているときは，-1を返し何もしない．
int ledLight::control(uint16_t val) {
  if (_enable_schedule) {
    return -1;
  } else {
    power(val);
    return val;
  }
}

void ledLight::incPower(float v) {
  if (v >= 0 && v <= MAX_PWM_VALUE) {
    _power += v;
    if (_power >= MAX_PWM_VALUE) {
      _power = MAX_PWM_VALUE;
    }
  } else {
    _power = MAX_PWM_VALUE;
  }
  if (int(_power) >= MAX_PWM_VALUE) {
    _power = MAX_PWM_VALUE;
    digitalWrite(_pin, HIGH);
  } else {
    analogWrite(_pin, int(_power));
  }
#ifdef DEBUG
  Serial.print("pwm: ");
  Serial.println(_power);
  Serial.print("status: ");
  Serial.println(_status);
#endif
}

void ledLight::decPower(float v) {
  if (v >= 0 && v <= MAX_PWM_VALUE) {
    _power -= v;
    if (_power <= 0 ) {
      _power = 0;
    }
  } else {
    _power = 0;
  }
  if ((int)_power <= 0) {
    digitalWrite(_pin, LOW);
  } else {
    analogWrite(_pin, int(_power));
  }
#ifdef DEBUG
  Serial.print("pwm: ");
  Serial.println(_power);
  Serial.print("status: ");
  Serial.println(_status);
#endif
}


// hh:mm時点で点灯すべきかどうかを判断する．1秒毎に呼び出される．
void ledLight::control(int hh, int mm) {
  if (_status == LIGHT_TURNING_ON) {
    incPower(float(MAX_PWM_VALUE) / float(_dim));
    if (_power == MAX_PWM_VALUE) {
      _status = LIGHT_ON;
    }
  }
  else if (_status == LIGHT_TURNING_OFF) {
    decPower(float(MAX_PWM_VALUE) / float(_dim));
    if (_power == 0) {
      _status = LIGHT_OFF;
    }
  }
  else if (_enable_schedule) {
    if ((_on_h < _off_h) || (_on_h == _off_h && _on_m <= _off_m )) {
      if ((hh > _on_h || hh >= _on_h && mm >= _on_m) && (hh < _off_h || hh == _off_h && mm < _off_m)) {
        if (power() == 0) {
          if (_reset_flag || _dim == 0) {
            power(MAX_PWM_VALUE);
            _status = LIGHT_ON;
          } else {
            _status = LIGHT_TURNING_ON;
          }
        }
      } else {
        if (power() == MAX_PWM_VALUE) {
          if (_reset_flag || _dim == 0) {
            power(0);
            _status = LIGHT_OFF;
          } else {
            _status = LIGHT_TURNING_OFF;
          }
        }
      }
    }
    else if ((_on_h > _off_h) || (_on_h == _off_h && _on_m >= _off_m )) {
      if ((hh > _off_h || hh >= _off_h && mm >= _off_m) && (hh < _on_h || hh == _on_h && mm < _on_m)) {
        if (power() == MAX_PWM_VALUE) {
          if (_reset_flag || _dim == 0) {
            power(0);
            _status = LIGHT_OFF;
          } else {
            _status = LIGHT_TURNING_OFF;
          }
        }
      } else {
        if (power() == 0) {
          if (_reset_flag || _dim == 0) {
            power(MAX_PWM_VALUE);
            _status = LIGHT_ON;
          } else {
            _status = LIGHT_TURNING_ON;
          }
        }
      }
    }
    _reset_flag = 0;
  }
}
