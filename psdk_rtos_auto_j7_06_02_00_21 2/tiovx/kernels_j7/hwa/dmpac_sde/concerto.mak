
ifeq ($(TARGET_CPU), $(filter $(TARGET_CPU), X86 x86_64 R5F))

include $(PRELUDE)
TARGET      := vx_target_kernels_dmpac_sde
TARGETTYPE  := library
ifeq ($(TARGET_CPU),R5F)
  ifeq ($(BUILD_VLAB),yes)
    CSOURCES    := vx_dmpac_sde_target_sim.c
    IDIRS       += $(J7_C_MODELS_PATH)/include
  else
    CSOURCES    := vx_dmpac_sde_target.c
    IDIRS       += $(PDK_PATH)/packages
    IDIRS       += $(VISION_APPS_PATH)/
    IDIRS       += $(XDCTOOLS_PATH)/packages
    IDIRS       += $(BIOS_PATH)/packages
    DEFS        += SOC_J721E
  endif
else
  CSOURCES    := vx_dmpac_sde_target_sim.c
  IDIRS       += $(J7_C_MODELS_PATH)/include
endif

CSOURCES    += vx_kernels_hwa_target.c
IDIRS       += $(CUSTOM_KERNEL_PATH)/hwa/include
IDIRS       += $(HOST_ROOT)/kernels/include
IDIRS       += $(HOST_ROOT)/kernels/openvx-core/include
IDIRS       += $(VXLIB_PATH)/packages
DEFS        += TIOVX_SDE

ifeq ($(TARGET_CPU)$(BUILD_VLAB),R5Fyes)
DEFS += VLAB_HWA
endif

include $(FINALE)

endif
