#!/usr/bin/env perl

# Copyright (c) 2011, 2014 Alexander Graf.  All rights reserved.

use strict;
use warnings;

use CGI 'param','header';
use File::Basename;

my $rrdtool_exe = "/usr/bin/rrdtool";

my @ranges = (
	{
		'name' => "day",
		'sec' => 86400
	},
	{
		'name' => "week",
		'sec' => 604800
	},
	{
		'name' => "eight weeks",
		'sec' => 4838400
	},
	{
		'name' => "year",
		'sec' => 31536000
	}
);

# get total amount of RAM for appropriate scaling
my $MemTotal;
open INFILE, "< /proc/meminfo";
while (<INFILE>) {
	if (/^MemTotal: *(\d*)/) {
		$MemTotal = $1*1024;
		last;
	}
}
close INFILE;

my @graphs = (
	{
		'name' => "loadavg",
		'title' => "Load Average",
		'create' => "-v \"Threads\" -l 0 DEF:la5=loadavg.rrd:la5:AVERAGE DEF:la15=loadavg.rrd:la15:AVERAGE LINE:la5\#ff0000:\"five minutes\" LINE:la15\#00ffff:\"15 minutes\""
	},

	{
		'name' => "cpu",
		'title' => "CPU Usage",
		'create' => "-v \"CPU usage\" -r -l 0 -u 1 DEF:user=cpu.rrd:user:AVERAGE DEF:nice=cpu.rrd:nice:AVERAGE DEF:system=cpu.rrd:system:AVERAGE DEF:iowait=cpu.rrd:iowait:AVERAGE DEF:irq=cpu.rrd:irq:AVERAGE DEF:softirq=cpu.rrd:softirq:AVERAGE AREA:user\#ff0000:\"user\" AREA:nice\#ffff00:\"nice\":STACK AREA:system\#00ff00:\"system\":STACK AREA:iowait\#00ffff:\"iowait\":STACK AREA:irq\#0000ff:\"irq\":STACK AREA:softirq\#ff00ff:\"softirq\":STACK"
	},

	{
		'name' => "temp",
		'title' => "CPU Temperature",
		'create' => "-v \"deg. Celsius\" -g -A DEF:c0=temp.rrd:core:AVERAGE LINE:c0\#ff0000:\"cpu\""
	},
#
#	{
#		'name' => "disk",
#		'title' => "Disk I/O",
#		'create' => "-v \"Bits/Second\" DEF:r0=disk.rrd:r0:AVERAGE DEF:r1=disk.rrd:r1:AVERAGE DEF:w0=disk.rrd:w0:AVERAGE DEF:w1=disk.rrd:w1:AVERAGE CDEF:r0b=r0,8,* CDEF:r1b=r1,8,* CDEF:w0b=w0,-8,* CDEF:w1b=w1,-8,* AREA:r0b\#ff0000:\"read0\" AREA:r1b\#ffff00:\"read1\":STACK HRULE:0#000000 AREA:w0b\#00ffff:\"write0\" AREA:w1b\#0000ff:\"write1\":STACK"
#	},

	{
		'name' => "traffic",
		'title' => "Network RX/TX",
		'create' => "-v \"Bits/Second\" DEF:rx=traffic.rrd:rx:AVERAGE DEF:tx=traffic.rrd:tx:AVERAGE CDEF:rxb=rx,-8,* CDEF:txb=tx,8,* AREA:txb\#ff0000:\"Upload\" AREA:rxb\#00ffff:\"Download\" HRULE:0#000000"
	},

	{
		'name' => "mem",
		'title' => "RAM Usage",
		'create' => "-v \"Bytes\" -b 1024 -r -l 0 -u ${MemTotal} DEF:used=mem.rrd:used:AVERAGE DEF:shared=mem.rrd:shared:AVERAGE DEF:buffers=mem.rrd:buffers:AVERAGE DEF:cache=mem.rrd:cache:AVERAGE AREA:used\#ff0000:\"used\" AREA:shared#7fff00:\"shared\":STACK AREA:buffers\#00ffff:\"buffers\":STACK AREA:cache\#7f00ff:\"cache\":STACK"
	},

	{
		'name' => "swap",
		'title' => "Swap",
		'create' => "-v \"Bytes\" -b 1024 -g -l 0 DEF:usedb=swap.rrd:used:AVERAGE CDEF:used=usedb,1024,* LINE:used\#ff0000:\"used\""
	},



);

my $r;

$r = int(param('r'));
$r = 0 unless $r;
$r = 0 if $r < 0;
$r = 0 unless $ranges[$r];

chdir(dirname(__FILE__));

for my $i ( 0 .. $#graphs ) {
	system "$rrdtool_exe graph --lazy $graphs[$i]{'name'}$r.png --start -$ranges[$r]{'sec'} -w 720 -h 200 $graphs[$i]{'create'} >/dev/null";
}

print header;

print "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
print "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.1//EN\" \"http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd\">\n";
print "<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"en\">";

print "<head>";
print "<title>system usage statistics</title>";
print "<meta http-equiv=\"Content-Type\" content=\"text/html;charset=utf-8\"/>";
print "</head>";

print "<body>";
#print "<h1>Serverstatistik</h1>";
#print "<p>Copyright (c) 2011, 2014 Alexander Graf.</p>";

#print "<pre>"; system("uptime; echo; echo -n Temp:; cat /sys/devices/virtual/thermal/thermal_zone0/temp; echo; df -h; echo; free -tm"); print "</pre>";

print "<p>Show period of <b>last</b>: ";


for (my $range = 0; $range < @ranges; $range++) {
	print " - " unless $range == 0;
	if ($range == $r) {
		print "<b>$ranges[$range]{'name'}</b>";
	} else {
		print "<a href=\"stat.pl?r=$range\">$ranges[$range]{'name'}</a>";
	}
}

print "</p>";

for my $i ( 0 .. $#graphs ) {
	print "<h2>$graphs[$i]{'title'}</h2>";
	print "<img alt=\"$graphs[$i]{'title'}\" src=\"$graphs[$i]{'name'}$r.png\"/>";
}

print "</body>";
print "</html>";
