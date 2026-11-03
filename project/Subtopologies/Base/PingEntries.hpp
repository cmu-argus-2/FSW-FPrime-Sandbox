// ======================================================================
// \title  PingEntries.hpp
// \brief  health ping thresholds for the Base subtopology's active components
//
// Each constant is how many missed pings are allowed before the health
// component emits a WARNING_HI event (WARN) or declares a FATAL (FATAL).
// ======================================================================
#ifndef BASE_PINGENTRIES_HPP
#define BASE_PINGENTRIES_HPP

namespace PingEntries {
namespace Base_rateGroup_1Hz {
enum { WARN = 3, FATAL = 5 };
}
namespace Base_rateGroup_0_5Hz {
enum { WARN = 3, FATAL = 5 };
}
namespace Base_rateGroup_0_25Hz {
enum { WARN = 3, FATAL = 5 };
}
namespace Base_cmdSeq {
enum { WARN = 3, FATAL = 5 };
}
}  // namespace PingEntries

#endif
