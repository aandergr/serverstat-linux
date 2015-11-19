CFLAGS+=-O2 -march=native -pipe -Wall -Wextra
LDFLAGS+=-lrrd

rrdupd: rrdupd.c
