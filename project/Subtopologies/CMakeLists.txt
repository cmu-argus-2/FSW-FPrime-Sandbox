# Each subtopology holds a set of instances and the connections between them,
# and is imported whole by the deployments that need it. Base IDs are
# 0x1SSCCxxx, where SS is the subtopology's slot below, so instances keep the
# same IDs in every deployment that imports them. Telemetry packets (in
# <Name>Packets.fppi) take ids from the subtopology's range:
#
#   SS  Packet ids  Subtopology
#   00  1-99        Base    command/telemetry plumbing, rate groups, UART link
#   01  100-199     Board   shared buses (I2C)
#   02  200-299     Eps
#   03  300-399     Adcs
#   0F  1500-1599   BenchTools  bench and bring-up tools; benches only, never Argus
#
# The F Prime subtopologies use 0x01000000 (CdhCore), 0x02000000 (ComCcsds)
# and 0x05000000 (FileHandling), set in project/config.
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Base/")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Board/")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Eps/")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Adcs/")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/BenchTools/")
