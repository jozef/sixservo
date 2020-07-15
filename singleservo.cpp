#include <Arduino.h>

#include "singleservo.h"
#include <Servo.h>

#define WAIT_CHANGE_MS 50   // how long to wait for feedback to stabilize
#define MAX_ITER 300
// #define CALIB_RANGE 0x60
// #define CALIB_EXTEND 0x04
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

bool singleservo::set_spos(int16_t new_spos) {
    return set_spos(new_spos, false);
}

bool singleservo::set_spos(int16_t new_spos, bool do_wait_stable) {
    if (d.is_disabled) return false;
    if (new_spos < d.spos_min) return false;
    if (new_spos > d.spos_max) return false;

    if (!_servo.attached()) {
        _servo.attach(d.signal_pin, d.spos_min, d.spos_max);
    }
    _servo.writeMicroseconds(new_spos);

    return (do_wait_stable ? wait_stable() : true);
}

bool singleservo::wait_stable() {
    if (!has_feedback) {
        return false;
    }

    uint16_t max_iterations = MAX_ITER + 1;
    while (--max_iterations) {
        int16_t _prev_fpos = get_fpos();
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

int16_t singleservo::get_fpos() {
    return get_fpos(4);
}

int16_t singleservo::get_fpos(uint8_t scount) {
    int16_t samples = 0;
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
    Serial.print(F("is reversed: "));   Serial.println(d.is_reversed);
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

// bool singleservo::set_zero_pos_current() {
//     if (!has_feedback) {
//         set_spos(cur_spos);
//         return false;
//     }
//
//     int16_t cur_spos = _last_spos || _spos_zero;
//     int16_t wanted_fpos = get_fpos();
//
//     uint16_t max_iterations = MAX_ITER + 1;
//     while (--max_iterations) {
//         set_spos(cur_spos);
//         delay(WAIT_CHANGE_MS);
//
//         get_fpos();
//         if (wanted_fpos == _last_fpos) {
//             break;
//         }
//         else if (wanted_fpos < _last_fpos) {
//             cur_spos++;
//         }
//         else {
//             cur_spos--;
//         }
//     }
//
//     return (max_iterations != 0);
// }
//
// bool singleservo::do_calibrate() {
//     if (!set_zero_pos_current()) {
//         return false;
//     }
//
//     if (!set_spos(CALIB_RANGE, true)) {
//         return false;
//     }
//     _fpos_max = get_fpos() * CALIB_EXTEND;
//     if (!set_spos(0-CALIB_RANGE, true)) {
//         return false;
//     }
//     _spos_min = get_fpos() * CALIB_EXTEND;
//
//     set_spos(0);
//     _spos_min = _spos_zero - (CALIB_RANGE * CALIB_EXTEND);
//     _spos_max = _spos_zero + (CALIB_RANGE * CALIB_EXTEND);
//     is_calibrated = true;
//
//     return true;
// }
//
// bool singleservo::set_spos(int16_t new_pos) {
//     return set_spos(new_pos, false);
// }
//
// bool singleservo::set_spos(int16_t new_pos, bool wait_stable) {
//     _last_spos = _spos_zero + new_pos;
//
//     // no change if out of range spos
//     if ((_last_spos < MIN_SPOS) or (_last_spos > MAX_SPOS)) {
//         _last_spos = _spos_zero - new_pos;
//         return false;
//     }
//
//     if (!_servo.attached()) {
//         _servo.attach(_signal_pin);
//     }
//     _servo.writeMicroseconds(_last_spos);
//
//     uint16_t max_iterations = MAX_ITER;
//     while (max_iterations--) {
//         int16_t _prev_fpos = get_fpos();
//         delay(WAIT_CHANGE_MS);
//         get_fpos();
//         if (_prev_fpos == _last_fpos) {
//             break;
//         }
//     }
//
//     return (max_iterations ? true : false);
// }
//
// int16_t singleservo::get_pos() {
//     // last position unknown
//     if (_last_spos == 0) {
//         return _spos_zero;
//     }
//
//     return _last_spos - _spos_zero;
// }
//
