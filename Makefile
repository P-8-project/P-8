prefix = /usr/local
varprefix = /var/local
configdir = $(prefix)/etc/p-8
bindir = $(prefix)/bin
libdir = $(prefix)/lib/p-8
rundir = $(varprefix)/run/p-8
logdir = $(varprefix)/log/p-8

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

install:
	mkdir -p $(bindir)
	mkdir -p $(configdir)
	mkdir -p $(configdir)/runner
	mkdir -p $(configdir)/runner/certs
	mkdir -p $(libdir)/handler
	mkdir -p $(libdir)/runner
	mkdir -p $(rundir)
	mkdir -p $(logdir)
	cp m2adapter/m2adapter $(bindir)
	cp proxy/p-8-proxy $(bindir)
	cp handler/p-8-handler $(bindir)
	cp handler/*.py $(libdir)/handler
	cp -a runner/*.py $(libdir)/runner
	cp -a runner/*.conf runner/*.template $(configdir)/runner
	sed -e "s,^default_config_dir =.*,default_config_dir = \"$(configdir)\",g" p-8 > $(bindir)/p-8
	sed -e "s,libdir=.*,libdir=$(libdir),g" -e "s,configdir=.*,configdir=$(configdir)/runner,g" -e "s,rundir=.*,rundir=$(rundir),g" -e "s,logdir=.*,logdir=$(logdir),g" config/p-8.conf.example > $(configdir)/p-8.conf
	cp config/routes.example $(configdir)/routes
