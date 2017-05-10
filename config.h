/* config.h.in.  Generated from configure.ac by autoheader.  */

/* Built on */
#undef BUILD_DATE

/* Configured on */
#undef CONFIGURED_ON

/* Debug */
#undef DEBUG

/* collect nw stats */
#define ENABLE_NETSTAT 1

/* Define to 1 if you have the <alloca.h> header file. */
#define HAVE_ALLOCA_H 1

/* Define to 1 if you have the <atomic.h> header file. */
#undef HAVE_ATOMIC_H

/* Have BSWAP_ macros */
#undef HAVE_BSWAP

/* Define to 1 if you have the `clock_gettime' function. */
#define HAVE_CLOCK_GETTIME 1

/* Have libcpc */
#undef HAVE_CPC

/* Define to 1 if you have the `gethrtime' function. */
#undef HAVE_GETHRTIME

/* Define to 1 if you have the `gethrvtime' function. */
#undef HAVE_GETHRVTIME

/* Define to 1 if the system has the type `hrtime_t'. */
#undef HAVE_HRTIME_T

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Have libkstat */
#undef HAVE_LIBKSTAT

/* Define to 1 if you have the `ssl' library (-lssl). */
#undef HAVE_LIBSSL

/* Define to 1 if you have the <linux/unistd.h> header file. */
#define HAVE_LINUX_UNISTD_H 1

/* Define to 1 if you have the `mach_absolute_time' function. */
#undef HAVE_MACH_ABSOLUTE_TIME

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the `nanosleep' function. */
#define HAVE_NANOSLEEP 1

/* Define to 1 if you have the <poll.h> header file. */
#define HAVE_POLL_H 1

/* Define to 1 if you have the `pthread_self' function. */
#define HAVE_PTHREAD_SELF 1

/* Have rds */
#undef HAVE_RDS

/* Have sctp */
#undef HAVE_SCTP

/* Have sctp_sendmsg() */
#undef HAVE_SCTP_SENDMSG

/* Have sctp_sendv() */
#undef HAVE_SCTP_SENDV

/* Have sendfilev */
#undef HAVE_SENDFILEV

/* Define to 1 if you have the <siginfo.h> header file. */
#undef HAVE_SIGINFO_H

/* Define to 1 if you have the <signal.h> header file. */
#define HAVE_SIGNAL_H 1

/* Have ssl */
#undef HAVE_SSL

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#undef HAVE_STRINGS_H

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strlcat' function. */
#define HAVE_STRLCAT 1

/* Define to 1 if you have the `strlcpy' function. */
#define HAVE_STRLCPY 1

/* Define to 1 if you have the `strncat' function. */
#define HAVE_STRNCAT 1

/* Define to 1 if you have the `strncpy' function. */
#define HAVE_STRNCPY 1

/* Define to 1 if you have the <stropts.h> header file. */
#undef HAVE_STROPTS_H

/* Define to 1 if you have the <sys/byteorder.h> header file. */
#undef HAVE_SYS_BYTEORDER_H

/* Define to 1 if you have the <sys/int_limits.h> header file. */
#undef HAVE_SYS_INT_LIMITS_H

/* Define to 1 if you have the <sys/ioctl.h> header file. */
#define HAVE_SYS_IOCTL_H 1

/* Define to 1 if you have the <sys/lwp.h> header file. */
#undef HAVE_SYS_LWP_H

/* Define to 1 if you have the <sys/poll.h> header file. */
#define HAVE_SYS_POLL_H 1

/* Define to 1 if you have the <sys/sendfile.h> header file. */
#define HAVE_SYS_SENDFILE_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/ttycom.h> header file. */
#undef HAVE_SYS_TTYCOM_H

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <sys/uio.h> header file. */
#define HAVE_SYS_UIO_H 1

/* Define to 1 if you have the <sys/varargs.h> header file. */
#undef HAVE_SYS_VARARGS_H

/* Define to 1 if you have the <termio.h> header file. */
#undef HAVE_TERMIO_H

/* Have udp */
#define HAVE_UDP 1

/* Define to 1 if the system has the type `uint32_t'. */
#define HAVE_UINT32_T 1

/* Define to 1 if the system has the type `uint64_t'. */
#define HAVE_UINT64_T 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if you have the <wait.h> header file. */
#undef HAVE_WAIT_H

/* Define to 1 if you have the `_lwp_self' function. */
#undef HAVE__LWP_SELF

/* Name of package */
#undef PACKAGE

/* Define to the address where bug reports for this package should be sent. */
#undef PACKAGE_BUGREPORT

/* Define to the full name of this package. */
#undef PACKAGE_NAME

/* Define to the full name and version of this package. */
#undef PACKAGE_STRING

/* Define to the one symbol short name of this package. */
#undef PACKAGE_TARNAME

/* Define to the home page for this package. */
#undef PACKAGE_URL

/* Define to the version of this package. */
#undef PACKAGE_VERSION

/* Define to 1 if you have the ANSI C header files. */
#undef STDC_HEADERS

/* Use only threads */
#undef STRAND_THREAD_ONLY

/* Setting machine os to Darwin */
#undef UPERF_DARWIN

/* Setting machine os to FreeBSD */
#undef UPERF_FREEBSD

/* Setting machine os to Linux */
#define UPERF_LINUX 1

/* Setting machine os to Android. Must be defined together with UPERF_LINUX */
#define UPERF_ANDROID 1

#if defined (UPERF_ANDROID) && ! defined (UPERF_LINUX)
#define UPERF_LINUX 1
#endif

/* Setting machine os to Solaris */
#undef UPERF_SOLARIS

/* Have cpu perf counters? */
#undef USE_CPC

/* Have version1 cpu perf counters? */
#undef USE_CPCv1

/* Have version2 cpu perf counters? */
#undef USE_CPCv2

/* Have getrusage */
#undef USE_GETRUSAGE

/* Version number of package */
#undef VERSION
