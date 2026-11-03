# Telemetry packet definitions for TlmPacketizer.
#
# A packet is downlinked whenever any channel in it is updated, carrying the
# latest value of every channel in it. Packets must fit in one
# FW_COM_BUFFER_MAX_SIZE (512 B) buffer, including a 15-byte header
# (descriptor, packet id, timestamp); TlmPacketizer asserts at boot if one
# does not.
#
# Groups can be enabled, disabled and rate-limited at runtime with the
# tlmSend ENABLE_GROUP and CONFIGURE_GROUP_RATES commands:
#   group 1: health and sensor data that changes every cycle
#   group 2: housekeeping that changes slowly
#   group 3: version strings, which change only at boot
#
# The packets themselves live with the subtopology that owns the channels;
# include one file per imported subtopology.

telemetry packets ArgusPackets {

  include "../../../Subtopologies/Base/BasePackets.fppi"
  include "../../../Subtopologies/Eps/EpsPackets.fppi"

} omit {
}
