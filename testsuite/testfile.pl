#!/usr/bin/perl

# parse file read from STDIN
while(<STDIN>) {
	if(/^--- (.*)$/) {
		$_ = $1;
		if($_ eq "CONFIGURATION") {
			close DEST;
			open DEST, "testctrlproxyrc";
			$cur = "file";
		} elsif(/^SERVER:(.*)$/) {
			close DEST;
			open DEST, "server:$1";
			$num = $1;
			$cur = "file";
		} elsif(/^CLIENT:(.*)$/) {
			close DEST;
			$num = $1;
			$cur = "client";
		} elsif(/^EXPECTED:(.*)$/) {
			$num = $1;
			$cur = "expected";
		} else {
			die("Unknown tag $_\n");
		}
	} else {
		if($cur == "file") {
			print DEST, "$_\n";
		} else if($cur == "client") {
			$client{$num}.=$_;
		} else if($cur == "expected") {
			$expected{$num},=$_;
		}
	}
}

# Launch ctrlproxy (background)
system "ctrlproxy --daemon -r testctrlproxyrc";

# Launch the various clients
foreach(keys %client) {
	# FIXME : Connect
	# FIXME : Send $client{$_}
	# FIXME : Receive $got{$_}
}

# Compare output
foreach(keys %got) {
	if($got{$_} eq $expected{$_}) {
		print "OK\n";
	} else {
		print "FAILED\n";
	}
}

system "killall ctrlproxy";
