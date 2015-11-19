/* Copyright (c) 2014 Alexander Graf.  All rights reserved. */

#include <sys/types.h>
#include <sys/sysctl.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <rrd.h>
#include <stdlib.h>
#include <kvm.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <devstat.h>
#include <sys/devicestat.h>

#define HEARTBEAT	120

#define C_USER	0
#define C_NICE	1
#define C_SYS	2
#define C_IR	3
#define C_IDLE	4
#define CSTATES	5

/* TODO: use sysctl MIB */

static kvm_t *kd;

static void update()
{
	unsigned int v_wire_count, v_active_count, v_inactive_count, v_cache_count;
	double loadavg[3];
	int temp0, temp1;
	struct kvm_swap kvm_swap;
	struct ifaddrs *ifap, *ifa;
	u_long ibytes, obytes;
	static u_long last_ibytes, last_obytes;
	struct statinfo statinfo;
	static struct devinfo devinfo;
	static unsigned long last_dread[2], last_dwrite[2];
	long cp_times[CSTATES*2];
	static long last_cp_times[CSTATES*2];
	static int last_valid;
	long cp_times_diff[CSTATES*2];
	long cp_times_dsum[2];
	double cp_times_dn[CSTATES*2];
	size_t len;
	char *argv[4];
	char buf[128];
	int i;

	/* CPU usage */
	len = sizeof(cp_times);
	sysctlbyname("kern.cp_times", cp_times, &len, 0, 0);
	if (last_valid) {
		for (i = 0; i < CSTATES*2; i++) {
			if (last_cp_times[i] > cp_times[i])
				goto cpuovfl;
			cp_times_diff[i] = cp_times[i] - last_cp_times[i];
		}
		cp_times_dsum[0] = cp_times_dsum[1] = 0;
		for (i = 0; i < CSTATES; i++) {
			cp_times_dsum[0] += cp_times_diff[i];
			cp_times_dsum[1] += cp_times_diff[i+CSTATES];
		}
		for (i = 0; i < CSTATES-1; i++) {
			cp_times_dn[i] = (double) cp_times_diff[i] / cp_times_dsum[0];
			cp_times_dn[i+CSTATES] = (double) cp_times_diff[i+CSTATES] / cp_times_dsum[1];
		}
		snprintf(buf, sizeof(buf), "N:%.6f:%.6f:%.6f:%.6f:%.6f:%.6f:%.6f:%.6f",
			 cp_times_dn[0], cp_times_dn[CSTATES+0],
			 cp_times_dn[1], cp_times_dn[CSTATES+1],
			 cp_times_dn[2], cp_times_dn[CSTATES+2],
			 cp_times_dn[3], cp_times_dn[CSTATES+3]);
		argv[0] = "update";
		argv[1] = "cpu.rrd";
		argv[2] = buf;
		argv[3] = 0;
		rrd_update(3, argv);
	}
cpuovfl:
	memcpy(last_cp_times, cp_times, sizeof(cp_times));

	/* VM stats */
	len = sizeof(unsigned int);
	sysctlbyname("vm.stats.vm.v_wire_count", &v_wire_count, &len, 0, 0);
	sysctlbyname("vm.stats.vm.v_active_count", &v_active_count, &len, 0, 0);
	sysctlbyname("vm.stats.vm.v_inactive_count", &v_inactive_count, &len, 0, 0);
	sysctlbyname("vm.stats.vm.v_cache_count", &v_cache_count, &len, 0, 0);
	snprintf(buf, sizeof(buf), "N:%u:%u:%u:%u",
	 	 v_active_count*4096, v_inactive_count*4096,
		 v_wire_count*4096, v_cache_count*4096);
	argv[0] = "update";
	argv[1] = "mem.rrd";
	argv[2] = buf;
	argv[3] = 0;
	rrd_update(3, argv);

	/* load average */
	getloadavg(loadavg, 3);
	snprintf(buf, sizeof(buf), "N:%.6f:%.6f",
		 loadavg[1], loadavg[2]);
	argv[0] = "update";
	argv[1] = "loadavg.rrd";
	argv[2] = buf;
	argv[3] = 0;
	rrd_update(3, argv);

	/* cpu temperature */
	len = sizeof(temp0);
	sysctlbyname("dev.cpu.0.temperature", &temp0, &len, 0, 0);
	sysctlbyname("dev.cpu.1.temperature", &temp1, &len, 0, 0);
	temp0 = (temp0 - 2732) / 10;	/* Kelvin -> Celsius */
	temp1 = (temp1 - 2732) / 10;
	snprintf(buf, sizeof(buf), "N:%f", (double) (temp0 + temp1) / 2.0);
	argv[0] = "update";
	argv[1] = "temp.rrd";
	argv[2] = buf;
	argv[3] = 0;
	rrd_update(3, argv);

	/* swap usage */
	kvm_getswapinfo(kd, &kvm_swap, 1, 0);
	snprintf(buf, sizeof(buf), "N:%li", ((long) kvm_swap.ksw_used)*4);
	argv[0] = "update";
	argv[1] = "swap.rrd";
	argv[2] = buf;
	argv[3] = 0;
	rrd_update(3, argv);

	/* network usage */
	getifaddrs(&ifap);
	/* we take first entry of ifap and hope it is the right one */
	ibytes = ((struct if_data *) ifap->ifa_data)->ifi_ibytes;
	obytes = ((struct if_data *) ifap->ifa_data)->ifi_obytes;
	freeifaddrs(ifap);
	if (last_valid &&
	    ibytes >= last_ibytes &&
	    obytes >= last_obytes) {
		snprintf(buf, sizeof(buf), "N:%lu:%lu",
			 (ibytes-last_ibytes)/HEARTBEAT,
			 (obytes-last_obytes)/HEARTBEAT);
		argv[0] = "update";
		argv[1] = "traffic.rrd";
		argv[2] = buf;
		argv[3] = 0;
		rrd_update(3, argv);
	}
	last_ibytes = ibytes;
	last_obytes = obytes;

	/* disk I/O */
	statinfo.dinfo = &devinfo;
	devstat_getdevs(NULL, &statinfo);
	if (last_valid &&
	    devinfo.devices[0].bytes[DEVSTAT_READ] >= last_dread[0] &&
	    devinfo.devices[1].bytes[DEVSTAT_READ] >= last_dread[1] &&
	    devinfo.devices[0].bytes[DEVSTAT_WRITE] >= last_dwrite[0] &&
	    devinfo.devices[1].bytes[DEVSTAT_WRITE] >= last_dwrite[1]) {
		snprintf(buf, sizeof(buf), "N:%lu:%lu:%lu:%lu",
			 (devinfo.devices[0].bytes[DEVSTAT_READ]-last_dread[0]) / HEARTBEAT,
			 (devinfo.devices[1].bytes[DEVSTAT_READ]-last_dread[1]) / HEARTBEAT,
			 (devinfo.devices[0].bytes[DEVSTAT_WRITE]-last_dwrite[0]) / HEARTBEAT,
			 (devinfo.devices[1].bytes[DEVSTAT_WRITE]-last_dwrite[1]) / HEARTBEAT);
		argv[0] = "update";
		argv[1] = "disk.rrd";
		argv[2] = buf;
		argv[3] = 0;
		rrd_update(3, argv);
	}
	last_dread[0] = devinfo.devices[0].bytes[DEVSTAT_READ];
	last_dread[1] = devinfo.devices[1].bytes[DEVSTAT_READ];
	last_dwrite[0] = devinfo.devices[0].bytes[DEVSTAT_WRITE];
	last_dwrite[1] = devinfo.devices[1].bytes[DEVSTAT_WRITE];

	if (!last_valid)
		last_valid = 1;
}

int main()
{
	struct timespec tp;
	time_t nextrun;

	chdir("/home/serverstat/scripts");

	kd = kvm_open("/dev/null", "/dev/null", "/dev/null", O_RDONLY, "kvm_open");

	clock_gettime(CLOCK_MONOTONIC_FAST, &tp);
	nextrun = tp.tv_sec;

	while (1) {
		update();
		clock_gettime(CLOCK_MONOTONIC_FAST, &tp);
		nextrun += HEARTBEAT;
		if (nextrun - tp.tv_sec > 0)
			sleep(nextrun - tp.tv_sec);
	}
}
