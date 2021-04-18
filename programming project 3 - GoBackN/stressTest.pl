#!/usr/bin/perl

use IPC::Run qw(run start pump finish timeout );


########################################
# Perform a test
########################################
sub doTest($$$) {

  ($count,$loss,$corrupt) = @_;
  

  @binary = ('./GoBackN', '-n', $count, '-l', $loss, '-c', $corrupt, '-t', '1', '-d', '4');


  run \@binary, \$in, \$out, \$err, timeout(100) or die "Can't run @binary\n";

  $packetSent = 0;
  $packetRecv = 0;
  $failed = 0;

  my @lines = split /\n/, $out;

  foreach my $line (@lines) {
    if ($line =~ /TOLAYER3: Scheduling/) {
      print "$line\n" if ($debug);
      $packetSent++;
    }

    
    if ($line =~ /TOLAYER5: Data received at application layer on side/) {
      @parts = split /\s+/,$line;
      if ($parts[9] =~ /$packet[$packetRecv % 26]/) {
	print "$parts[9] equals ",$packet[$packetRecv % 26],"\n" if ($debug);
      }
      else {
	print "ERROR: \'$parts[9]\' does not equal expected \'",$packet[$packetRecv % 26],"\'\n";
	$failed = 1;
      }
      $packetRecv ++;
    }
  }

  $failed = 1 if ($count != $packetRecv);
  
  print "Test ", join(' ',@binary);
  print " passed\n" if (!$failed);
  print " failed\n" if ($failed);
  print "$packetSent packets sent\n";
  print "$packetRecv packets recieved\n\n";
}


########################################
# main
########################################

###############
# Build the list of packet values
###############
@packet = ();
foreach $letter (a..z) {
  $line = "(";
  for ($j=0;$j<20;$j++) {
    $line = $line . $letter;
  }
  $line = $line . ").";
  push @packet,$line;
}

###############
# Run the tests.
###############
doTest(10000,0,0);
doTest(10000,.01,0);
doTest(10000,0,.01);
doTest(10000,.01,.01);
doTest(1000,.1,.1);
