/**
 * \file
 * \brief Telemetry Channel configuration override for the RP2350 deployment
 *
 * The upstream default sizes this table for a desktop/Linux-scale
 * deployment (500 hash buckets), which alone needs ~600KB of RAM -
 * more than the RP2350's entire 520KB SRAM. This deployment currently
 * defines 94 telemetry channels, so the bucket count is sized just
 * above that with a small margin for headroom.
 */

#ifndef TLMCHANIMPLCFG_HPP_
#define TLMCHANIMPLCFG_HPP_

namespace {

enum {
    TLMCHAN_NUM_TLM_HASH_SLOTS = 15,
    TLMCHAN_HASH_MOD_VALUE = 99,

    TLMCHAN_HASH_BUCKETS = 80,  // !< Must stay >= number of telemetry channels in the topology (currently 68)

    TLMCHAN_MAX_ENTRIES_PER_RUN = TLMCHAN_HASH_BUCKETS,
};

}

#endif /* TLMCHANIMPLCFG_HPP_ */
