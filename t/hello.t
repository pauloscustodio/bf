#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

capture_ok("bf hello.bf", <<END);
Hello, World!
END

capture_ok("bfpp hello.bfpp | bf", <<END);
Hello, World!
END

unlink_testfiles;
done_testing;
