CFLAGS+=-O2 -march=native -pipe -Wall -Wextra
LDFLAGS+=-lrrd

rrdupd: rrdupd.c
	$(CC) -o $@ $(CFLAGS) $< $(LDFLAGS)

clean:
	rm -f rrdupd
