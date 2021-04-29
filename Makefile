all: make-p-8-proxy

clean:
	if [ -f proxy/Makefile ]; then cd proxy && make clean; fi

distclean:
	if [ -f proxy/Makefile ]; then cd proxy && make distclean; fi
	rm -f proxy/conf.pri

make-p-8-proxy: proxy/conf.pri
	cd proxy && make

proxy/conf.pri:
	cd proxy && ./configure
