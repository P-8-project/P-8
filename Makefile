all: p-8

clean:
	if [ -f proxy/Makefile ]; then cd proxy && make clean; fi

distclean:
	if [ -f proxy/Makefile ]; then cd proxy && make distclean; fi
	rm -f proxy/conf.pri

p-8: proxy/p-8-proxy

proxy/p-8-proxy: proxy/conf.pri
	cd proxy && make

proxy/conf.pri:
	cd proxy && ./configure
