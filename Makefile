fsplayer: fsplayer.c
	cc -I/usr/local/include -L/usr/local/lib -lvlc -lX11 -lXxf86vm -v -o fsplayer fsplayer.c

clean:
	rm fsplayer

install:
	cp fsplayer /usr/bin
	chmod a+rx /usr/bin/fsplayer

uninstall:
	rm /usr/bin/fsplayer
