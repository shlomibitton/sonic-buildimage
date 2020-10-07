# Mellanox FW Dump Me

MLNX_FW_DUMP_ME_VERSION = 1.0.0

export MLNX_FW_DUMP_ME_VERSION

MLNX_FW_DUMP_ME = mlnx-fw-dump-me_1.mlnx.$(MLNX_FW_DUMP_ME_VERSION)_amd64.deb
$(MLNX_FW_DUMP_ME)_SRC_PATH = $(PLATFORM_PATH)/mlnx-fw-dump-me
$(MLNX_FW_DUMP_ME)_DEPENDS += $(MLNX_SDK_DEBS)
$(MLNX_FW_DUMP_ME)_RDEPENDS += $(MLNX_SDK_RDEBS) $(MLNX_SDK_DEBS)
MLNX_FW_DUMP_ME_DBGSYM = mlnx-fw-dump-me-dbgsym_1.mlnx.$(MLNX_FW_DUMP_ME_VERSION)_amd64.deb
$(eval $(call add_derived_package,$(MLNX_FW_DUMP_ME),$(MLNX_FW_DUMP_ME_DBGSYM)))
SONIC_MAKE_DEBS += $(MLNX_FW_DUMP_ME)
