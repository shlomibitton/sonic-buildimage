.ONESHELL:
SHELL = /bin/bash
.SHELLFLAGS += -e

MAIN_TARGET = mlnx-fw-dump-me_1.mlnx.$(MLNX_FW_DUMP_ME_VERSION)_amd64.deb
DERIVED_TARGETS = mlnx-fw-dump-me-dbgsym_1.mlnx.$(MLNX_FW_DUMP_ME_VERSION)_amd64.deb

$(addprefix $(DEST)/, $(MAIN_TARGET)): $(DEST)/% :
	pushd fw-dump-me
	debuild -b -us -uc -j$(nproc)
	popd
	mv $(DERIVED_TARGETS) $* $(DEST)/
