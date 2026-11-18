# Each subtopology holds a set of instances and the connections between them,
# and is imported whole by the deployments that need it. Base IDs are
# 0x1SSCCxxx, where SS is the subtopology's slot below, so instances keep the
# same IDs in every deployment that imports them. Telemetry packets (in
# <Name>Packets.fppi) take ids from the subtopology's range:
#
#   SS  Packet ids  Subtopology
#   00  1-99        Base    rate groups, time, events, telemetry, downlink
#   01  100-199     Board   shared buses (I2C)
#   02  200-299     Eps
#   03  300-399     Adcs
#   04  400-499     Ground  uplink and command dispatcher
#
# Every deployment imports Base; one that is commanded from the GDS also
# imports Ground. The F Prime subtopologies (CdhCore, ComCcsds, FileHandling,
# configured in project/config) are no longer imported by any deployment.
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Base/")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Ground/")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Board/")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Eps/")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Adcs/")
