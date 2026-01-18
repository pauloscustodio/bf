#!/usr/bin/env perl

use Modern::Perl;
use Test::More;
use Config;
use Capture::Tiny 'capture_merged';
use Path::Tiny;
use Text::Diff;

$ENV{PATH} = join($Config{path_sep}, 
			".",
			$ENV{PATH});

use vars '$test', '$null';
$test = "test_".(($0 =~ s/\.t$//r) =~ s/[\.\/\\]/_/gr);
$null = ($^O eq 'MSWin32') ? 'nul' : '/dev/null';

unlink_testfiles();

#------------------------------------------------------------------------------
sub check_bin_file {
    my($got_file, $exp_bin) = @_;
    local $Test::Builder::Level = $Test::Builder::Level + 1;
    
	my $got_bin = slurp($got_file);
	eq_or_dump_diff($got_bin, $exp_bin, "bin file $got_file ok");
	
	(Test::More->builder->is_passing) or die;
}

#------------------------------------------------------------------------------
sub check_text_file {
    my($got_file, $exp_text) = @_;
    local $Test::Builder::Level = $Test::Builder::Level + 1;
    
	(my $got_text = slurp($got_file)) =~ s/\r\n/\n/g;
	$exp_text =~ s/\r\n/\n/g;
	
	my $diff = diff(\$exp_text, \$got_text, {STYLE => 'Table'});
	is $diff, "", "text file $got_file ok";
	
	(Test::More->builder->is_passing) or die;
}

#------------------------------------------------------------------------------
sub capture_ok {
    my($cmd, $exp_out) = @_;
    local $Test::Builder::Level = $Test::Builder::Level + 1;
    
    run_ok($cmd." > ${test}.stdout");
    check_text_file("${test}.stdout", $exp_out);
	
	(Test::More->builder->is_passing) or die;
}

#------------------------------------------------------------------------------
sub capture_nok {
    my($cmd, $exp_err) = @_;
    local $Test::Builder::Level = $Test::Builder::Level + 1;

    run_nok($cmd." 2> ${test}.stderr");
    check_text_file("${test}.stderr", $exp_err);
	
	(Test::More->builder->is_passing) or die;
}

#------------------------------------------------------------------------------
sub run_ok {
    my($cmd) = @_;
    local $Test::Builder::Level = $Test::Builder::Level + 1;
	
	ok 1, "Running: $cmd";
    ok 0==system($cmd), $cmd;
	
	(Test::More->builder->is_passing) or die;
}

#------------------------------------------------------------------------------
sub run_nok {
    my($cmd) = @_;
    local $Test::Builder::Level = $Test::Builder::Level + 1;
	
	ok 1, "Running: $cmd";
    ok 0!=system($cmd), $cmd;
	
	(Test::More->builder->is_passing) or die;
}

#------------------------------------------------------------------------------
sub unlink_testfiles {
	my(@additional) = @_;
    unlink(<${test}*>, @additional) 
        if Test::More->builder->is_passing;
}

#------------------------------------------------------------------------------
# path()->spew fails sometimes on Windows (race condition?) with 
# Error rename on 'test_t_ALIGN.asm37032647357911' -> 'test_t_ALIGN.asm': Permission denied
# replace by a simpler spew without renames
sub spew {
	my($file, @data) = @_;
	local $Test::Builder::Level = $Test::Builder::Level + 1;

	my $open_ok = open(my $fh, ">:raw", $file);
	ok $open_ok, "write $file"; 
	
	if ($open_ok) {
		print $fh join('', @data);
	}
	
	(Test::More->builder->is_passing) or die;
}

#------------------------------------------------------------------------------
# and for simetry
sub slurp {
	my($file) = @_;
	local $Test::Builder::Level = $Test::Builder::Level + 1;

	my $open_ok = open(my $fh, "<:raw", $file);
	ok $open_ok, "read $file";
	
	if ($open_ok) {
		read($fh, my $data, -s $file);
		return $data;
	}
	else {
		return "";
	}
	
	(Test::More->builder->is_passing) or die;
}

1;
