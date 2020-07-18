#include <Arduino.h>

#include "singleservo.h"
#include <Servo.h>

#define WAIT_CHANGE_MS 100   // how long to wait for feedback to stabilize
#define MAX_ITER 300
#define CALIB_RANGE 0x160
#define NC_TH 5
#define DEBUG 0

void singleservo::detach() {
    if (_servo.attached()) {
        _servo.detach();
    }
}

bool singleservo::set_dpos(uint8_t new_dpos) {
    return set_dpos(new_dpos, false);
}

bool singleservo::set_dpos(uint8_t new_dpos, bool do_wait_stable) {
    if (new_dpos > 180) return false;

    return set_spos(
        map(new_dpos, 0, 180, d.spos_min, d.spos_max),
        do_wait_stable
    );
}

bool singleservo::set_spos(uint16_t new_spos) {
    return set_spos(new_spos, false);
}

bool singleservo::set_spos(uint16_t new_spos, bool do_wait_stable) {
    if (d.is_disabled) return false;
    if (new_spos < d.spos_min) {
        set_spos(d.spos_min, do_wait_stable);
        return false;
    }
    if (new_spos > d.spos_max) {
        set_spos(d.spos_max, do_wait_stable);
        return false;
    }

    if (!_servo.attached()) {
        _servo.attach(d.signal_pin, d.spos_min, d.spos_max);
    }
    _servo.writeMicroseconds(new_spos);
    _last_spos = new_spos;

    return (do_wait_stable ? wait_stable() : true);
}

bool singleservo::set_rpos(int16_t new_rpos) {
    return set_rpos(new_rpos, false);
}

bool singleservo::set_rpos(int16_t new_rpos, bool do_wait_stable) {
    if (d.is_clockwise) new_rpos *= -1;
    return set_spos(d.spos_zero + new_rpos, do_wait_stable);
}

bool singleservo::wait_stable() {
    if (!has_feedback) {
        return false;
    }

    uint16_t max_iterations = MAX_ITER + 1;
    while (--max_iterations) {
        uint16_t _prev_fpos = get_fpos();
        delay(WAIT_CHANGE_MS);

        get_fpos();
        if (_prev_fpos == _last_fpos) {
            if (DEBUG) {
                Serial.print(F("stable at fpos: "));
                Serial.println(_last_fpos);
            }
            break;
        }
    }

    return (max_iterations ? true : false);
}

uint16_t singleservo::get_fpos() {
    return get_fpos(4);
}

uint16_t singleservo::get_fpos(uint8_t scount) {
    uint16_t samples = 0;
    for (int8_t i = 0; i < scount; i++) {
        if (i) delay(10);
        samples += analogRead(d.feedback_pin);
    }
    _last_fpos = samples / scount;
    return _last_fpos;
}


void singleservo::dump_data() {
    Serial.print(F("signal pin: "));    Serial.println(d.signal_pin);
    Serial.print(F("feedback pin: "));  Serial.println(d.feedback_pin);
    Serial.print(F("has feedback: "));  Serial.println(has_feedback);
    Serial.print(F("is calibrated: ")); Serial.println(d.is_calibrated);
    Serial.print(F("is clockwise: "));   Serial.println(d.is_clockwise);
    Serial.print(F("is disabled: "));   Serial.println(d.is_disabled);
    Serial.print(F("spos zero: "));     Serial.println(d.spos_zero);
    Serial.print(F("spos min: "));      Serial.println(d.spos_min);
    Serial.print(F("spos max: "));      Serial.println(d.spos_max);
    Serial.print(F("pos min: "));       Serial.println(d.pos_min);
    Serial.print(F("pos max: "));       Serial.println(d.pos_max);
    Serial.print(F("fpos zero: "));     Serial.println(d.fpos_zero);
    Serial.print(F("fpos min: "));      Serial.println(d.fpos_min);
    Serial.print(F("fpos max: "));      Serial.println(d.fpos_max);
    Serial.print(F("fpos cur: "));      Serial.println(get_fpos());
}

void singleservo::_init() {
    pinMode(d.feedback_pin, INPUT);
    has_feedback = (get_fpos(8) < NC_TH ? false : true);
    if (!set_spos(d.spos_zero, 1)) delay(500);
    detach();
}

singleservo::singleservo(uint8_t spin, uint8_t fpin) {
    d.signal_pin = spin;
    d.feedback_pin = fpin;
    _init();
}

singleservo::singleservo(singleservo_data init_d) {
    d = init_d;
    _init();
}

bool singleservo::set_zero_pos_current() {
    if (!has_feedback) {
        if (_last_spos == 0) return false;

        set_spos(_last_spos);
        return true;
    }

    uint16_t cur_spos = (_last_spos > 0 ? _last_spos : d.spos_zero);
    uint16_t wanted_fpos = get_fpos();

    uint16_t max_iterations = MAX_ITER + 1;
    while (--max_iterations) {
        set_spos(cur_spos);
        delay(WAIT_CHANGE_MS);

        get_fpos();
        if (wanted_fpos == _last_fpos) {
            break;
        }
        else {
            if (wanted_fpos > _last_fpos) {
                cur_spos++;
            }
            else {
                cur_spos--;
            }
            if (abs(wanted_fpos - _last_fpos) > 10) {
                cur_spos += ((wanted_fpos - _last_fpos) * 2);
            }
        }
    }

    if (max_iterations != 0) {
        d.spos_zero = cur_spos;
        d.fpos_zero = wanted_fpos;
        return true;
    }
    else {
        return false;
    }
}

int16_t _approx_border (int16_t l10, int16_t l11, int16_t l12, int16_t l20, int16_t l21) {
    int32_t ll11 = l11 - l10;
    int32_t ll12 = l12 - l11;
    int32_t ll21 = l21 - l20;
    int32_t ll22 = (ll21 * ll12) / ll11;
    return (l21 + ll22);
}

bool singleservo::calibrate() {
    if (!set_zero_pos_current()) {
        return false;
    }

    set_rpos(CALIB_RANGE, true);
    uint16_t fpos_cw = get_fpos();
    uint16_t spos_cw = _last_spos;
    d.fpos_min = _approx_border(d.spos_zero, spos_cw, d.spos_min, d.fpos_zero, fpos_cw);

    set_rpos(0-CALIB_RANGE, true);
    uint16_t fpos_ccw = get_fpos();
    uint16_t spos_ccw = _last_spos;
    d.fpos_max = _approx_border(d.spos_zero, spos_ccw, d.spos_max, d.fpos_zero, fpos_ccw);

    set_rpos(0);
    d.is_calibrated = true;

    return true;
}

uint16_t singleservo::get_spos() {
    if (!has_feedback) return _last_spos;

    return map(get_fpos(), d.fpos_min, d.fpos_max, d.spos_min, d.spos_max);
}

int16_t singleservo::get_rpos() {
    if (!d.is_calibrated) return 0;

    return (d.is_clockwise ? 1 : -1) * (d.spos_zero - get_spos());
}
