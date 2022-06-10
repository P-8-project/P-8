TEMPLATE = aux

include($$OUT_PWD/../../conf.pri)

# generate p-8.conf for installation

p-8_conf_inst.target = p-8.conf.inst
p-8_conf_inst.commands = sed -e \"s,configdir=.*,configdir=$$CONFIGDIR/runner,g\" -e \"s,rundir=.*,rundir=$$RUNDIR,g\" -e \"s,logdir=.*,logdir=$$LOGDIR,g\" ../../examples/config/p-8.conf > p-8.conf.inst
p-8_conf_inst.depends = ../../examples/config/p-8.conf

QMAKE_EXTRA_TARGETS += p-8_conf_inst
PRE_TARGETDEPS += p-8.conf.inst

# install general lib files

libfiles.path = $$LIBDIR
libfiles.files = internal.conf

# install config files

routes.path = $$CONFIGDIR
routes.extra = test -e $(INSTALL_ROOT)$$routes.path/routes || cp -f ../../examples/config/routes $(INSTALL_ROOT)$$routes.path/routes

p-8conf.path = $$CONFIGDIR
p-8conf.extra = test -e $(INSTALL_ROOT)$$p-8conf.path/p-8.conf || cp -f p-8.conf.inst $(INSTALL_ROOT)$$p-8conf.path/p-8.conf

INSTALLS += libfiles routes p-8conf
