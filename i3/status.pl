#!/usr/bin/env perl

# Adapted from https://github.com/i3/i3status/blob/master/contrib/wrapper.pl

# This script is a simple wrapper which prefixes each i3status line with custom
# information. To use it, ensure your ~/.i3status.conf contains this line:
#     output_format = "i3bar"
# in the 'general' section.
# Then, in your ~/.i3/config, use:
#     status_command i3status | ~/i3status/contrib/wrapper.pl
# In the 'bar' section.

use strict;
use warnings;
use JSON;

# Get initial money amount
my $money = `freeagent get-daily-total`;

# Don’t buffer any output.
$| = 1;

# Skip the first line which contains the version header.
print scalar <STDIN>;

# The second line contains the start of the infinite array.
print scalar <STDIN>;

# Read lines forever, ignore a comma at the beginning if it exists.
while (my ($statusline) = (<STDIN> =~ /^,?(.*)/)) {
    # Decode the JSON-encoded line.
    my @blocks = @{decode_json($statusline)};

    my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime();

    # Update daily total every minute
    if ($sec == 0) {
        $money = `freeagent get-daily-total`;
    }

    # Prefix our own information (you could also suffix or insert in the
    # middle).
    @blocks = ({
        full_text => "\x{A3} " . $money,
        name => 'money',
        color => ''
    }, @blocks);

    # Output the line as JSON.
    print encode_json(\@blocks) . "\n,";
}
