# Docker image for DHCP relay

DOCKER_DHCP_RELAY_STEM = docker-dhcp-relay
DOCKER_DHCP_RELAY = $(DOCKER_DHCP_RELAY_STEM).gz
DOCKER_DHCP_RELAY_DBG = $(DOCKER_DHCP_RELAY_STEM)-$(DBG_IMAGE_MARK).gz

$(DOCKER_DHCP_RELAY)_PATH = $(DOCKERS_PATH)/$(DOCKER_DHCP_RELAY_STEM)

$(DOCKER_DHCP_RELAY)_DEPENDS += $(ISC_DHCP_RELAY) $(SONIC_DHCPMON)

$(DOCKER_DHCP_RELAY)_DBG_DEPENDS = $($(DOCKER_CONFIG_ENGINE_BUSTER)_DBG_DEPENDS)
$(DOCKER_DHCP_RELAY)_DBG_DEPENDS += $(ISC_DHCP_RELAY_DBG)

$(DOCKER_DHCP_RELAY)_DBG_IMAGE_PACKAGES = $($(DOCKER_CONFIG_ENGINE_BUSTER)_DBG_IMAGE_PACKAGES)

$(DOCKER_DHCP_RELAY)_LOAD_DOCKERS = $(DOCKER_CONFIG_ENGINE_BUSTER)

$(DOCKER_DHCP_RELAY)_VERSION = 1.0.0
$(DOCKER_DHCP_RELAY)_PACKAGE_NAME = dhcp-relay
$(DOCKER_DHCP_RELAY)_PACKAGE_DEPENDS = database^1.0.0

$(DOCKER_DHCP_RELAY)_SERVICE_REQUIRES = updategraph
$(DOCKER_DHCP_RELAY)_SERVICE_AFTER = swss syncd teamd
$(DOCKER_DHCP_RELAY)_SERVICE_BEFORE = ntp-config
$(DOCKER_DHCP_RELAY)_SERVICE_DEPENDENT_OF = swss

SONIC_DOCKER_IMAGES += $(DOCKER_DHCP_RELAY)
SONIC_DOCKER_DBG_IMAGES += $(DOCKER_DHCP_RELAY_DBG)

ifeq ($(INCLUDE_KUBERNETES),y)
$(DOCKER_DHCP_RELAY)_DEFAULT_FEATURE_OWNER = kube
endif

$(DOCKER_DHCP_RELAY)_DEFAULT_FEATURE_STATE_ENABLED = y

ifeq ($(INSTALL_DEBUG_TOOLS),y)
SONIC_PACKAGES_LOCAL += $(DOCKER_DHCP_RELAY_DBG)
else
SONIC_PACKAGES_LOCAL += $(DOCKER_DHCP_RELAY)
endif

$(DOCKER_DHCP_RELAY)_CONTAINER_NAME = dhcp_relay
$(DOCKER_DHCP_RELAY)_CONTAINER_PRIVILEGED = true
$(DOCKER_DHCP_RELAY)_CONTAINER_VOLUMES += /etc/sonic:/etc/sonic:ro
$(DOCKER_DHCP_RELAY)_CONTAINER_VOLUMES += /usr/share/sonic/scripts:/usr/share/sonic/scripts:ro
$(DOCKER_DHCP_RELAY)_CONTAINER_TMPFS += /tmp/
$(DOCKER_DHCP_RELAY)_CONTAINER_TMPFS += /var/tmp/

$(DOCKER_DHCP_RELAY)_CLI_CONFIG_PLUGIN = /cli/config/plugins/dhcp_relay.py
$(DOCKER_DHCP_RELAY)_CLI_SHOW_PLUGIN = /cli/show/plugins/show_dhcp_relay.py

$(DOCKER_DHCP_RELAY)_FILES += $(SUPERVISOR_PROC_EXIT_LISTENER_SCRIPT)
