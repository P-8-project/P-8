TEMPLATE = subdirs

src.subdir = src

tools.subdir = tools
tools.depends = src

SUBDIRS += src tools
