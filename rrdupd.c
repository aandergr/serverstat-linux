/* Copyright (c) 2014 Alexander Graf.  All rights reserved. */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <rrd.h>


#define HOMEDIR		"/home/ich/serverstat-linux"
#define HEARTBEAT	120


#define prefmatch(str, prefix) !strncmp(str, prefix, strlen(prefix))

static void update()
{
	// Not yet needed
	//static int last_valid;

	FILE *proc;
	unsigned int vm_MemTotal = 0, vm_MemFree = 0, vm_Buffers = 0,
		     vm_Cached = 0, vm_Shmem = 0;
	int i;
	char buf[64];
	char *argv[4];
	unsigned int cp_times[7], cp_times_diff[7], cp_times_dsum;
	static unsigned int last_valid, last_cp_times[7];
	double cp_times_dn[7];



	/* CPU usage */
	proc = fopen("/proc/stat", "r");
	fscanf(proc, "%*s %d %d %d %d %d %d %d", &cp_times[0], &cp_times[1], &cp_times[2], &cp_times[3], &cp_times[4], &cp_times[5], &cp_times[6]);
        if (last_valid) {
		for (i = 0; i < 7; i++) {
			if (last_cp_times[i] > cp_times[i])
				goto cpuovfl;
			cp_times_diff[i] = cp_times[i] - last_cp_times[i];
		}
		cp_times_dsum = 0;
		for (i = 0; i < 7; i++) {
			cp_times_dsum += cp_times_diff[i];
		}
		for (i = 0; i < 7; i++) {
			cp_times_dn[i] = (double) cp_times_diff[i] / cp_times_dsum;
		}
		snprintf(buf, sizeof(buf), "N:%.6f:%.6f:%.6f:%.6f:%.6f:%.6f",
			 cp_times_dn[0], cp_times_dn[1], cp_times_dn[2],
			 cp_times_dn[4], cp_times_dn[5], cp_times_dn[6]);
		argv[0] = "update";
		argv[1] = "cpu.rrd";
		argv[2] = buf;
		argv[3] = 0;
		rrd_update(3, argv);
        }
cpuovfl:
	memcpy(last_cp_times, cp_times, sizeof(cp_times));
	
	
	
	/* VM stats */
	proc = fopen("/proc/meminfo", "r");
	i = 0;
	while (fgets(buf, sizeof(buf), proc)) {
		if (prefmatch(buf, "MemTotal:")) {
			i++;
			vm_MemTotal = atoi(buf + sizeof("MemTotal:"));
		} else if (prefmatch(buf, "MemFree:")) {
			i++;
			vm_MemFree = atoi(buf + sizeof("MemFree:"));
		} else if (prefmatch(buf, "Buffers:")) {
			i++;
			vm_Buffers = atoi(buf + sizeof("Buffers:"));
		} else if (prefmatch(buf, "Cached:")) {
			i++;
			vm_Cached = atoi(buf + sizeof("Cached:"));
		} else if (prefmatch(buf, "Shmem:")) {
			i++;
			vm_Shmem = atoi(buf + sizeof("Shmem:"));
		}
		if (i == 5)
			break;
	}
	fclose(proc);
	assert(i == 5);
	assert((vm_MemTotal - (vm_MemFree + vm_Buffers + vm_Cached + vm_Shmem)) +
	       (vm_MemFree - vm_Shmem) + vm_Shmem + vm_Buffers +
	       (vm_Cached + vm_Shmem) == vm_MemTotal);
	snprintf(buf, sizeof(buf), "N:%u:%u:%u:%u",
		 1024 * (vm_MemTotal - (vm_MemFree + vm_Buffers + vm_Cached + vm_Shmem)),
		 1024 * vm_Shmem,
		 1024 * vm_Buffers,
		 1024 * (vm_Cached + vm_Shmem));
	argv[0] = "update";
	argv[1] = "mem.rrd";
	argv[2] = buf;
	argv[3] = 0;
	rrd_update(3, argv);

	/* load average */
	#if 0
	getloadavg(loadavg, 3);
	snprintf(buf, sizeof(buf), "N:%.6f:%.6f",
		 loadavg[1], loadavg[2]);
	argv[0] = "update";
	argv[1] = "loadavg.rrd";
	argv[2] = buf;
	argv[3] = 0;
	rrd_update(3, argv);
	#endif

	/* cpu temperature */
	#if 0
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
	#endif

	/* swap usage */
	#if 0
	kvm_getswapinfo(kd, &kvm_swap, 1, 0);
	snprintf(buf, sizeof(buf), "N:%li", ((long) kvm_swap.ksw_used)*4);
	argv[0] = "update";
	argv[1] = "swap.rrd";
	argv[2] = buf;
	argv[3] = 0;
	rrd_update(3, argv);
	#endif

	/* network usage */
	#if 0
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
	#endif

	/* disk I/O */
	#if 0
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
	#endif

	last_valid = 1;
}

int main()
{
	struct timespec tp;
	time_t nextrun;

	if (chdir(HOMEDIR) != 0)
		perror(HOMEDIR);


	clock_gettime(CLOCK_MONOTONIC, &tp);
	nextrun = tp.tv_sec;

	while (1) {
		update();
		clock_gettime(CLOCK_MONOTONIC, &tp);
		nextrun += HEARTBEAT;
		if (nextrun - tp.tv_sec > 0)
			sleep(nextrun - tp.tv_sec);
	}
}
