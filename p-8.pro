TEMPLATE = subdirs

src.subdir = src
tools.subdir = tools

SUBDIRS += src tools

include($$OUT_PWD/conf.pri)

runnerlibfiles.path = $$LIBDIR/runner
runnerlibfiles.files = src/runner/processmanager.py src/runner/services.py src/runner/runner.py

runnerconfigfiles.path = $$CONFIGDIR/runner
runnerconfigfiles.files = src/runner/mongrel2.conf.template src/runner/m2adapter.conf.template src/runner/zurl.conf.template src/runner/certs

runnerbinfiles.path = $$BINDIR
runnerbinfiles.extra = cp -f p-8.inst $(INSTALL_ROOT)$$runnerbinfiles.path/p-8

p-8configfiles.path = $$CONFIGDIR
p-8configfiles.files = examples/config/internal.conf

routes.path = $$CONFIGDIR
routes.extra = test -e $(INSTALL_ROOT)$$routes.path/routes || cp -f examples/config/routes $(INSTALL_ROOT)$$routes.path/routes

p-8conf.path = $$CONFIGDIR
p-8conf.extra = test -e $(INSTALL_ROOT)$$p-8conf.path/p-8.conf || cp -f p-8.conf.inst $(INSTALL_ROOT)$$p-8conf.path/p-8.conf

INSTALLS += runnerlibfiles runnerconfigfiles runnerbinfiles p-8configfiles routes p-8conf
