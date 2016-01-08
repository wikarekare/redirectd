all: redirectd

redirectd: redirectd.c
	cc -o redirectd redirectd.c

clean:
	rm -f *.o *.a redirectd

install: all
	install redirectd /usr/local/sbin
