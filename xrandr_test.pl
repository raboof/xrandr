#!/usr/bin/perl

#
# xrandr Test suite
#
# Do a set of xrandr calls and verify that the screen setup is as expected
# after each call.
#

$xrandr="xrandr";
$xrandr=$ENV{XRANDR} if defined $ENV{XRANDR};
$version="0.1";
$inbetween="";

# Get output configuration
@outputs=();
%out_modes=();
%modes=();
open P, "$xrandr|" or die "$xrandr";
while (<P>) {
  if (/^(\S+)\s(\S+)\s/) {
    $o="";
    if ($2 eq "connected") {
      $o=$1;
      push @outputs, $o;
      $out_modes{$o}=[];
    }
    elsif ($2 eq "unknown") {
      $o=$1;
      push @outputs_unknown, $o;
      $out_modes{$o}=[];
    }
  } elsif (/^\s+(\d+)x(\d+)\s+(\d.*?)\s*$/) {
    my $w=$1, $h=$2;
    $_=$3;
    while (/([0-9.]+)/g) {
      push @{$out_modes{$o}}, "${w}x$h\@$1";
      $modes{"$o:${w}x$h\@$1"} = 1;
      $modes{"$o:${w}x$h"} = "${w}x$h\@$1";
    }
  }
}
close P;

# preamble
print "\n***** xrandr test suite V$version *****\n\n";
if ($ARGV[0] eq "-w") {
  print "Waiting for keypress after each test for manual verification.\n\n";
  $inbetween='print "    Press <Return> to continue...\n"; $_=<STDIN>';
} elsif ($ARGV[0] ne "") {
  print "Preparing for test # $ARGV[0]\n\n";
  $prepare = $ARGV[0];
}

print "Detected connected outputs and available modes:\n\n";
for $o (@outputs) {
  print "$o:  ";
  for $m (@{$out_modes{$o}}) {
    print " $m";
  }
  print "\n";
}
print "\n";

@outputs=(@outputs,@outputs_unknown) if @outputs < 2;
if (@outputs < 2) {
  print "Found less than two connected outputs. No tests available for that.\n";
  exit 1;
}
if (@outputs > 2) {
  print "Note: No tests for more than two connected outputs available yet.\n";
  print "Using the first two outputs.\n\n";
}

$a=$outputs[0];
$b=$outputs[1];

# For each resolution only a single refresh rate should be used in order to
# reduce ambiguities. For that we need to find unused modes. The %used hash is
# used to track used ones. All references point to <width>x<height>@<refresh>.
#   <output>:<width>x<height>@<refresh>
#   <output>:<width>x<height>
#   <width>x<height>@<refresh>
#   <width>x<height>
%used=();

# Find biggest common mode
undef $sab;
for my $m (@{$out_modes{$a}}) {
  if (defined $modes{"$b:$m"}) {
    $sab=$m;
    $m =~ m/(\d+)x(\d+)\@([0-9.]+)/;
    $used{"$a:$m"} = $m;
    $used{"$b:$m"} = $m;
    $used{"$a:$1x$2"} = $m;
    $used{"$b:$1x$2"} = $m;
    $used{$m} = $m;
    $used{"$1x$2"} = $m;
    last;
  }
}
if (! defined $sab) {
  print "Cannot find common mode between $a and $b.\n";
  print "Test suite is designed to need a common mode.\n";
  exit 1;
}

# Find sets of additional non-common modes
# Try to get non-overlapping resolution set, but if that fails get overlapping
# ones but with different refresh values, and if that fails any one, but warn.
# Try modes unknown to other outputs first, they might need common ones
# themselves.
sub get_mode {
  my $o=$_[0];
  for my $pass (1, 2, 3, 4, 5, 6, 7) {
    CONT: for my $m (@{$out_modes{$o}}) {
      $m =~ m/(\d+)x(\d+)\@([0-9.]+)/;
      next CONT if defined $used{"$o:$m"};
      next CONT if defined $used{"$o:$1x$2"} && $pass < 7;
      next CONT if defined $used{$m} && $pass < 6;
      next CONT if defined $used{"$1x$2"} && $pass < 4;
      for my $other (@outputs) {
        next if $other eq $o;
        next CONT if $used{"$other:$1x$2"} && $pass < 5;
	next CONT if $modes{"$other:$m"} && $pass < 3;
	next CONT if $modes{"$other:$1x$2"} && $pass < 2;
      }
      if ($pass >= 6) {
        print "Warning: No more non-common modes, using $m for $o\n";
      }
      $used{"$o:$m"} = $m;
      $used{"$o:$1x$2"} = $m;
      $used{$m} = $m;
      $used{"$1x$2"} = $m;
      return $m;
    }
  }
  print "Warning: Cannot find any more modes for $o.\n";
  return undef;
}
sub mode_to_randr {
  $_[0] =~ m/(\d+)x(\d+)\@([0-9.]+)/;
  return "--mode $1x$2 --refresh $3";
}

$sa1=get_mode($a);
$sa2=get_mode($a);
$sb1=get_mode($b);
$sb2=get_mode($b);

$mab=mode_to_randr($sab);
$ma1=mode_to_randr($sa1);
$ma2=mode_to_randr($sa2);
$mb1=mode_to_randr($sb1);
$mb2=mode_to_randr($sb2);

# Shortcuts
$oa="--output $a";
$ob="--output $b";

# Print config
print "A:  $a (mab,ma1,ma2)\nB:  $b (mab,mb1,mb2)\n\n";
print "mab: $sab\nma1: $sa1\nma2: $sa2\nmb1: $sb1\nmb2: $sb2\n\n";
print "Initial config:\n";
system "$xrandr";
print "\n";

# Test subroutine
sub t {
  my $name=$_[0];
  my $expect=$_[1];
  my $args=$_[2];
  print "*** $name:\n";
  print "?   $expect\n" if $expect ne "";
  if ($name eq $prepare) {
    print "->  Prepared to run test\n\nRun test now with\n$xrandr --verbose $args\n\n";
    exit 0;
  }
  my $r = "", $out="";
  if (system ("$xrandr --verbose $args") == 0) {
    # Determine active configuration
    open P, "$xrandr --verbose|" or die "$xrandr";
    my $o, $c, $m, $x;
    while (<P>) {
      $out.=$_;
      if (/^\S/) {
        $o=""; $c=""; $m=""; $x="";
      }
      if (/^(\S+)\s(connected|unknown connection) (\d+x\d+)\+\d+\+\d+\s+\((0x[0-9a-f]+)\)/) {
        $o=$1;
	$m=$3;
	$x=$4;
	$o="A" if $o eq $a;
	$o="B" if $o eq $b;
      } elsif (/^\s*CRTC:\s*(\d)/) {
        $c=$1;
      } elsif (/^\s+$m\s+\($x\)/) {
        while (<P>) {
	  if (/^\s+v:.*?([0-9.]+)Hz\s*$/) {
            $r="$r  $o: $m\@$1($c)";
	    last;
	  }
	}
      }
    }
    close P;
  } else {
    $expect="success" if $expect="";
    $r="failed";
  }
  # Verify
  if ($expect ne "") {
    print "->$r\n";
    if ($r eq "  $expect") {
      print "->  ok\n\n";
    } else {
      print "\n$out";
      print "\n->  FAILED: Test # $name:\n\n";
      print "    $xrandr --verbose $args\n\n";
      exit 1;
    }
    eval $inbetween;
  } else {
    print "->  ignored\n\n";
  }
}


# Test cases
#
# The tests are carefully designed to test certain transitions between
# RandR states that can only be reached by certain calling sequences.
# So be careful with altering them. For additional tests, better add them
# to the end of already existing tests of one part.

# Part 1: Single output switching tests
t ("-",   "",                        "$oa --off $ob --off");
t ("s1",  "A: $sa1(0)",              "$oa $ma1 --crtc 0");
t ("s2",  "A: $sa1(0)  B: $sab(1)",  "$ob $mab");
# TODO: should be A: $sab(1) someday (auto re-cloning)"
#t ("s3",  "A: $sab(1)  B: $sab(1)",  "$oa $mab");
t ("s3",  "A: $sab(0)  B: $sab(1)",  "$oa $mab --crtc 0");
t ("s3a", "A: $sab(1)  B: $sab(1)",  "$oa $mab --crtc 1");
t ("s4",  "A: $sa2(0)  B: $sab(1)",  "$oa $ma2");
t ("s5",  "A: $sa1(0)  B: $sab(1)",  "$oa $ma1");
t ("s6",  "A: $sa1(0)  B: $sb1(1)",  "$ob $mb1");
t ("s7",  "A: $sab(0)  B: $sb1(1)",  "$oa $mab");
t ("s8",  "A: $sab(0)  B: $sb2(1)",  "$ob $mb2");
t ("s9",  "A: $sab(0)  B: $sb1(1)",  "$ob $mb1");
# TODO: should be B: $sab(0) someday (auto re-cloning)"
#t ("s10", "A: $sab(0)  B: $sab(0)",  "$ob $mab");
t ("s10", "A: $sab(0)  B: $sab(0)",  "$ob $mab --crtc 0");
t ("s11", "A: $sa1(1)  B: $sab(0)",  "$oa $ma1");
t ("s12", "A: $sa1(1)  B: $sb1(0)",  "$ob $mb1");
t ("s13", "A: $sa1(1)  B: $sab(0)",  "$ob $mab");
t ("s14", "A: $sa2(1)  B: $sab(0)",  "$oa $ma2");
t ("s15", "A: $sa1(1)  B: $sab(0)",  "$oa $ma1");

# Part 2: Dual output switching tests

# Done

print "All tests succeeded.\n";

exit 0;

