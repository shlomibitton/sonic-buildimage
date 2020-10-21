.ONESHELL:
SHELL = /bin/bash
.SHELLFLAGS += -e

MAIN_TARGET = $(ETHTOOL)

$(addprefix $(DEST)/, $(MAIN_TARGET)): $(DEST)/% :
	# Obtaining the ethtool
	rm -rf ./ethtool
	git clone https://git.kernel.org/pub/scm/network/ethtool/ethtool.git
	pushd $(ETHTOOL)
	git checkout 88ca347ef35acb16ec0a0107f55ab599ddb18f56
	# Build package
	./autogen.sh
	./configure
	make
	cp $(ETHTOOL) $(DEST)/
	popd
	rm -rf $(ETHTOOL)
