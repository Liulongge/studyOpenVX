# currently required to be set to yes
BUILD_CONFORMANCE_TEST?=yes
# currently required to be set to yes
BUILD_TUTORIAL?=yes
# currently required to be set to no
BUILD_BAM?=no
# currently required to be set to yes
BUILD_IGNORE_LIB_ORDER?=yes

# Build for SoC
BUILD_TARGET_MODE?=yes
# Build for x86 PC
BUILD_EMULATION_MODE?=no
# valid values: X86 x86_64 all
BUILD_EMULATION_ARCH?=x86_64

# Flags to enable disable, groups of conformance tests (CT)
BUILD_CT_KHR=yes
BUILD_CT_TIOVX=yes
BUILD_CT_TIOVX_TEST_KERNELS=yes
BUILD_CT_TIOVX_IVISION=yes
BUILD_CT_TIOVX_TIDL=yes
BUILD_CT_TIOVX_HWA=yes
BUILD_CT_TIOVX_HWA_NEGATIVE_TESTS=yes
BUILD_CT_TIOVX_HWA_CAPTURE_TESTS=no
BUILD_CT_TIOVX_HWA_DISPLAY_TESTS=no
BUILD_CT_TIOVX_HWA_CSITX_TESTS=no

# valid values: release debug all
PROFILE?=all

# Applied to target mode only
BUILD_LINUX_A72?=yes
# Applied to target mode only - by default kept as no so that all users don't have to change
BUILD_QNX_A72?=no

# Applied to target mode only
BUILD_VLAB?=no
