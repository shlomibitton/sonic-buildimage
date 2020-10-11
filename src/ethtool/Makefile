.ONESHELL:
SHELL = /bin/bash
.SHELLFLAGS += -e

MAIN_TARGET = $(ETHTOOL)

$(addprefix $(DEST)/, $(MAIN_TARGET)): $(DEST)/% :
	# Obtaining the ethtool
	rm -rf ./ethtool
	git clone https://git.kernel.org/pub/scm/network/ethtool/ethtool.git
	pushd $(ETHTOOL)
	git checkout $(ETHTOOL_HASH)
	# Build package
	./autogen.sh
	./configure
	make
	cp $(ETHTOOL) $(DEST)/
	popd
	rm -rf $(ETHTOOL)
