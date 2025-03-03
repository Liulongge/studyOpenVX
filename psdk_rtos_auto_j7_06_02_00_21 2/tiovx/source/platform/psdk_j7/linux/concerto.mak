
ifeq ($(TARGET_PLATFORM),J7)
ifeq ($(TARGET_CPU),$(filter $(TARGET_CPU), A72))
ifeq ($(TARGET_OS),LINUX)

include $(PRELUDE)
TARGET      := vx_platform_psdk_j7_linux
TARGETTYPE  := library

COMMON_FILES_REL_PATH = ../../../../source/platform/os/linux

CSOURCES    := \
    $(COMMON_FILES_REL_PATH)/tivx_event.c \
    $(COMMON_FILES_REL_PATH)/tivx_mutex.c \
    $(COMMON_FILES_REL_PATH)/tivx_queue.c \
    $(COMMON_FILES_REL_PATH)/tivx_task.c  \
    ../common/tivx_target_config_mpu1_0.c           \
    ../common/tivx_ipc.c                            \
    ../common/tivx_init.c                           \
    tivx_platform_linux.c                           \
	../common/tivx_host.c                           \
    ../common/tivx_platform.c                       \
	../common/tivx_mem.c                                      \

IDIRS       += $(HOST_ROOT)/source/include
IDIRS       += $(HOST_ROOT)/source/platform/psdk_j7/common
IDIRS       += $(VISION_APPS_PATH)

DEFS += TARGET_HLOS

include $(FINALE)

endif
endif
endif

