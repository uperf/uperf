# uperf configure file
# neel@sun.com
AC_INIT(uperf, 1.0.7, realneel@gmail.com)
AC_CONFIG_SRCDIR([src/flowops.c])
AC_CONFIG_HEADER([config.h])

CFLAGS="${CFLAGS=}"
UPERF_LIBS="${UPERF_LIBS=}"
#UPERF_LIBS="$UPERF_LIBS -lpthread -lrt -lm"

# Checks for programs.
AM_INIT_AUTOMAKE
AC_PROG_CC_C99
AC_PROG_RANLIB
AC_PROG_MAKE_SET

if test "x$CC" = "xcc" -a "x$(uname -s)" = "xSunOS"; then
	AC_MSG_NOTICE([*** Setting flags for Sun Compiler ***])
	CFLAGS="$CFLAGS -mt -xO3"
else
	AC_MSG_NOTICE([*** Setting flags for gcc ***])
	CFLAGS="$CFLAGS -O2 -D_REENTRANT"
fi

AM_CONDITIONAL([HAVE_PROCESSES], [true])
AM_CONDITIONAL([HAVE_CPC], [false])
AM_CONDITIONAL([HAVE_KSTAT], [false])

#enable these by default
netstat_default="yes"
udp_default="yes"
rds_default="no"
sctp_default="yes"
ssl_default="yes"
cpc_default="yes"

good_os="yes"
case "$(uname -s)" in
	FreeBSD)
		AC_MSG_NOTICE([*** FreeBSD Machine ***])
		AC_DEFINE([UPERF_FREEBSD],[1],[Setting machine os to FreeBSD])
		good_os="no"
		sctp_default="yes"
		ssl_default="yes"
		UPERF_LIBS="$UPERF_LIBS -lrt -lm -pthread"
		CFLAGS="-D_GNU_SOURCE $CFLAGS"
		;;
	Linux)
		AC_MSG_NOTICE([*** Linux Machine ***])
		AC_DEFINE([UPERF_LINUX],[1],[Setting machine os to Linux])
		good_os="no"
		sctp_default="yes"
		ssl_default="no"
		cpc_default="no"
		UPERF_LIBS="$UPERF_LIBS -lrt -lm -lpthread"
		CFLAGS="-D_GNU_SOURCE $CFLAGS"
		;;
	SunOS)
		AC_MSG_NOTICE([*** Solaris Machine ***])
		AC_DEFINE([UPERF_SOLARIS],[1],[Setting machine os to Solaris])
		UPERF_LIBS="$UPERF_LIBS -lrt -lm -lsocket -lnsl -lumem -lkstat -lsendfile"
		LIBS=$UPERF_LIBS
		;;
	Darwin)
		AC_MSG_NOTICE([*** Darwin Machine ***])
		AC_DEFINE([UPERF_DARWIN],[1],[Setting machine os to Darwin])
		good_os="no"
		sctp_default="no"
		ssl_default="no"
		UPERF_LIBS="$UPERF_LIBS -lm"
		CFLAGS="-D_GNU_SOURCE -Wno-deprecated-declarations $CFLAGS"
		;;
esac

AC_CHECK_TYPES([hrtime_t], [], [], [[#include <sys/time.h>]])
AC_CHECK_TYPES([uint64_t, uint32_t], [], [], [[#include <stdint.h>]])
# Solaris 9 and below do not support RUSAGE_LWP with getrusage
# fallback to using /proc if not defined
AC_MSG_CHECKING(if we can use getrusage)
AC_COMPILE_IFELSE([AC_LANG_SOURCE[#include <sys/resource.h>
		int i=RUSAGE_LWP;]],
		[AC_MSG_RESULT(yes)
	  	 AC_DEFINE([USE_GETRUSAGE],[1],[Have getrusage])
		],
		[
		 AC_MSG_RESULT(no)
		 ## AC_DEFINE([USE_PROC], [1], [Use proc])
		])

# Some solaris versions do not define BSWAP_* macros
AC_MSG_CHECKING(BSWAP_ macros)
AC_LINK_IFELSE([AC_LANG_SOURCE[#include <sys/byteorder.h>
		int main(int argc, char *argv[]){int i=BSWAP_32(1);}]],
		[AC_MSG_RESULT(yes)
	  	 AC_DEFINE([HAVE_BSWAP],[1],[Have BSWAP_ macros])
		],
		[AC_MSG_RESULT(no)
		])
# Linux 2.6 does not have sendfilev
AC_MSG_CHECKING(sendfilev)
AC_LINK_IFELSE([AC_LANG_SOURCE[#include <sys/sendfile.h>
		main() {int i=sendfilev(1,0, 1, 0);}]],
		[AC_MSG_RESULT(yes)
	  	 AC_DEFINE([HAVE_SENDFILEV],[1],[Have sendfilev])
		],
		[AC_MSG_RESULT(no)
		])

# Check for PTHREAD_PROCESS_SHARED. Have to run it to see if it works
# as its declared, but does not work on FreeBSD
LIBS="$UPERF_LIBS"
AC_MSG_CHECKING(if we can have working PTHREAD_PROCESS_SHARED)
process_shared_works=yes
AC_TRY_RUN([
#include <pthread.h>
#include <sys/types.h>
int
main(int argc, char *argv[])
{
	pthread_rwlockattr_t rwattr;
	pthread_rwlock_t barrier;
	pthread_mutexattr_t attr;
	pthread_mutex_t lock;

	if (pthread_mutexattr_init(&attr)) {
		return (1);
	}
	if (pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED)) {
		return (1);
	}
	if (pthread_mutex_init(&lock, &attr)) {
		return (1);
	}
	if (pthread_mutex_lock(&lock)) {
		return (1);
	}
	if (pthread_mutex_unlock(&lock)) {
		return (1);
	}
	if (pthread_rwlockattr_init(&rwattr))
		return (1);
	if (pthread_rwlockattr_setpshared(&rwattr, PTHREAD_PROCESS_SHARED))
		return (1);
	if (pthread_rwlock_init(&barrier, &rwattr))
		return (1);
	if (pthread_rwlock_wrlock(&barrier))
		return (1);
	if (pthread_rwlock_unlock(&barrier))
		return (1);
	return (0);
}],[AC_MSG_RESULT(yes)],
   [
      AC_MSG_RESULT(no)
      AC_DEFINE([STRAND_THREAD_ONLY], [1], [Use only threads])
      AM_CONDITIONAL([HAVE_PROCESSES], [false])
   ]
)
# Checks for libraries.
AC_MSG_CHECKING(whether to enable cpu performance counters support?)
AC_ARG_ENABLE(cpc,
	AC_HELP_STRING([--enable-cpc],[Turn on cpu counter support ]),,
	enable_cpc=$cpc_default)
if test "x$enable_cpc" = "xyes"; then
	AC_MSG_RESULT(yes)
	AC_CHECK_LIB([cpc], [cpc_open],
		[
     	  	#AC_MSG_RESULT(yes)
	  	AC_DEFINE([USE_CPC],[1],[Have cpu perf counters?])
	  	AC_DEFINE([USE_CPCv2],[1],[Have version2 cpu perf counters?])
	  	UPERF_LIBS="$UPERF_LIBS -lcpc -lintl"
	  	cpc="CPC version 2"
	  	AM_CONDITIONAL([HAVE_CPC], [true])
	          AC_DEFINE([HAVE_CPC],[1],[Have libcpc ])
		],
		[ #check for v1
		AC_CHECK_LIB([cpc], [cpc_version],
		 	[AC_DEFINE([USE_CPC],[1],[Have cpu perf counters?])
			 AC_DEFINE([USE_CPCv1],[1],[Have version1 cpu perf counters?])
			 UPERF_LIBS="$UPERF_LIBS -lcpc"
	  		AM_CONDITIONAL([HAVE_CPC], [true])
          		AC_DEFINE([HAVE_CPC],[1],[Have libcpc ])
	  		cpc="CPC version 1"
     	  		 #AC_MSG_RESULT(yes)
			],
	  		[ AM_CONDITIONAL([HAVE_CPC], [false])
	  	        	cpc="Not supported"
          		  AC_DEFINE([HAVE_CPC],[0],[Have libcpc ])
			]
		)
	]
	)
else
	cpc="Not enabled"
	HAVE_CPC=
	AM_CONDITIONAL([HAVE_CPC], [false])
	AC_MSG_RESULT(no)
fi
AC_CHECK_LIB([kstat], [kstat_open],
	[ AC_DEFINE([HAVE_LIBKSTAT], [1], [Have libkstat])
	  UPERF_LIBS="$UPERF_LIBS -lkstat"
	  AM_CONDITIONAL([ENABLE_NETSTAT], [true])
	],
	[ AM_CONDITIONAL([ENABLE_NETSTAT], [false])
	]
)

AC_MSG_CHECKING(whether to enable netstat support)
AC_ARG_ENABLE(netstat,
	AC_HELP_STRING([--enable-netstat], [Collect network stats? default=yes]),,
	enable_netstat=$netstat_default)
if test "x$enable_netstat" = "xyes"; then
	AM_CONDITIONAL([ENABLE_NETSTAT], [true])
	AC_DEFINE([ENABLE_NETSTAT],[1],[collect nw stats])
	AC_MSG_RESULT(yes)
else
	ENABLE_NETSTAT=
	AM_CONDITIONAL([ENABLE_NETSTAT], [false])
	AC_MSG_RESULT(no)
fi

AC_CHECK_HEADERS([atomic.h siginfo.h sys/int_limits.h sys/lwp.h signal.h sys/byteorder.h poll.h sys/poll.h sys/varargs.h stdint.h termio.h stropts.h sys/ttycom.h wait.h alloca.h sys/sendfile.h sys/types.h linux/unistd.h sys/ioctl.h sys/uio.h])

LIBS="$UPERF_LIBS"
AC_CHECK_FUNCS([nanosleep])
AC_CHECK_FUNCS([pthread_self])
AC_CHECK_FUNCS([_lwp_self])
AC_CHECK_FUNCS([strncpy])
AC_CHECK_FUNCS([strncat])
AC_CHECK_FUNCS([strlcpy])
AC_CHECK_FUNCS([strlcat])
AC_CHECK_FUNCS([gethrvtime])
AC_CHECK_FUNCS([gethrtime],[],
	[AC_CHECK_FUNCS([clock_gettime],[],
		[AC_CHECK_FUNCS([mach_absolute_time],[],
			[AC_MSG_ERROR([Could not find either gethrtime nor clock_gettime nor mach_absolute_time])])])])

AC_MSG_CHECKING(whether to enable debug mode)
AC_ARG_ENABLE(debug,
     AC_HELP_STRING([--enable-debug],[Turn on debugging]),
     [case "${enableval}" in
       yes) debug=true
     	    AC_DEFINE([DEBUG],[1],[Debug])
	    CFLAGS="$CFLAGS -g"
     	    AC_MSG_RESULT(yes)
            ;;
       no)  debug=false
     	    AC_MSG_RESULT(no)
       		;;
       *) AC_MSG_ERROR(bad value ${enableval} for --enable-debug) ;;
     esac],[debug=false; AC_MSG_RESULT(no)])
     AM_CONDITIONAL(DEBUG, test x$debug = xtrue)

#check for udp
AC_MSG_CHECKING(whether to enable udp support)
AC_ARG_ENABLE(udp,
	AC_HELP_STRING([--enable-udp], [Turn on udp support]),,
	enable_udp=$udp_default)
if test "x$enable_udp" = "xyes"; then
	AM_CONDITIONAL([UDP_C], [true])
	AC_DEFINE([HAVE_UDP],[1],[Have udp])
	AC_MSG_RESULT(yes)
else
	UDP_C=
	AM_CONDITIONAL([UDP_C], [false])
	AC_MSG_RESULT(no)
fi

#check for rds
AC_MSG_CHECKING(whether to enable rds support)
AC_ARG_ENABLE(rds,
        AC_HELP_STRING([--enable-rds], [Turn on rds support]),,
        enable_rds=$rds_default)
if test "x$enable_rds" = "xyes"; then
        AM_CONDITIONAL([RDS_C], [true])
        AC_DEFINE([HAVE_RDS],[1],[Have rds])
        AC_MSG_RESULT(yes)
else
        RDS_C=
        AM_CONDITIONAL([RDS_C], [false])
        AC_MSG_RESULT(no)
fi

AC_MSG_CHECKING(whether to enable sctp support)
AC_ARG_ENABLE(sctp,
	AC_HELP_STRING([--enable-sctp],[Turn on sctp support ]),,
	enable_sctp=$sctp_default)
if test "x$enable_sctp" = "xyes"; then
	AC_MSG_RESULT(yes)
	AC_CHECK_FUNC(sctp_sendv,
	              [AC_DEFINE([HAVE_SCTP_SENDV],[1], [Have sctp_sendv()])],
	              [AC_CHECK_LIB(sctp,
	                            sctp_sendv,
	                            [AC_DEFINE([HAVE_SCTP_SENDV],[1], [Have sctp_sendv()])
	                             UPERF_LIBS="$UPERF_LIBS -lsctp"])])
	AC_CHECK_FUNC(sctp_sendmsg,
	              [AC_DEFINE([HAVE_SCTP_SENDMSG],[1], [Have sctp_sendmsg()])],
	              [AC_CHECK_LIB(sctp,
	                            sctp_sendmsg,
	                            [AC_DEFINE([HAVE_SCTP_SENDMSG],[1], [Have sctp_sendmsg()])
	                             UPERF_LIBS="$UPERF_LIBS -lsctp"])])
	AM_CONDITIONAL([SCTP_C], [true])
	AC_DEFINE([HAVE_SCTP],[1], [Have sctp])
else
	SCTP_C=
	AM_CONDITIONAL([SCTP_C], [false])
	AC_MSG_RESULT(no)
fi

AC_ARG_WITH(ssl-includes,
	    AC_HELP_STRING([--with-ssl-includes=DIR],
                           [Specify location of ssl headers]),
            [CFLAGS="$CFLAGS -I$withval"],
            [])

AC_ARG_WITH(ssl-libs,
	    AC_HELP_STRING([--with-ssl-libs=DIR],
                           [Specify location of ssl libraries]),
	    [LDFLAGS="$LDFLAGS -L$withval"],
	    [])
	##[LDFLAGS="$LDFLAGS -L$withval -R$withval"],
	##[LDFLAGS="$LDFLAGS -L/usr/sfw/lib -R/usr/sfw/lib"])



AC_MSG_CHECKING(whether to enable ssl support)
AC_ARG_ENABLE(ssl,
	AC_HELP_STRING([--enable-ssl],[Turn on ssl support ]),,
	enable_ssl=$ssl_default)
if test "x$enable_ssl" = "xyes"; then
	AC_MSG_RESULT(yes)
	AC_CHECK_LIB([ssl], [SSL_CTX_new],[],
		[echo Error: Cannot find libssl. Try with --disable-ssl; exit -1])
	AM_CONDITIONAL([SSL_C], [true])
	AC_DEFINE([HAVE_SSL],[1],[Have ssl ])
	UPERF_LIBS="$UPERF_LIBS -lcrypto -lssl"
else
	SSL_C=
	AM_CONDITIONAL([SSL_C], [false])
	AC_MSG_RESULT(no)
fi

SOURCE_DATE_EPOCH="${SOURCE_DATE_EPOCH:-`date +%s`}"
timestamp=`date -u -d "@$SOURCE_DATE_EPOCH" 2>/dev/null || date -u -r "$SOURCE_DATE_EPOCH" 2>/dev/null || date -u`

AC_DEFINE_UNQUOTED([CONFIGURED_ON],["$timestamp"],[Configured on])
AC_DEFINE_UNQUOTED([BUILD_DATE],["$timestamp"],[Built on])

AC_SUBST(CFLAGS)
AC_SUBST(LDFLAGS)
AC_SUBST(UPERF_LIBS)


#AC_CONFIG_FILES([Makefile
AC_OUTPUT([Makefile
                 src/Makefile
		 tests/Makefile
                 workloads/Makefile])

echo "CC=$CC"
echo " "
echo " "
echo "+------------------------------------------------+"
if test x$build_master = xfalse; then
	echo "| Warning: Will build only the slave             |"
else
	echo "| Will build combined binary (master + slave)    |"
fi
echo "+------------------------------------------------+"
echo "| Supported Protocols:                           |"
printf "|%20s:%5s%22s|\n" "TCP" "yes" " "
printf "|%20s:%5s%22s|\n" "UDP" "${enable_udp}" " "
printf "|%20s:%5s%22s|\n" "RDS" "${enable_rds}" " "
printf "|%20s:%5s%22s|\n" "SSL" "${enable_ssl}" " "
printf "|%20s:%5s%22s|\n" "SCTP" "${enable_sctp}" " "
echo "+------------------------------------------------+"
printf "|%33s:%14s|\n" "Network stats collection enabled?" "$enable_netstat"
printf "|%33s:%14s|\n" "CPU Performance counters?"  "$cpc"
echo "+------------------------------------------------+"

