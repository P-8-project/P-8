TEMPLATE = aux

include($$OUT_PWD/../../conf.pri)

p-8_conf_inst.target = ../../p-8.conf.inst
p-8_conf_inst.commands = sed -e \"s,configdir=.*,configdir=$$CONFIGDIR/runner,g\" -e \"s,rundir=.*,rundir=$$RUNDIR,g\" -e \"s,logdir=.*,logdir=$$LOGDIR,g\" ../../examples/config/p-8.conf > ../../p-8.conf.inst
p-8_conf_inst.depends = ../../examples/config/p-8.conf

p-8_inst.target = ../../p-8.inst
p-8_inst.commands = sed -e \"s,^default_libdir = .*,default_libdir = \'$$LIBDIR\',g\" ../../p-8 | sed -e \"s,^default_configdir =.*,default_configdir = \'$$CONFIGDIR\',g\" | sed -e \"s,^version =.*,version = \'$$APP_VERSION\',g\" > ../../p-8.inst && chmod 755 ../../p-8.inst
p-8_inst.depends = ../../p-8

QMAKE_EXTRA_TARGETS += p-8_conf_inst p-8_inst

PRE_TARGETDEPS += ../../p-8.conf.inst ../../p-8.inst
