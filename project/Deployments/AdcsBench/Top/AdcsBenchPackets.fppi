# Telemetry packet definitions for TlmPacketizer. See
# project/Deployments/Argus/Top/ArgusPackets.fppi for how packets and groups
# behave. Include one file per imported subtopology.

telemetry packets AdcsBenchPackets {

  include "../../../Subtopologies/Base/BasePackets.fppi"
  include "../../../Subtopologies/Adcs/AdcsPackets.fppi"

} omit {
}
