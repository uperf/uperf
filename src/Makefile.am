## Process this file with automake to produce Makefile.in

bin_PROGRAMS = uperf

$BUILD_DATE:sh =date

uperf_SOURCES =  workorder.c strand.c execute.c flowops_library.c \
	flowops.c common.c main.c slave.c  stats.c handshake.c parse.c shm.c \
	master.c print.c signals.c goodbye.c delay.c rate.c sendfilev.c \
	logging.c netstat.c numbers.c sync.c protocol.c tcp.c generic.c \
	common.h delay.h execute.h flowops.h flowops_library.h generic.h \
	goodbye.h handshake.h hwcounter.h logging.h main.h netstat.h \
	numbers.h parse.h print.h protocol.h rate.h sendfilev.h shm.h \
	signals.h ssl.h stats.h strand.h sync.h uperf.h workorder.h

uperf_LDADD = $(UPERF_LIBS)
if HAVE_CPC
uperf_SOURCES += hwcounter.c
endif
if SCTP_C
uperf_SOURCES += sctp.c
endif
if SSL_C
uperf_SOURCES += ssl.c
endif
if UDP_C
uperf_SOURCES += udp.c
endif
if RDS_C
uperf_SOURCES +=rds.c
endif

#uperf_CPPFLAGS = -DBUILD_DATE="\"$(BUILD_DATE)\""
