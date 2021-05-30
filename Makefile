all: make-m2adapter make-p-8-proxy

clean:
	if [ -f m2adapter/Makefile ]; then cd m2adapter && make clean; fi
	if [ -f proxy/Makefile ]; then cd proxy && make clean; fi

distclean:
	if [ -f m2adapter/Makefile ]; then cd m2adapter && make distclean; fi
	rm -f m2adapter/conf.pri m2adapter/conf.log
	if [ -f proxy/Makefile ]; then cd proxy && make distclean; fi
	rm -f proxy/conf.pri proxy/conf.log

make-m2adapter: m2adapter/conf.pri
	cd m2adapter && make

make-p-8-proxy: proxy/conf.pri
	cd proxy && make

m2adapter/conf.pri:
	cd m2adapter && ./configure

proxy/conf.pri:
	cd proxy && ./configure
