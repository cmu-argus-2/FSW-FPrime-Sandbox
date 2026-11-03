# Telemetry packet definitions for TlmPacketizer. See
# project/Deployments/Argus/Top/ArgusPackets.fppi for how packets and groups
# behave. Include one file per imported subtopology.

telemetry packets EpsBenchPackets {

  include "../../../Subtopologies/Base/BasePackets.fppi"
  include "../../../Subtopologies/Eps/EpsPackets.fppi"

} omit {
}
