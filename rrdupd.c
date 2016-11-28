/* Copyright (c) 2014, 2016 Alexander Graf and André Koch-Kramer.
 * All rights reserved. */

#include <assert.h>
#include <linux/major.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <rrd.h>


#define HEARTBEAT	120

/* uncomment this if you want to measure temperature. */
//#define HAVE_TEMP
#define TEMPSENSOR	"/sys/devices/virtual/thermal/thermal_zone0/temp"
#define TEMPSAMPLES	8


#define prefmatch(str, prefix) !strncmp(str, prefix, strlen(prefix))

#ifdef HAVE_TEMP
static int temp_sum;
#endif

#if !defined(LVM_BLK_MAJOR)
#define LVM_BLK_MAJOR	58
#endif

#define DM_MAJOR_RAID	253
#define DM_MAJOR_CRYPT	254

typedef struct list {
	char *devname;
	char is_device;
	struct list *next;
} *devicelist;

static devicelist devices = NULL;

static devicelist insert(devicelist list, char *device, char is_device) {
	devicelist tmplist = (devicelist) malloc(sizeof(struct list));
	if (list == NULL) {
		tmplist->next = NULL;
	} else {
		tmplist->next = list;
	}
	int size = strlen(device) + 1;
	tmplist->devname = (char*) malloc(size);
	strncpy(tmplist->devname, device, size);
	tmplist->is_device = is_device;
	return tmplist;
}

static char is_disk(char *dev) {
	char devpath[27] =  "/sys/block/";
	strncat(devpath, dev, 27);
	devicelist tmplist = devices;
	while (tmplist != NULL) {
		if (!strcmp(tmplist->devname, dev))
			break;
		tmplist = tmplist->next;
	}
	if (tmplist != NULL)
		return tmplist->is_device;
	else {
		char is_device = !access(devpath, F_OK);
		devices = insert(devices, dev, is_device);
		return is_device;
	}
}

static void update()
{
	FILE *proc;
	unsigned long vm_MemTotal = 0, vm_MemFree = 0, vm_Buffers = 0,
		      vm_Cached = 0, vm_SwapCached = 0, vm_Shmem = 0,
                      vm_PageTables = 0, vm_KernelStack = 0, vm_Slab = 0,
		      vm_AnonPages = 0, vm_SwapTotal = 0, vm_SwapFree = 0;
	int i, j;
	char buf[256];
	char *argv[4];
	unsigned long long cp_times[7], cp_times_diff[7], cp_times_dsum;
	static unsigned long long last_cp_times[7];
	static int last_valid;
	float cp_times_dn[7];
	float loadavg5, loadavg15;
	char devname[16];
	unsigned long long ibytes, obytes;
	unsigned long long ibytes_d, obytes_d;
	static unsigned long long last_ibytes, last_obytes;
	unsigned int major, minor;
	unsigned long long reads, writes, reads_d, writes_d;
	static unsigned long long last_reads, last_writes;


	/* CPU usage */
	proc = fopen("/proc/stat", "r");
	i = fscanf(proc, "%*s %Lu %Lu %Lu %Lu %Lu %Lu %Lu",
		   &cp_times[0], &cp_times[1], &cp_times[2], &cp_times[3],
		   &cp_times[4], &cp_times[5], &cp_times[6]);
	assert(i == 7);
	fclose(proc);
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
			cp_times_dn[i] = (float) cp_times_diff[i] / cp_times_dsum;
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
		} else if (prefmatch(buf, "SwapCached:")) {
			i++;
			vm_SwapCached = atoi(buf + sizeof("SwapCached:"));
		} else if (prefmatch(buf, "Shmem:")) {
			i++;
			vm_Shmem = atoi(buf + sizeof("Shmem:"));
		} else if (prefmatch(buf, "PageTables:")) {
			i++;
			vm_PageTables = atoi(buf + sizeof("PageTables:"));
		} else if (prefmatch(buf, "KernelStack:")) {
			i++;
			vm_KernelStack = atoi(buf + sizeof("KernelStack:"));
		} else if (prefmatch(buf, "Slab:")) {
			i++;
			vm_Slab = atoi(buf + sizeof("Slab:"));
		} else if (prefmatch(buf, "AnonPages:")) {
			i++;
			vm_AnonPages = atoi(buf + sizeof("AnonPages:"));
		} else if (prefmatch(buf, "SwapTotal:")) {
			i++;
			vm_SwapTotal = atoi(buf + sizeof("SwapTotal:"));
		} else if (prefmatch(buf, "SwapFree:")) {
			i++;
			vm_SwapFree = atoi(buf + sizeof("SwapFree:"));
		}
		if (i == 12)
			break;
	}
	fclose(proc);
	assert(i == 12);
	// tautological assert:
	/*assert((vm_MemTotal - (vm_MemFree + vm_Buffers + vm_Cached + vm_Shmem)) +
		(vm_MemFree - vm_Shmem) + vm_Shmem + vm_Buffers +
		(vm_Cached + vm_Shmem) == vm_MemTotal);
	*/
	snprintf(buf, sizeof(buf), "N:%lu:%lu:%lu:%lu:%lu:%lu:%lu",
		 1024 * vm_AnonPages,
		 1024 * (vm_PageTables + vm_KernelStack),
		 1024 * vm_Slab,
		 1024 * vm_Buffers,
		 1024 * (vm_Cached + vm_SwapCached - vm_Shmem),
		 1024 * vm_Shmem,
		 1024 * (vm_MemTotal - (vm_MemFree + vm_AnonPages +
					vm_PageTables + vm_KernelStack + vm_Slab +
					vm_Buffers + vm_Cached + vm_SwapCached)));
	argv[0] = "update";
	argv[1] = "mem.rrd";
	argv[2] = buf;
	argv[3] = 0;
	rrd_update(3, argv);
	snprintf(buf, sizeof(buf), "N:%li", vm_SwapTotal - vm_SwapFree);
	argv[0] = "update";
	argv[1] = "swap.rrd";
	argv[2] = buf;
	argv[3] = 0;
	rrd_update(3, argv);

	/* load average */
	proc = fopen("/proc/loadavg", "r");
	i = fscanf(proc, "%*f %f %f", &loadavg5, &loadavg15);
	assert(i == 2);
	fclose(proc);
	snprintf(buf, sizeof(buf), "N:%.6f:%.6f", loadavg5, loadavg15);
	argv[0] = "update";
	argv[1] = "loadavg.rrd";
	argv[2] = buf;
	argv[3] = 0;
	rrd_update(3, argv);

#ifdef HAVE_TEMP
	/* cpu temperature */
	snprintf(buf, sizeof(buf), "N:%.3f",
		 temp_sum / 1000.0 / (float) TEMPSAMPLES);
	temp_sum = 0;
	argv[0] = "update";
	argv[1] = "temp.rrd";
	argv[2] = buf;
	argv[3] = 0;
	rrd_update(3, argv);
#endif

	/* network usage */
	proc = fopen("/proc/net/dev", "r");
	i = 0;
	ibytes = obytes = 0;
	while (fgets(buf, sizeof(buf), proc)) {
		if (i++ < 2)
			/* skip first two lines */
			continue;
		j = sscanf(buf, "%15s %Lu %*d %*d %*d %*d %*d %*d %*d %Lu",
			   devname, &ibytes_d, &obytes_d);
		assert(j == 3);
		if (!strcmp(devname, "lo:"))
			/* skip loopback interface */
			continue;
		ibytes += ibytes_d;
		obytes += obytes_d;
	}
	fclose(proc);
	if (last_valid &&
	    ibytes >= last_ibytes &&
	    obytes >= last_obytes) {
		snprintf(buf, sizeof(buf), "N:%Lu:%Lu",
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
	proc = fopen("/proc/diskstats", "r");
	reads = writes = 0;
	while (fgets(buf, sizeof(buf), proc)) {
		j = sscanf(buf, "%u %u %s %*u %*u %Lu %*u %*u %*u %Lu", &major,
			   &minor, devname, &reads_d, &writes_d);
		if (j == 5 && major != LVM_BLK_MAJOR && major != NBD_MAJOR
		    && major != RAMDISK_MAJOR && major != LOOP_MAJOR
		    && major != DM_MAJOR_CRYPT && major != DM_MAJOR_RAID) {
			if (is_disk(devname)) {
				reads += reads_d;
				writes += writes_d;
			}
		}
	}
	fclose(proc);
	if (last_valid &&
	    reads >= last_reads &&
	    writes >= last_writes) {
		snprintf(buf, sizeof(buf), "N:%Lu:%Lu",
			 4096*(reads-last_reads)/HEARTBEAT,
			 4096*(writes-last_writes)/HEARTBEAT);
		argv[0] = "update";
		argv[1] = "disk.rrd";
		argv[2] = buf;
		argv[3] = 0;
		rrd_update(3, argv);
	}
	last_reads = reads;
	last_writes = writes;

	last_valid = 1;
}

#ifdef HAVE_TEMP
static void measure_temp()
{
	FILE *proc;
	int i, temp;

	proc = fopen(TEMPSENSOR, "r");
	if (proc == NULL) {
		fprintf(stderr, "Adjust TEMPSENSOR define in rrdupd.c!\n");
		exit(1);
	}
	i = fscanf(proc, "%d", &temp);
	assert(i == 1);
	fclose(proc);

	temp_sum += temp;
}
#endif

int main(int argc, char **argv)
{
	struct timespec tp;
	time_t nextrun;
#ifdef HAVE_TEMP
	int cnt = 0;
#endif

	/* go to directory where binary resides, as we expect rrd files there */
	assert(argc == 1);
	if (chdir(dirname(argv[0])) != 0)
		perror("chdir");

	clock_gettime(CLOCK_MONOTONIC, &tp);
	nextrun = tp.tv_sec;

	while (1) {
#ifdef HAVE_TEMP
		nextrun += HEARTBEAT / TEMPSAMPLES;
		measure_temp();

		if (++cnt == TEMPSAMPLES) {
			update();
			cnt = 0;
		}
#else
		nextrun += HEARTBEAT;
		update();
#endif

		clock_gettime(CLOCK_MONOTONIC, &tp);
		if (nextrun - tp.tv_sec > 0)
			sleep(nextrun - tp.tv_sec);
	}
}
