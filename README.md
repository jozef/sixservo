# NAME

sixservo.ino - control 6 servo motors via serial console

# SYNOPSIS

after flashing into Arduino, connect via serial console 19200 to send commands:

    supported commands:
        sd [X] [int]             - turn servo to 0-180
        ss [X] [int]             - set puls width 500-2500
        sr [X] [int]             - set position relative to zero -1000-1000
        srf [X] [int] [int]      - set position relative to zero -1000-1000 and time in ms to do so
        sweep [X] [int] [int]    - sweep servo [X] 0-180-0, optional count and step delay
        d [X]                    - detach / stop controlling the servo
        z [X]                    - set current as zero position
        c [X]                    - calibrate
        m [X] [int] [int]        - monitor servos, optional count and delay [s]
        save                     - save config
        dump [X]                 - show servo debug values
        ?                        - print this help
    [X] is servo index 1-6 or bitwise 000-077 (6 last bits as mask), default is zero -> 077
    ss 1 1500
    ok 1
    sd 0143 45
    ok 1
    ok 2
    ok 6

# DESCRIPTION

Serial console (19200) accepts commands and sets servo motor accordingly.

# COMMANDS

\[X\] is servo index 1-6 or bitwise 000-077 (6 last bits as mask),
default is zero -> 077

## sd \[X\] \[int\] \[int\]

Set servo angle.
Value range 0 to 180.

## ss \[X\] \[int\] \[int\]

Set servo angle using PWM peak time length in nano seconds.
Value range 500 to 2500.

## sr \[X\] \[int\] \[int\]

Set servo angle using PWM peak time length, but ±relative to the zero
point set using `z` or `c` commands.

## srf \[X\] \[int\] \[int\] \[int\]

Sames as `sr` but the third argument is time in miliseconds when servo
angle should be fully reached -> makes for fluent/smooth movements.

## sweep \[X\] \[int\] \[int\]

Moves servo with index of first parameter from 90 to 180, then back to
0 and again to 90 degree.

Both parameters are optional.
First is the number of repeats, default 1.
Second sets the time in milliseconds to wait between each angle change,
default 15.

## d \[X\]

Will detach - stop sending PWM to servo, turning the control off.

## z \[X\]

Will set current servo position as zero.

## c \[X\]

Will calibrate servo -> calculate feedback be moving servo form current
position back and forth.

## m \[X\] \[int\] \[int\]

Will monitor servos and return `sr` commands based on the feedback pin
readings.

Both paramters are optional.
First parameter is the count of how many readings will be done. Use -1
or 0xffff to set infinite count. Will finish on any serial input.
Second parameter is the delay between reading.

## dump \[X\]

Will dump servo data.

## save \[X\]

Will save servo data into eeprom for permanent storage.

## rfst \[X\]

Factory-reset will clear eeprom and start from defaults.

# SEE ALSO

KiCAD PCBs are here: [https://github.com/jozef/sixservo/tree/master/KiCAD](https://github.com/jozef/sixservo/tree/master/KiCAD),
first single servo version: [https://blog.kutej.net/2020/04/servo-tester](https://blog.kutej.net/2020/04/servo-tester)
and application [https://blog.kutej.net/2020/10/bob-303](https://blog.kutej.net/2020/10/bob-303)

`examples/sixservo-mirror.pl` and `examples/sixservo-replay.pl`
scripts to mirror, record and replay servo positions.

# LICENSE

This is free software, licensed under the MIT License.
