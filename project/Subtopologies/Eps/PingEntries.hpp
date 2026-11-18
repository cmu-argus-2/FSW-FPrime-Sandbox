// ======================================================================
// \title  PingEntries.hpp
// \brief  health ping thresholds for the Eps subtopology's active components
//
// Each constant is how many missed pings are allowed before the health
// component emits a WARNING_HI event (WARN) or declares a FATAL (FATAL).
// ======================================================================
#ifndef EPS_PINGENTRIES_HPP
#define EPS_PINGENTRIES_HPP

namespace PingEntries {
namespace Eps_epsManager {
enum { WARN = 3, FATAL = 5 };
}
}  // namespace PingEntries

#endif