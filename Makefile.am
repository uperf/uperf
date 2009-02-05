## Process this file with automake to produce Makefile.in

SUBDIRS = workloads src
TODAY = `date +%m%d%y`

dist-hook: releaseinfo

releaseinfo:
	echo $(TODAY)
	@sed -e 's/version/'$(VERSION)'/'  -e 's/builddate/'$(TODAY)'/' release.template > release
	cp release $(distdir)/release
