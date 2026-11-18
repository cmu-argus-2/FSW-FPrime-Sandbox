# A deployment can size the telemetry packetizer to its own packet set by
# pointing ARGUS_TLM_PACKETIZER_CFG at its own TlmPacketizerCfg.hpp, from its
# deployment.cmake (see the root CMakeLists.txt). The rest use this one.
if (NOT DEFINED ARGUS_TLM_PACKETIZER_CFG)
    set(ARGUS_TLM_PACKETIZER_CFG "${CMAKE_CURRENT_LIST_DIR}/TlmPacketizerCfg.hpp")
endif()

register_fprime_config(
    project_zephyr_config
    CONFIGURATION_OVERRIDES
        "${CMAKE_CURRENT_LIST_DIR}/PlatformCfg.fpp"
        "${CMAKE_CURRENT_LIST_DIR}/TlmChanImplCfg.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/CdhCoreConfig.fpp"
        "${CMAKE_CURRENT_LIST_DIR}/CdhCoreTlmConfig.fpp"
        "${ARGUS_TLM_PACKETIZER_CFG}"
        "${CMAKE_CURRENT_LIST_DIR}/ComCcsdsConfig.fpp"
        "${CMAKE_CURRENT_LIST_DIR}/FileHandlingConfig.fpp"
    INTERFACE
)
