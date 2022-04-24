TEMPLATE = aux

include($$OUT_PWD/../../conf.pri)

# generate p-8.conf for installation

p-8_conf_inst.target = p-8.conf.inst
p-8_conf_inst.commands = sed -e \"s,configdir=.*,configdir=$$CONFIGDIR/runner,g\" -e \"s,rundir=.*,rundir=$$RUNDIR,g\" -e \"s,logdir=.*,logdir=$$LOGDIR,g\" ../../examples/config/p-8.conf > p-8.conf.inst
p-8_conf_inst.depends = ../../examples/config/p-8.conf

QMAKE_EXTRA_TARGETS += p-8_conf_inst
PRE_TARGETDEPS += p-8.conf.inst

# generate p-8 launcher for installation
# FIXME: if runner becomes a qmake project, move this

p-8_inst.target = p-8.inst
p-8_inst.commands = sed -e \"s,^default_libdir = .*,default_libdir = \'$$LIBDIR\',g\" ../../p-8 | sed -e \"s,^default_configdir =.*,default_configdir = \'$$CONFIGDIR\',g\" | sed -e \"s,^version =.*,version = \'$$APP_VERSION\',g\" > p-8.inst && chmod 755 p-8.inst
p-8_inst.depends = ../../p-8

QMAKE_EXTRA_TARGETS += p-8_inst
PRE_TARGETDEPS += p-8.inst

# install runner files
# FIXME: if runner becomes a qmake project, move this

runnerlibfiles.path = $$LIBDIR/runner
runnerlibfiles.files = ../runner/processmanager.py ../runner/services.py ../runner/runner.py

runnerconfigfiles.path = $$CONFIGDIR/runner
runnerconfigfiles.files = ../runner/mongrel2.conf.template ../runner/m2adapter.conf.template ../runner/zurl.conf.template ../runner/certs

runnerbinfiles.path = $$BINDIR
runnerbinfiles.extra = cp -f p-8.inst $(INSTALL_ROOT)$$runnerbinfiles.path/p-8

INSTALLS += runnerlibfiles runnerconfigfiles runnerbinfiles

# install config files

p-8configfiles.path = $$CONFIGDIR
p-8configfiles.files = ../../examples/config/internal.conf

routes.path = $$CONFIGDIR
routes.extra = test -e $(INSTALL_ROOT)$$routes.path/routes || cp -f ../../examples/config/routes $(INSTALL_ROOT)$$routes.path/routes

p-8conf.path = $$CONFIGDIR
p-8conf.extra = test -e $(INSTALL_ROOT)$$p-8conf.path/p-8.conf || cp -f p-8.conf.inst $(INSTALL_ROOT)$$p-8conf.path/p-8.conf

INSTALLS += p-8configfiles routes p-8conf
