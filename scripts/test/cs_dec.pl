##
# cs_trim.pl
#
# Basic tests to ensure that colorspace decoding is behaving sanely.
#

use strict;
use warnings;

my $bowtie = "./bowtie";
if(system("$bowtie --version") != 0) {
	$bowtie = `which bowtie`;
	chomp($bowtie);
	if(system("$bowtie --version") != 0) {
		die "Could not find bowtie in current directory or in PATH\n";
	}
}

my $bowtie_d = "./bowtie-debug";
if(system("$bowtie_d --version") != 0) {
	$bowtie_d = `which bowtie-debug`;
	chomp($bowtie_d);
	if(system("$bowtie_d --version") != 0) {
		die "Could not find bowtie-debug in current directory or in PATH\n";
	}
}

if(! -f "e_coli_c.1.ebwt") {
	print STDERR "Making colorspace e_coli index\n";
	system("make bowtie-build") && die;
	system("bowtie-build -C genomes/NC_008253.fna e_coli_c") && die;
} else {
	print STDERR "Colorspace e_coli index already present...\n";
}

sub btrun {
	my ($args, $exseq, $exqual) = @_;
	for my $bt ($bowtie, $bowtie_d) {
		my $cmd = "$bt -c -C $args";
		print "$cmd\n";
		open BTIE, "$cmd |" || die;
		while(<BTIE>) {
			print $_;
			my @s = split;
			my ($seq, $quals) = ($s[4], $s[5]);
			$seq eq $exseq || die "Sequence didn't match expected sequence:\n     got: $seq\nexpected: $exseq\n";
			$quals eq $exqual || die "Qualities didn't match expected qualities:\n     got: $quals\nexpected: $exqual\n";
		}
		$? == 0 || die "$bt returned non-zero status $?\n";
	}
	print "PASSED\n";
}

my @cases = (
	"2112233000030311222131111011,ACAGATAAAAATTACAGAGTACACAAC,qqqqqqqqqqqqqqqqqqqqqqqqqqq,e_coli_c, ",
	"2112233011030311222131111011,ACAGATAACAATTACAGAGTACACAAC,qqqqqqqqqqqqqqqqqqqqqqqqqqq,e_coli_c, ",
	"2112233000030311222..1111011,ACAGATAAAAATTACAGAGTACACAAC,qqqqqqqqqqqqqqqqqqI!Iqqqqqq,e_coli_c, ",
	"2112233000030311222...111011,ACAGATAAAAATTACAGAGTACACAAC,qqqqqqqqqqqqqqqqqqI!!Iqqqqq,e_coli_c,-v 3",
	"2112233010030311222131111011,ACAGATAAAAATTACAGAGTACACAAC,qqqqqqq!!qqqqqqqqqqqqqqqqqq,e_coli_c, ",
	"2112233000030311222311111011,ACAGATAAAAATTACAGAGCACACAAC,qqqqqqqqqqqqqqqqqqqqqqqqqqq,e_coli_c, "
);

for my $c (@cases) {
	my @cs = split(/,/, $c);
	$#cs == 4 || die;
	my ($read, $exseq, $exqual, $index, $args) = @cs;
	btrun("$index $args -c $read", $exseq, $exqual);
}