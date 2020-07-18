// for documentation see end of this file

#include <Arduino.h>
#include <EEPROM.h>             // https://github.com/arduino/ArduinoCore-avr/blob/2f67c916f6ab6193c404eebe22efe901e0f9542d/libraries/EEPROM/src/EEPROM.h
#include <TextCMD.h>            // https://github.com/jozef/Arduino-TextCMD
#include <UpTime.h>             // https://github.com/jozef/Arduino-UpTime
#include <Servo.h>
#include "singleservo.h"

#define MAX_SERVOS 6

const char MAGIC[] = "6servo 0.01 ";

singleservo *sixservos[MAX_SERVOS];

struct sixservo_config {
    uint8_t monitor_on;
    uint16_t monitor_delay;
    uint8_t future_conf[29];
};
uint8_t i2c_register = 0;

sixservo_config config = {
    0,    // which servos to monitor
    1000, // monitor delay
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0  // place-holder for future config options
};

cmd_dispatch commands[] = {
    { "?",     &cmd_help     },
    { "dump",  &cmd_dump },
    { "ss",    &cmd_set_spos },
    { "sd",    &cmd_set_dpos },
    { "sr",    &cmd_set_rpos },
    { "z",     &cmd_set_zero },
    { "c",     &cmd_calibrate },
    { "m",     &cmd_monitor },
    { "sweep", &cmd_servo_sweep },
    { "d",     &cmd_detach },
    { "save",  &cmd_save },
    { "frst",  &cmd_cfg_reset }
};
TextCMD cmd((sizeof(commands)/sizeof(commands[0])),commands);

void setup () {
    analogReference(DEFAULT);
    sixservos[0] = new singleservo(5, A3);
    sixservos[1] = new singleservo(6, A2);
    sixservos[2] = new singleservo(7, A1);
    sixservos[3] = new singleservo(8, A0);
    sixservos[4] = new singleservo(9, A7);
    sixservos[5] = new singleservo(10, A6);

    Serial.begin(9600);
    while (Serial.available()) { Serial.read(); }

    char magic[strlen(MAGIC)];
    for (uint8_t i=0; i<strlen(MAGIC); i++) {
        magic[i] = EEPROM.read(i);
    }

    if (memcmp(MAGIC,magic,strlen(MAGIC)) == 0) {
        Serial.print(F("eeprom magic found: "));
        Serial.println(MAGIC);

        // read config
        for (uint8_t i = 0; i < sizeof(config); i++) {
            *((char*)&config+i) = EEPROM.read(strlen(MAGIC)+i);
        }
        // read servo configuration
        for (uint8_t idx = 0; idx < MAX_SERVOS; idx++) {
            uint8_t base = strlen(MAGIC)+sizeof(config)+sizeof(singleservo_data)*idx;
            EEPROM.get(base, sixservos[idx]->d);
        }
    }
    else {
        Serial.println(F("eeprom magic missmatch, doing init"));
        // after magic, there is MAX_SERVOS of bool has_address and DeviceAddress
        // write magic
        for (uint8_t i=0; i<strlen(MAGIC); i++) {
            EEPROM.write(i,MAGIC[i]);
        }
        // write config
        for (uint8_t i = 0; i < sizeof(config); i++) {
            EEPROM.write(strlen(MAGIC)+i, *((char*)&config+i));
        }
        // write servo configuration
        for (uint8_t idx = 0; idx < MAX_SERVOS; idx++) {
            uint8_t base = strlen(MAGIC)+sizeof(config)+sizeof(singleservo_data)*idx;
            EEPROM.put(base, sixservos[idx]->d);
        }
    }
}

void loop () {
    while (Serial.available()) {
        char ch = Serial.read();
        if (ch == '\b') { Serial.print(F("\b \b")); }
        else { Serial.print(ch); }
        switch (cmd.add_char(ch)) {
            case -1: Serial.println(F("unknown command or syntax error. send '?' for help")); break;
        }
    }
    delay(100);
}

int8_t cmd_help(uint8_t argc, const char* argv[]) {
    Serial.println(MAGIC);
    Serial.println(F("supported commands:"));
    Serial.println(F("    sd [X] [int]             - turn servo to 0-180"));
    Serial.println(F("    ss [X] [int]             - set puls width 500-2500"));
    Serial.println(F("    sr [X] [int]             - set position relative to zero -1000-1000"));
    Serial.println(F("    sweep [X] [int] [int]    - sweep servo [X] 0-180-0, optional step delay and count"));
    Serial.println(F("    d [X]                    - detach / stop controlling the servo"));
    Serial.println(F("    z [X]                    - set current as zero position"));
    Serial.println(F("    c [X]                    - calibrate"));
    Serial.println(F("    m [X] [int] [int]        - monitor servos"));
    Serial.println(F("    dump [X]                 - show servo debug values"));
    Serial.println(F("    frst                     - factory reset"));
    Serial.println(F("    ?                        - print this help"));
    Serial.println(F("[X] is servo index 1-6 or bitwise 0100-0177 (6 last bits as mask), default is zero -> 0177"));
    return 0;
}

void _okko(bool well, uint8_t sidx) {
    if (well) {
        Serial.print(F("ok "));
    }
    else {
        Serial.print(F("ko "));
    }
    Serial.println(sidx + 1);
}

int8_t _decode_idxs(const char* nchars) {
    uint8_t base = 0;
    if ((nchars[0] == '0') && (nchars[1] == 'b')) {
        nchars += 2;
        base = 2;
    }
    uint8_t idxs = strtol(nchars, NULL, base);
    if (idxs == 0) {
        idxs = 077;
    }
    else if (idxs < 7) {
        idxs = 1 << (idxs - 1);
    }
    else {
        idxs &= 077;
    }

    return idxs;
}

int8_t cmd_dump(uint8_t argc, const char* argv[]) {
    uint8_t idxs = (argc > 1 ? _decode_idxs(argv[1]) : 0177);

    for (uint8_t idx = 0; idx < 7; idx++) {
        if (!(idxs & (1 << idx))) continue;
        Serial.print(F("--- Servo "));
        Serial.print(idx + 1);
        Serial.println(F(" ---"));
        sixservos[idx]->dump_data();
    }
    Serial.println(F("---"));

    return 0;
}

int8_t cmd_set_spos(uint8_t argc, const char* argv[]) {
    if (argc != 3) return -1;
    uint8_t idxs = _decode_idxs(argv[1]);
    int spos = strtol(argv[2], NULL, 0);

    for (uint8_t idx = 0; idx < 7; idx++) {
        if (!(idxs & (1 << idx))) continue;
        _okko(
            sixservos[idx]->set_spos(spos, 0),
            idx
        );
    }

    return 0;
}

int8_t cmd_set_rpos(uint8_t argc, const char* argv[]) {
    if (argc != 3) return -1;
    uint8_t idxs = _decode_idxs(argv[1]);
    int rpos = strtol(argv[2], NULL, 0);

    for (uint8_t idx = 0; idx < 7; idx++) {
        if (!(idxs & (1 << idx))) continue;
        _okko(
            sixservos[idx]->set_rpos(rpos, 0),
            idx
        );
    }

    return 0;
}

void set_dpos(uint8_t idxs, uint8_t dpos) {
    for (uint8_t idx = 0; idx < 7; idx++) {
        if (!(idxs & (1 << idx))) continue;
        _okko(
            sixservos[idx]->set_dpos(dpos, 0),
            idx
        );
    }
}

int8_t cmd_set_dpos(uint8_t argc, const char* argv[]) {
    if (argc != 3) return -1;
    uint8_t idxs = _decode_idxs(argv[1]);
    int dpos = strtol(argv[2], NULL, 0);

    set_dpos(idxs, dpos);

    return 0;
}


int8_t cmd_detach(uint8_t argc, const char* argv[]) {
    uint8_t idxs = (argc > 1 ? _decode_idxs(argv[1]) : 0177);

    for (uint8_t idx = 0; idx < 7; idx++) {
        if (!(idxs & (1 << idx))) continue;
        sixservos[idx]->detach();
        _okko( true, idx );
    }

    return 0;
}

int8_t cmd_save(uint8_t argc, const char* argv[]) {
    uint8_t idxs = (argc > 1 ? _decode_idxs(argv[1]) : 0177);

    for (uint8_t idx = 0; idx < 7; idx++) {
        if (!(idxs & (1 << idx))) continue;
        uint8_t base = strlen(MAGIC)+sizeof(config)+sizeof(singleservo_data)*idx;
        EEPROM.put(base, sixservos[idx]->d);
        _okko( true, idx );
    }

    return 0;
}

int8_t cmd_set_zero(uint8_t argc, const char* argv[]) {
    uint8_t idxs = (argc > 1 ? _decode_idxs(argv[1]) : 0177);

    for (uint8_t idx = 0; idx < 7; idx++) {
        if (!(idxs & (1 << idx))) continue;
        _okko( sixservos[idx]->set_zero_pos_current(), idx );
    }

    return 0;
}

int8_t cmd_calibrate(uint8_t argc, const char* argv[]) {
    uint8_t idxs = (argc > 1 ? _decode_idxs(argv[1]) : 0177);

    for (uint8_t idx = 0; idx < 7; idx++) {
        if (!(idxs & (1 << idx))) continue;
        _okko( sixservos[idx]->calibrate(), idx );
    }

    return 0;
}

int8_t cmd_monitor(uint8_t argc, const char* argv[]) {
    uint8_t idxs = (argc > 1 ? _decode_idxs(argv[1]) : 0177);
    uint16_t iterations = (argc > 2 ? strtol(argv[2], NULL, 0) : 1);
    uint16_t step_delay = (argc > 3 ? strtol(argv[3], NULL, 0) : 1000);

    for (uint16_t iter = 0; (iterations != 0xffff ? iter < iterations : 1); iter++) {
        if (iter != 0) delay(step_delay);

        Serial.print("t ");
        Serial.println(uptime());
        for (uint8_t idx = 0; idx < 7; idx++) {
            if (!(idxs & (1 << idx))) continue;

            Serial.print(F("sr "));
            Serial.print(idx+1);
            Serial.print(' ');
            Serial.println(sixservos[idx]->get_rpos());
        }

        if (Serial.available()) break;
    }

    return 0;
}

int8_t cmd_servo_sweep(uint8_t argc, const char* argv[]) {
    int delay_time = 15;
    int loop_count = 1;
    uint8_t idxs = (argc > 1 ? _decode_idxs(argv[1]) : 077);

    if (argc >= 3) {
        loop_count = strtol(argv[2], NULL, 0);
        if (loop_count <= 0) {
            Serial.println(F("loop count has to be positive, non-zero int"));
            return -1;
        }
        if (argc >= 4) {;
            delay_time = strtol(argv[3], NULL, 0);
            if (delay_time <= 0) {
                Serial.println(F("delay has to be positive, non-zero int"));
                return -1;
            }
        }
    }

    for (uint8_t loop = 0; loop < loop_count; loop++) {
        Serial.print(F("Sweep "));
        Serial.print(loop+1);
        Serial.print(F(" of "));
        Serial.println(loop_count);

        for (uint8_t pos = 90; pos < 180; pos++) {
            set_dpos(idxs, pos);
            delay(delay_time);
        }
        delay(delay_time);
        for (uint8_t pos = 179; pos != 0; pos--) {
            set_dpos(idxs, pos);
            delay(delay_time);
        }
        delay(delay_time);
        for (uint8_t pos = 0; pos < 91; pos++) {
            set_dpos(idxs, pos);
            delay(delay_time);
        }
    }

    return 0;
}

int8_t cmd_cfg_reset(uint8_t argc, const char* argv[]) {
    EEPROM.write(0, 0);
    Serial.print(F("magic cleared, now doing reset"));
    void (*do_reset)(void) = 0;
    do_reset();
    return 0;
}


/*

=head1 NAME

sixservo.ino - control 6 servo motors via serial console

=head1 SYNOPSIS

after flashing into Arduino, connect via serial console 9600 to send commands:

    supported commands:
        sd [X] [int]             - turn servo to 0-180
        ss [X] [int]             - set puls width 500-2500
        sr [X] [int]             - set position relative to zero -1000-1000
        sweep [X] [int] [int]    - sweep servo [X] 0-180-0, optional count and step delay
        d [X]                    - detach / stop controlling the servo
        z [X]                    - set current as zero position
        c [X]                    - calibrate
        m [X] [int] [int]        - monitor servos, optional count and delay
        dump [X]                 - show servo debug values
        ?                        - print this help
    [X] is servo index 1-6 or bitwise 0100-0177 (6 last bits as mask), default is zero -> 0177
    ss 1 1500
    ok 1
    sd 0143 45
    ok 1
    ok 2
    ok 6

=head1 DESCRIPTION

Serial console accepts commands and sets servo motor accordingly.

=head1 COMMANDS

[X] is servo index 1-6 or bitwise 0100-0177 (6 last bits as mask),
default is zero -> 0177

=head2 sd [X] [int] [int]

Set servo angle.
Value range 0 to 180.

=head2 ss [X] [int] [int]

Set servo angle using PWM peak time length in nano seconds.
Value range 500 to 2500.

=head1 sweep [X] [int] [int]

Moves servo with index of first parameter from 90 to 180, then back to
0 and again to 90 degree.

Both parameters are optional.
First is the number of repeats, default 1.
Second sets the time in milliseconds to wait between each angle change,
default 15.

=head1 d [X]

Will detach - stop sending PWM to servo, turning the control off.

=head1 z [X]

Will set current servo position as zero.

=head1 c [X]

Will calibrate servo -> calculate feedback be moving servo form current
position back and forth.

=head1 m [X] [int] [int]

Will monitor servos and return C<sr> commands based on the feedback pin
readings.

Both paramters are optional.
First parameter is the count of how many readings will be done. Use -1
or 0xffff to set infinite count. Will finish on any serial input.
Second parameter is the delay between reading.

=head1 dump [X]

Will dump servo data.

=head1 save [X]

Will save servo data into eeprom for permanent storage.

=head1 rfst [X]

Factory-reset will clear eeprom and start from defaults.

=head1 SEE ALSO

KiCAD PCBs are here: L<https://github.com/jozef/sixservo/tree/master/KiCAD>,
first single servo version: L<https://blog.kutej.net/2020/04/servo-tester>

=head1 LICENSE

This is free software, licensed under the MIT License.

=cut

*/
