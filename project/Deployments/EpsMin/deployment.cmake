# Build settings for EpsMin, included by the root CMakeLists.txt before
# Zephyr is set up.

# Kconfig on top of prj.conf: a thread pool sized to this deployment
list(APPEND EXTRA_CONF_FILE "${CMAKE_CURRENT_LIST_DIR}/deployment.conf")

# Telemetry packetizer sized to this deployment's packet set
set(ARGUS_TLM_PACKETIZER_CFG "${CMAKE_CURRENT_LIST_DIR}/config/TlmPacketizerCfg.hpp")
