#!/usr/bin/env perl

use strict;
use warnings;
use 5.010;

use AnyEvent;
use AnyEvent::SerialPort;
use Path::Class qw(file);
use Time::HiRes qw(time);
use Getopt::Long;
use Pod::Usage;

my $DEFAULT_S_PORT = '/dev/ttyUSB0';
my $STEP_MS_FIRST  = 3000;
my $STEP_MS_BASE   = 750;

exit main();

sub main {
    my ($help,$do_init,@rigid, @serial_ports);
    my $step_ms      = $STEP_MS_BASE;
    GetOptions(
        'help|h' => \$help,
        'port=s' => \@serial_ports,
        'step=s' => \$step_ms,
        'rigid'  => \@rigid,
        'init'   => \$do_init,
    ) or pod2usage(1);
    pod2usage(0) if $help;
    @serial_ports = ($DEFAULT_S_PORT)
        unless @serial_ports;

    # read/update commands
    my @cmds;
    my @last_cmd;
    while (my $line = <>) {
        chomp($line);
        if ($line =~ m/^#/) {
            if (@last_cmd) {
                push(@cmds, [@last_cmd]);
                @last_cmd = ();
            }
            last if $do_init;
            next;
        }
        push(@last_cmd, $line);
    }
    push(@cmds, \@last_cmd)
        if @last_cmd;
    exit(1) unless @cmds;

    # run
    my (@run_cvs, @all_timers);
    foreach my $serial_port (@serial_ports) {
        my ($run_cv, $timers) = schedule_cmds($serial_port, $step_ms, [@cmds], shift(@rigid));
        push(@run_cvs,    $run_cv);
        push(@all_timers, @$timers);
    }
    foreach my $run_cv (@run_cvs) {
        $run_cv->recv;
    }
    return 0;
}

sub schedule_cmds {
    my ($serial_port, $step_ms, $cmds, $rigid) = @_;

    my $run_cv = AnyEvent->condvar;

    my $actor_ae = AnyEvent::SerialPort->new(
        serial_port => [
            $serial_port,
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
                    say $serial_port.'< a: ' . $line;
                }
            );
        }
    );

    my $term_cb = sub {
        say 'terminating, bye bye';
        $actor_ae->push_write("d 077\n");
        my $w;
        $w = AE::timer(1, 0, sub {$w = undef; $run_cv->send;});
    };
    my $wt = AE::signal TERM => $term_cb;
    my $wh = AE::signal HUP  => $term_cb;
    my $wi = AE::signal INT  => $term_cb;

    my $do_one_cmd = sub {
        my ($is_first) = @_;
        return $run_cv->send
            unless @$cmds;

        say 'time: ' . time();

        my @cmd_set = @{shift(@$cmds)};
        foreach my $cmd (@cmd_set) {
            if (!$rigid) {
                my $step_ms = ($is_first ? ($STEP_MS_FIRST + int($step_ms / 3)) : $step_ms);
                $cmd =~ s/^sr\s(.+)/srf $1 $step_ms/;
            }

            say $serial_port.'> a: ' . $cmd;
            $actor_ae->push_write($cmd . "\n");
        }
    };

    my $first_step;
    my $ticker;
    my $posponed_start = AE::timer(
        ($STEP_MS_FIRST / 1000),
        0,
        sub {
            $first_step = AE::timer(0, ($STEP_MS_FIRST / 1000), sub { $do_one_cmd->(1) });
            $ticker = AE::timer(($STEP_MS_FIRST / 1000), $step_ms / 1000, sub { $do_one_cmd->(0) });
        }
    );

    return ($run_cv, [$posponed_start, $first_step, $ticker]);
}

__END__

=head1 NAME

sixservo-replay.pl - will replay monitored servo moves

=head1 SYNOPSIS

    sixservo-replay.pl moves-file.log
        --port=XXX      set serial port, (ex. /dev/ttyUSB1)
                        can be present multiple times
        --step=AAA      set time in milli seconds for one step
        --init          perform only first step/position
        --rigid         will replay commands as they are, no smoothening
                        can be present multiple times, once per port
        --help          this help

=head1 DESCRIPTION

Will send commands from STDIN to F</dev/ttyUSB0>.

=head1 LINKS

See F<sixservo-mirror.pl> with C<--log_on_enter>.

More info: L<https://blog.kutej.net/2020/10/bob-303>

=cut
