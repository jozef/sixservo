#!/usr/bin/env perl

use strict;
use warnings;
use 5.010;

use AnyEvent;
use AnyEvent::SerialPort;
use AnyEvent::IO qw(aio_open aio_write :flags);
use Getopt::Long;
use Pod::Usage;

my $MONITOR_PORT = '/dev/ttyUSB1';
my $ACTOR_PORT   = '/dev/ttyUSB0';

exit main();

sub main {
    my ($help,$do_log);
    GetOptions(
        'log_on_enter=s' => \$do_log,
        'help|h'         => \$help,
    ) or pod2usage(1);
    pod2usage(0) if $help;


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
    my %mon_servos;
    $monitor_ae->on_read(
        sub {
            my ($hdl) = @_;
            $hdl->push_read(
                line => sub {
                    my ($hdl, $line) = @_;
                    say '< m: ' . $line;
                    if ($line =~ m/^sr (\d) (.+)/) {
                        $mon_servos{$1} = $2;
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

    my $log_fh;
    my $stdin_ae;
    if ($do_log) {
        aio_open(
            $do_log,
            (O_WRONLY | O_CREAT | O_APPEND),
            0600,
            sub {
                ($log_fh) = @_ or return AE::log error => "$! - can not open log";
            }
        );

        $stdin_ae = AnyEvent->io (fh => \*STDIN, poll => 'r', cb => sub {
            <STDIN>;
            return unless $log_fh;
            return unless %mon_servos;

            foreach my $servo_no (sort keys %mon_servos) {
                aio_write($log_fh, sprintf("sr %d %d\n", $servo_no, $mon_servos{$servo_no}), sub {});
            }
            aio_write($log_fh, "# ---\n", sub {});
            say "--- saved ---\n";
        });
    }

    $run_cv->recv;

    return 0;
}


__END__

=head1 NAME

sixservo-mirror.pl - will replay monitored servo moves

=head1 SYNOPSIS

    sixservo-mirror.pl
        --log_on_enter=FILE     will save current C<sr> values into a FILE
        --help                  print out this help

=head1 DESCRIPTION

Will turn on monitoring on F</dev/ttyUSB1> and send them to F</dev/ttyUSB0>.

=head1 LINKS

See script in action: L<https://www.instagram.com/p/CF66RVUDJ8H/>

=cut
