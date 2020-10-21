#!/usr/bin/env perl

use strict;
use warnings;
use 5.010;

use AnyEvent;
use AnyEvent::SerialPort;
use Path::Class qw(file);
use Time::HiRes qw(time);

my $ACTOR_PORT = '/dev/ttyUSB0';
my $STEP_DIVIDER = 10;

exit main();

sub main {
    my $run_cv = AnyEvent->condvar;

    my @cmds;
    my @last_cmd;
    while (my $line = <>) {
        if ($line =~ m/^#/) {
            if (@last_cmd) {
                push(@cmds, [@last_cmd]);
                @last_cmd = ();
            }
            next;
        }
        push(@last_cmd, $line);
    }
    push(@cmds, \@last_cmd)
        if @last_cmd;

    exit(1) unless @cmds;

    my @cmds_smooth = ($cmds[0]);
    for (my $i = 1; $i < @cmds; $i++) {
        my $sr_prev      = parse_rs($cmds[$i - 1]);
        my $sr_cur_delta = parse_rs($cmds[$i]);
        foreach my $idx (keys %$sr_cur_delta) {
            $sr_cur_delta->{$idx} = ($sr_cur_delta->{$idx} - $sr_prev->{$idx})/$STEP_DIVIDER;
        }
        foreach my $step (1..$STEP_DIVIDER-1) {
            push(
                @cmds_smooth,
                [   map {sprintf("sr %d %d\n", $_, $sr_prev->{$_} + $step * $sr_cur_delta->{$_})}
                    sort keys %$sr_cur_delta
                ]
            );
        }
        push(@cmds_smooth, $cmds[$i]);
    }

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

    my $term_cb = sub {
        say 'terminating, bye bye';
        $actor_ae->push_write("d 077\n");
        my $w;
        $w = AE::timer(1, 0, sub {$w = undef; $run_cv->send;});
    };
    my $wt = AE::signal TERM => $term_cb;
    my $wh = AE::signal HUP  => $term_cb;
    my $wi = AE::signal INT  => $term_cb;

    my $ticker;
    my $posponed_start = AE::timer(
        3, 0,
        sub {
            $ticker = AE::timer(
                1, (1/$STEP_DIVIDER),
                sub {
                    return $run_cv->send
                        unless @cmds_smooth;

                    say 'time: ' . time();

                    my @cmd_set = @{shift(@cmds_smooth)};
                    foreach my $cmd (@cmd_set) {
                        print '> a: ' . $cmd;
                        $actor_ae->push_write($cmd);
                    }
                }
            );
        }
    );

    $run_cv->recv;

    return 0;
}

sub parse_rs {
    my ($cmd_set) = @_;
    my %srs;
    foreach my $cmd (@{$cmd_set}) {
        next unless $cmd =~ m/^sr (\d) (-?\d+)$/;
        $srs{$1} = $2;
    }
    return \%srs;
}

__END__

=head1 NAME

sixservo-replay.pl - will replay monitored servo moves

=head1 SYNOPSIS

    sixservo-replay.pl moves-file.log

=head1 DESCRIPTION

Will send commands from STDIN to F</dev/ttyUSB0>.

=head1 LINKS

See F<sixservo-mirror.pl> with C<--log_on_enter>.

More info: L<https://blog.kutej.net/2020/10/bob-303>

=cut
