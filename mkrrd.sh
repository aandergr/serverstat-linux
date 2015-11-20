#!/bin/sh

# Copyright (c) 2011, 2014 Alexander Graf.  All rights reserved.

rrdtool create loadavg.rrd -s 120\
	DS:la5:GAUGE:240:0:U	\
	DS:la15:GAUGE:240:0:U	\
	RRA:AVERAGE:0.5:1:720	\
	RRA:AVERAGE:0.5:15:336	\
	RRA:AVERAGE:0.5:180:224	\
	RRA:AVERAGE:0.5:720:365

rrdtool create traffic.rrd -s 120\
	DS:rx:GAUGE:240:0:U	\
	DS:tx:GAUGE:240:0:U	\
	RRA:AVERAGE:0.5:1:720	\
	RRA:AVERAGE:0.5:15:336	\
	RRA:AVERAGE:0.5:180:224	\
	RRA:AVERAGE:0.5:720:365

rrdtool create mem.rrd -s 120	\
	DS:used:GAUGE:240:0:U	\
	DS:shared:GAUGE:240:0:U	\
	DS:buffers:GAUGE:240:0:U\
	DS:cache:GAUGE:240:0:U	\
	RRA:AVERAGE:0.5:1:720	\
	RRA:AVERAGE:0.5:15:336	\
	RRA:AVERAGE:0.5:180:224	\
	RRA:AVERAGE:0.5:720:365

#rrdtool create swap.rrd -s 120	\
#	DS:used:GAUGE:240:0:U	\
#	RRA:AVERAGE:0.5:1:720	\
#	RRA:AVERAGE:0.5:15:336	\
#	RRA:AVERAGE:0.5:180:224	\
#	RRA:AVERAGE:0.5:720:365
#
#rrdtool create temp.rrd -s 120	\
#	DS:core:GAUGE:240:0:99	\
#	RRA:AVERAGE:0.5:1:720	\
#	RRA:AVERAGE:0.5:15:336	\
#	RRA:AVERAGE:0.5:180:224	\
#	RRA:AVERAGE:0.5:720:365
#
rrdtool create cpu.rrd -s 120	\
	DS:user:GAUGE:240:0:1\
	DS:nice:GAUGE:240:0:1\
	DS:system:GAUGE:240:0:1	\
	DS:iowait:GAUGE:240:0:1	\
	DS:irq:GAUGE:240:0:1	\
	DS:softirq:GAUGE:240:0:1	\
	RRA:AVERAGE:0.5:1:720	\
	RRA:AVERAGE:0.5:15:336	\
	RRA:AVERAGE:0.5:180:224	\
	RRA:AVERAGE:0.5:720:365

#rrdtool create disk.rrd -s 120	\
#	DS:r0:GAUGE:240:0:U	\
#	DS:r1:GAUGE:240:0:U	\
#	DS:w0:GAUGE:240:0:U	\
#	DS:w1:GAUGE:240:0:U	\
#	RRA:AVERAGE:0.5:1:720	\
#	RRA:AVERAGE:0.5:15:336	\
#	RRA:AVERAGE:0.5:180:224	\
#	RRA:AVERAGE:0.5:720:365
