CFLAGS+=-I/usr/local/include
LDFLAGS+=-L/usr/local/lib -lrrd -lkvm -ldevstat

rrdupd: rrdupd.c
