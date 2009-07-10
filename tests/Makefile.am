TESTS_ENVIRONMENT = $(top_srcdir)/tests/test.sh $(top_srcdir)/src/uperf
TESTS = unknown_proto.xml parse_err.xml 01simple_tcp.xml 02two_groups.xml \
	02_2proto1group.xml accept-tcp.xml accept-connect.xml \
	a.xml canfail.xml disconnect_iter.xml friendliness.xml \
	high_connection_count.xml mix_thr_proc.xml test_4groups.xml \
	test-duration-process.xml test_netperf.xml test-sendfilev-chunked.xml \
	test-sendfile.xml test_send_recv.xml test-ssize-flowop-count-dur.xml 
	
XFAIL_TESTS = unknown_proto.xml parse_err.xml

if SCTP_C
TESTS += 01simple_sctp.xml 3proto.xml accept-sctp.xml multi_proto_connect.xml \
	throughput_sctp.xml
endif
if SSL_C
TESTS += 01simple_ssl.xml
endif
if UDP_C
TESTS += 01simple_udp.xml test_udp.xml
endif


