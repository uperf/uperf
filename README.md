# Quickstart

Please visit [uperf.org](http://www.uperf.org) for the latest information

On the slave run `./uperf -s`

On the master (System under test) `./uperf -m netperf.xml`

To get lots of statistics, use the `-a` option. For ex

    ./uperf -m netperf.xml -a

Please open Github issues for comments, bugs.


# Compiling uperf

uperf uses the standard GNU build tools (`./configure;make`)

# Running uperf

1. The ./uperf can be run as either a master(active) OR slave(passive).
2. When run as active it needs  master flag(-m) with a profile describing
   the test application. Sample profiles have been provided which are
   sure to run, more and more application profiles are expected to be
   added defining the various test cases.
3. When run as the slave the program needs -s flag
4. By default we are using 20000 port as the master port.
5. We can set the verbose level, three level are provided
	1. (-V) High - all the messages are printed
	2. (-v) med - warnings and critical messages are displayed
	3. ( )  low - only critical messages are displayed.
6. Outputs: See usage for more information


# Profile examples

Please see `workloads/` directory for example profiles

## Compiling on Android

It is possible to build uperf as a part of AOSP platform. Because autotools
are not used in Android platform config.h cannot be generated automatically.
A version of config.h for Android N is created manually. Modifications may
be required for compatibility with other Android versions.

**Steps**

1. Download uperf sources and place them under appropriate AOSP directory,
   e.g. external/uperf
2. Initialize build environment following standard Android steps
   (i.e. run `source build/envsetup.sh`, refer to Android official documentation
   for this part)
3. Build uperf
   ```
   cd external/uperf
   mm
   ```
