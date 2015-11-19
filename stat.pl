#!/usr/bin/env perl

# Copyright (c) 2011, 2014 Alexander Graf.  All rights reserved.

use strict;
use warnings;

use CGI 'param','header';

my $rrdtool_exe = "/usr/local/bin/rrdtool";
my $locgraphpath = "/home/serverstat/scripts";

my @ranges = (
	{
		'name' => "Tages",
		'sec' => 86400
	},
	{
		'name' => "Woche",
		'sec' => 604800
	},
	{
		'name' => "Acht Wochen",
		'sec' => 4838400
	},
	{
		'name' => "Jahres",
		'sec' => 31536000
	}
);

my @graphs = (
	{
		'name' => "loadavg",
		'title' => "Load Average",
		'create' => "-v \"Threads\" -l 0 DEF:la5=loadavg.rrd:la5:AVERAGE DEF:la15=loadavg.rrd:la15:AVERAGE LINE:la5\#ff0000:\"Fünf Minuten\" LINE:la15\#0000ff:\"15 Minuten\""
	},

	{
		'name' => "cpu",
		'title' => "CPU Usage",
		'create' => "-v \"Used CPUs\" -l 0 DEF:u0=cpu.rrd:user0:AVERAGE DEF:u1=cpu.rrd:user1:AVERAGE DEF:n0=cpu.rrd:nice0:AVERAGE DEF:n1=cpu.rrd:nice1:AVERAGE DEF:s0=cpu.rrd:sys0:AVERAGE DEF:s1=cpu.rrd:sys1:AVERAGE DEF:i0=cpu.rrd:ir0:AVERAGE DEF:i1=cpu.rrd:ir1:AVERAGE AREA:u0\#ff0000:\"user0\" AREA:u1\#ff7f00:\"user1\":STACK AREA:n0\#7fff00:\"nice0\":STACK AREA:n1\#00ff00:\"nice1\":STACK AREA:s0\#00ffff:\"sys0\":STACK AREA:s1\#007fff:\"sys1\":STACK AREA:i0\#7f00ff:\"ir0\":STACK AREA:i1\#ff00ff:\"ir1\":STACK"
	},

	{
		'name' => "temp",
		'title' => "CPU Temperature",
		'create' => "-v \"deg. Celsius\" -g -A DEF:c0=temp.rrd:core:AVERAGE LINE:c0\#ff0000:\"cpu\""
	},

	{
		'name' => "disk",
		'title' => "Disk I/O",
		'create' => "-v \"Bits/Second\" DEF:r0=disk.rrd:r0:AVERAGE DEF:r1=disk.rrd:r1:AVERAGE DEF:w0=disk.rrd:w0:AVERAGE DEF:w1=disk.rrd:w1:AVERAGE CDEF:r0b=r0,8,* CDEF:r1b=r1,8,* CDEF:w0b=w0,-8,* CDEF:w1b=w1,-8,* AREA:r0b\#ff0000:\"read0\" AREA:r1b\#ffff00:\"read1\":STACK HRULE:0#000000 AREA:w0b\#00ffff:\"write0\" AREA:w1b\#0000ff:\"write1\":STACK"
	},
	{
		'name' => "traffic",
		'title' => "Network RX/TX",
		'create' => "-v \"Bits/Second\" DEF:rx=traffic.rrd:rx:AVERAGE DEF:tx=traffic.rrd:tx:AVERAGE CDEF:rxb=rx,-8,* CDEF:txb=tx,8,* AREA:txb\#ff0000:\"Upload\" AREA:rxb\#00ffff:\"Download\" HRULE:0#000000"
	},


	{
		'name' => "mem",
		'title' => "RAM Usage",
		'create' => "-v \"Bytes\" -b 1024 -r -l 0 -u 4294967296 DEF:active=mem.rrd:active:AVERAGE DEF:inact=mem.rrd:inact:AVERAGE DEF:wired=mem.rrd:wired:AVERAGE DEF:cache=mem.rrd:cache:AVERAGE AREA:active\#ff0000:\"active\" AREA:inact#7fff00:\"inactive\":STACK AREA:wired\#00ffff:\"wired\":STACK AREA:cache\#7f00ff:\"cache\":STACK"
	},

	{
		'name' => "swap",
		'title' => "Swap",
		'create' => "-v \"Bytes\" -b 1024 -g -l 0 DEF:usedb=swap.rrd:used:AVERAGE CDEF:used=usedb,1024,* LINE:used\#ff0000:\"Benutzt\""
	},



);

my $r;

$r = int(param('r'));
$r = 0 unless $r;
$r = 0 if $r < 0;
$r = 0 unless $ranges[$r];

for my $i ( 0 .. $#graphs ) {
	system "$rrdtool_exe graph $locgraphpath/$graphs[$i]{'name'}$r.png --start -$ranges[$r]{'sec'} -w 720 -h 200 $graphs[$i]{'create'} >/dev/null";
}

print header;

print "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
print "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.1//EN\" \"http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd\">\n";
print "<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"en\">";

print "<head>";
print "<title>Serverstatistik</title>";
print "<meta http-equiv=\"Content-Type\" content=\"text/html;charset=utf-8\"/>";
print "</head>";

print "<body>";
print "<h1>Serverstatistik</h1>";
#print "<p>Copyright (c) 2011, 2014 Alexander Graf.</p>";

print "<pre>"; system("uptime; /sbin/sysctl dev.cpu.0.temperature dev.cpu.1.temperature; df -h"); print "</pre>";

print "<p>Zeige Zeitraum des/der <b>letzten</b>: ";

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
