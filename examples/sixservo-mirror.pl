#!/usr/bin/env perl

use strict;
use warnings;
use 5.010;

use AnyEvent;
use AnyEvent::SerialPort;

my $MONITOR_PORT = '/dev/ttyUSB1';
my $ACTOR_PORT   = '/dev/ttyUSB0';

exit main();

sub main {
    my $run_cv = AnyEvent->condvar;

    my $actor_ae = AnyEvent::SerialPort->new(
        serial_port => [
            $ACTOR_PORT,
            [baudrate => 19200],
            [parity   => 'none'],
            [databits => 8],
            [stopbits => 1],
        ],
        on_error => sub {
            my ($hdl, $fatal, $msg) = @_;
            die $msg;
        },
        on_eof => sub {
            $run_cv->send;
        },
    );
    $actor_ae->on_read(
        sub {
            my ($hdl) = @_;
            $hdl->push_read(
                line => sub {
                    my ($hdl, $line) = @_;
                    say '< a: ' . $line;
                }
            );
        }
    );

    my $monitor_ae = AnyEvent::SerialPort->new(
        serial_port => [
            $MONITOR_PORT,
            [baudrate => 19200],
            [parity   => 'none'],
            [databits => 8],
            [stopbits => 1],
        ],
        on_error => sub {
            my ($hdl, $fatal, $msg) = @_;
            die $msg;
        },
        on_eof => sub {
            $run_cv->send;
        },
    );
    $monitor_ae->on_read(
        sub {
            my ($hdl) = @_;
            $hdl->push_read(
                line => sub {
                    my ($hdl, $line) = @_;
                    say '< m: ' . $line;
                    if ($line =~ m/^sr \d /) {
                        say '> a: ' . $line;
                        $actor_ae->push_write($line . "\n");
                    }
                }
            );
        }
    );
    $monitor_ae->push_write("m 077 -1 1\n");

    my $term_cb = sub {
        say 'terminating, bye bye';
        $monitor_ae->push_write("d 077\n");
        $actor_ae->push_write("d 077\n");
        my $w;
        $w = AE::timer(1, 0, sub {$w = undef; $run_cv->send;})
    };
    my $wt = AE::signal TERM => $term_cb;
    my $wh = AE::signal HUP  => $term_cb;
    my $wi = AE::signal INT  => $term_cb;

    $run_cv->recv;

    return 0;
}


__END__

=head1 NAME

sixservo-mirror.pl - will replay monitored servo moves

=head1 DESCRIPTION

Will turn on monitoring on F</dev/ttyUSB1> and send them to F</dev/ttyUSB0>.

=head1 LINKS

See script in action: L<https://www.instagram.com/p/CF66RVUDJ8H/>

=cut
