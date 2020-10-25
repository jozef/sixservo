#ifndef singleservo_h
#define singleservo_h

#include <Arduino.h>
#include <Servo.h>

#define SPOS_MIN 500
#define SPOS_MAX 2500

struct singleservo_data {
    uint8_t signal_pin;
    uint8_t feedback_pin;

    bool is_calibrated = false;
    bool is_clockwise  = true;
    bool is_disabled   = false;

    uint16_t spos_zero = 1500;
    uint16_t spos_min  = SPOS_MIN;
    uint16_t spos_max  = SPOS_MAX;
    int16_t pos_min   = -500;
    int16_t pos_max   =  500;
    uint16_t fpos_zero =  511;
    uint16_t fpos_min  =    0;
    uint16_t fpos_max  = 1023;
};

class singleservo {
    public:
        singleservo(uint8_t spin, uint8_t fpin);
        singleservo(singleservo_data init_d);

        singleservo_data d;
        bool has_feedback;

        bool set_rpos(int16_t new_pos);
        bool set_rfpos(int16_t new_pos, uint16_t in_millis);
        bool set_rpos(int16_t new_pos, bool wait_stable);
        int16_t get_rpos();
        bool set_zero_pos_current();

        bool set_spos(uint16_t new_spos);
        bool set_spos(uint16_t new_spos, bool do_wait_stable);
        uint16_t get_spos();
        uint16_t get_fpos();
        uint16_t get_fpos(uint8_t scount);
        bool set_dpos(uint8_t new_dpos);
        bool set_dpos(uint8_t new_dpos, bool do_wait_stable);

        bool calibrate();
        void detach();
        bool wait_stable();
        void dump_data();
        void tick();
    private:
        uint16_t _last_spos = 0;
        uint16_t _last_fpos = 0;

        // future/tick positions
        int16_t _t_slots = 0;
        uint16_t _next_spos = 0;
        unsigned long _next_spos_millis = 0;
        uint16_t _next_spos2 = 0;
        unsigned long _next_spos2_millis = 0;
        Servo _servo;

        void _init();
};

#endif
