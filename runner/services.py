import os
import subprocess

def compile_template(infilename, outfilename, vars):
	f = open(infilename, "r")
	buf = f.read()
	f.close()
	buf = buf.replace("{{ port }}", vars["port"])
	buf = buf.replace("{{ rootdir }}", vars["rootdir"])
	f = open(outfilename, "w")
	f.write(buf)
	f.close()

class Service(object):
	def __init__(self, rundir, logdir):
		self.rundir = rundir
		self.logdir = logdir

	def name(self):
		pass

	def getlogfile(self):
		return os.path.join(self.logdir, self.name() + ".log")

	def getpidfile(self):
		return os.path.join(self.rundir, self.name() + ".pid")

	def getargs(self):
		pass

	def pre_start(self):
		pass

	def post_start(self, pid):
		pidfilename = self.getpidfile()
		if pidfilename:
			pidfile = open(pidfilename, "w")
			pidfile.write(str(pid) + "\n")
			pidfile.close()

	def post_stop(self):
		pidfilename = self.getpidfile()
		if pidfilename:
			os.remove(pidfilename)

class Mongrel2Service(Service):
	def __init__(self, binpath, configpath, port, rootdir, rundir, logdir):
		super(Mongrel2Service, self).__init__(rundir, logdir)
		self.binpath = binpath
		self.configpath = configpath
		self.port = port
		self.rootdir = rootdir

	def name(self):
		return "mongrel2"

	def getpidfile(self):
		# mongrel2 writes its own pid file
		return None

	def getargs(self):
		return [self.binpath, self.sqlconfigpath, "default"]

	def pre_start(self):
		super(Mongrel2Service, self).pre_start()

		# calculate mongrel2 relative rootdir
		absroot = os.path.abspath(self.rootdir)
		path = os.path.relpath(absroot, os.getcwd())
		if path.startswith(".."):
			raise ValueError("cannot run from deeper than %s" % absroot)

		if path.startswith("."):
			path = path[1:]
		if len(path) > 0 and not path.startswith("/"):
			path = "/" + path
		self.rootdir = path

		assert(self.configpath.endswith(".template"))
		fname = os.path.basename(self.configpath)
		path, ext = os.path.splitext(fname)
		genconfigpath = os.path.join(self.rundir, path)

		# mongrel2 will refuse to start if it sees a pidfile
		pidfilename = super(Mongrel2Service, self).getpidfile()
		if os.path.isfile(pidfilename):
			os.remove(pidfilename)

		vars = dict()
		vars["port"] = str(self.port)
		vars["rootdir"] = self.rootdir
		compile_template(self.configpath, genconfigpath, vars)

		path, ext = os.path.splitext(genconfigpath)
		self.sqlconfigpath = path + ".sqlite"

		# generate sqlite config
		subprocess.check_call(["m2sh", "load", "-config", genconfigpath, "-db", self.sqlconfigpath])

class ZurlService(Service):
	def __init__(self, binpath, configpath, verbose, rundir, logdir):
		super(ZurlService, self).__init__(rundir, logdir)
		self.binpath = binpath
		self.configpath = configpath
		self.verbose = verbose

	def name(self):
		return "zurl"

	def getargs(self):
		args = list()
		args.append(self.binpath)
		if self.verbose:
			args.append("--verbose")
		args.append("--config=%s" % self.configpath)
		return args

class P-8ProxyService(Service):
	def __init__(self, binpath, configpath, verbose, rundir, logdir):
		super(P-8ProxyService, self).__init__(rundir, logdir)
		self.binpath = binpath
		self.configpath = configpath
		self.verbose = verbose

	def name(self):
		return "p-8-proxy"

	def getargs(self):
		args = list()
		args.append(self.binpath)
		if self.verbose:
			args.append("--verbose")
		args.append("--config=%s" % self.configpath)
		return args

class P-8HandlerService(Service):
	def __init__(self, binpath, configpath, rundir, logdir):
		super(P-8HandlerService, self).__init__(rundir, logdir)
		self.binpath = binpath
		self.configpath = configpath

	def name(self):
		return "p-8-handler"

	def getargs(self):
		return [self.binpath, "--config=%s" % self.configpath]
