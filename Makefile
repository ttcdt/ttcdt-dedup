PREFIX=/usr/local/bin

ttcdt-dedup: ttcdt-dedup.c
	$(CC) -g -Wall $< -o $@

install:
	install ttcdt-dedup $(PREFIX)/ttcdt-dedup

uninstall:
	rm -f $(PREFIX)/ttcdt-dedup

dist: clean
	rm -f ttcdt-dedup.tar.gz && cd .. && \
        tar czvf ttcdt-dedup/ttcdt-dedup.tar.gz ttcdt-dedup/*

clean:
	rm -f ttcdt-dedup *.tar.gz *.asc
