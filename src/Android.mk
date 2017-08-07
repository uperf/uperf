LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := uperf
LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES :=  workorder.c strand.c execute.c flowops_library.c \
        flowops.c common.c main.c slave.c  stats.c handshake.c parse.c shm.c \
        master.c print.c signals.c goodbye.c delay.c rate.c sendfilev.c \
        logging.c netstat.c numbers.c sync.c protocol.c tcp.c generic.c \
        udp.c

LOCAL_CFLAGS := -DHAVE_CONFIG_H
LOCAL_C_INCLUDES := $(LOCAL_PATH)/..

LOCAL_SHARED_LIBRARIES := libhardware_legacy

include $(BUILD_EXECUTABLE)
