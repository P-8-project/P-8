TEMPLATE = subdirs

m2adapter.subdir = m2adapter
proxy.subdir = proxy
handler.subdir = handler
tools.subdir = tools/publish

SUBDIRS += \
	m2adapter \
	proxy \
	handler \
	tools

include($$OUT_PWD/conf.pri)

runnerlibfiles.path = $$LIBDIR/runner
runnerlibfiles.files = runner/processmanager.py runner/services.py runner/runner.py

runnerconfigfiles.path = $$CONFIGDIR/runner
runnerconfigfiles.files = runner/mongrel2.conf.template runner/m2adapter.conf.template runner/zurl.conf.template runner/certs

runnerbinfiles.path = $$BINDIR
runnerbinfiles.extra = cp -f p-8.inst $(INSTALL_ROOT)$$runnerbinfiles.path/p-8

p-8configfiles.path = $$CONFIGDIR
p-8configfiles.files = examples/config/internal.conf

routes.path = $$CONFIGDIR
routes.extra = test -e $(INSTALL_ROOT)$$routes.path/routes || cp -f examples/config/routes $(INSTALL_ROOT)$$routes.path/routes

p-8conf.path = $$CONFIGDIR
p-8conf.extra = test -e $(INSTALL_ROOT)$$p-8conf.path/p-8.conf || cp -f p-8.conf.inst $(INSTALL_ROOT)$$p-8conf.path/p-8.conf

INSTALLS += runnerlibfiles runnerconfigfiles runnerbinfiles p-8configfiles routes p-8conf
