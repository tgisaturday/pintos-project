# -*- perl -*-

# The expected output looks like this:
#
# (priority-fifo) iteration: 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
# (priority-fifo) iteration: 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
# (priority-fifo) iteration: 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
# (priority-fifo) iteration: 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
# (priority-fifo) iteration: 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
# (priority-fifo) iteration: 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
# (priority-fifo) iteration: 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
# (priority-fifo) iteration: 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
# (priority-fifo) iteration: 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
# (priority-fifo) iteration: 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
# (priority-fifo) iteration: 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
# (priority-fifo) iteration: 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
# (priority-fifo) iteration: 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
# (priority-fifo) iteration: 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
# (priority-fifo) iteration: 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
# (priority-fifo) iteration: 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
#
# A different permutation of 0...15 is acceptable, but every line must
# be in the same order.

use strict;
use warnings;
use tests::tests;

our ($test);
my (@output) = read_text_file ("$test.output");

common_checks ("run", @output);

my ($thread_cnt) = 16;
my ($iter_cnt) = 16;
my (@order);
my (@t) = (-1) x $thread_cnt;

my (@iterations) = grep (/iteration:/, @output);
fail "No iterations found in output.\n" if !@iterations;

#my (@numbering) = $iterations[0] =~ /(\d+)/g;
#fail "First iteration does not list exactly $thread_cnt threads.\n"
#  if @numbering != $thread_cnt;

#my (@sorted_numbering) = sort { $a <=> $b } @numbering;
#for my $i (0...$#sorted_numbering) {
#    if ($sorted_numbering[$i] != $i) {
#        fail "First iteration does not list all threads "
#          . "0...$#sorted_numbering\n";
#    }
#}

for my $i (0...$thread_cnt - 1) {
    my (@numbering) = $iterations[$i] =~ /(\d+)/g;
    fail "Iteration $i does not list exactly $iter_cnt threads.\n"
        if @numbering != $iter_cnt;
    my (@sorted_numbering) = sort { $a <=> $b } @numbering;
    fail "Iteration $i not composed by an unique thread"
        if $sorted_numbering[0] != $sorted_numbering[$thread_cnt - 1];
    fail "Priority was interleaved"
        if $sorted_numbering[0] != $thread_cnt - 1 - $i;
}

#for my $i (1...$#iterations) {
#    if ($iterations[$i] ne $iterations[0]) {
#        fail "Iteration $i differs from iteration 0\n";
#    }
#}

fail "$iter_cnt iterations expected but " . scalar (@iterations)  . " found\n"
if $iter_cnt != @iterations;

pass;

