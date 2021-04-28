import sys
import os
import time
import signal
import ConfigParser
import services

config_file = "/etc/p-8/p-8.conf"
for arg in sys.argv:
	if arg.startswith("--config="):
		config_file = arg[9:]
		break

config = ConfigParser.ConfigParser()
config.read([config_file])

configdir = config.get("runner", "configdir")
if not os.path.isabs(configdir):
	configdir = os.path.join(os.path.dirname(config_file), configdir)

service_names = config.get("runner", "services").split(",")

http_port = int(config.get("runner", "http_port"))

rundir = config.get("runner", "rundir")
if not os.path.isabs(rundir):
	rundir = os.path.join(os.path.dirname(config_file), rundir)

logdir = config.get("runner", "logdir")
if not os.path.isabs(logdir):
	logdir = os.path.join(os.path.dirname(config_file), logdir)

exedir = os.path.dirname(os.path.realpath(__file__))

proxybin = "p-8-proxy"
path = os.path.normpath(os.path.join(exedir, "../proxy/p-8-proxy"))
if os.path.isfile(path):
	proxybin = path

handlerbin = "p-8-handler.py"
path = os.path.normpath(os.path.join(exedir, "../handler/p-8-handler.py"))
if os.path.isfile(path):
	handlerbin = path

service_objs = list()
for name in service_names:
	if name == "mongrel2":
		service_objs.append(services.Mongrel2Service("mongrel2", os.path.join(configdir, "mongrel2.conf.template"), http_port, configdir, rundir, logdir))
	elif name == "zurl":
		service_objs.append(services.ZurlService("zurl", os.path.join(configdir, "zurl.conf"), rundir, logdir))
	elif name == "p-8-proxy":
		service_objs.append(services.P-8ProxyService(proxybin, config_file, rundir, logdir))
	elif name == "p-8-handler":
		service_objs.append(services.P-8HandlerService(handlerbin, config_file, rundir, logdir))

quit = False

def termfunc(signum, frame):
	global quit
	quit = True

signal.signal(signal.SIGTERM, termfunc)
signal.signal(signal.SIGINT, termfunc)

print "starting..."

for s in service_objs:
	print "starting %s" % s.name()
	if not s.start():
		print "error starting %s" % s.name()

print "started"

while not quit:
	for s in service_objs:
		if not s.process():
			print "error processing %s" % s.name()
	time.sleep(1)

print "stopping..."

for s in service_objs:
	print "stopping %s" % s.name()
	s.stop()

while True:
	all_stopped = True
	for s in service_objs:
		if not s.process():
			print "error processing %s" % s.name()
		if not s.is_stopped():
			all_stopped = False
	if all_stopped:
		break
	time.sleep(1)

print "stopped"
