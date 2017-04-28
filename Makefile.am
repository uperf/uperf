## Process this file with automake to produce Makefile.in

SUBDIRS = workloads src tests
TODAY = `date +%m%d%y`

dist-hook: releaseinfo

releaseinfo:
	echo $(TODAY)
	@sed -e 's/version/'$(VERSION)'/'  -e 's/builddate/'$(TODAY)'/' release.template > release
	cp release $(distdir)/release
	cp server.pem $(distdir)/server.pem
	cp client.pem $(distdir)/client.pem
