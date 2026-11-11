// ======================================================================
// \title  I2cScanner.hpp
// \brief  I2C bus scanner: a bench tool, not flight software.
// ======================================================================

#ifndef project_I2cScanner_HPP
#define project_I2cScanner_HPP

#include "project/Tools/I2cScanner/I2cScannerComponentAc.hpp"

namespace project {

class I2cScanner final : public I2cScannerComponentBase {
  public:
    //! 0x00-0x07 and 0x78-0x7F are reserved by the I2C specification and
    //! never probed.
    static constexpr U8 FIRST_ADDRESS = 0x08;
    static constexpr U8 LAST_ADDRESS = 0x77;

    //! Capacity of the address list in BusScanned, including its terminator:
    //! event strings are capped at FW_LOG_STRING_MAX_SIZE (200) characters.
    static constexpr FwSizeType LIST_CAPACITY = 201;

    explicit I2cScanner(const char* const compName);
    ~I2cScanner() override;

    //! Append `address` to the space-separated `list`, which holds `used`
    //! characters. If it would not fit with room left for a trailing " ...",
    //! nothing is appended and false is returned.
    static bool appendAddress(char* list, FwSizeType& used, U8 address);

  private:
    //! Probe one address on one bus. True if it acknowledged.
    bool probe(U8 bus, U8 address);

    //! Probe every address on one bus and emit BusScanned.
    void scanBus(U8 bus);

    void SCAN_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;
};

}  // namespace project

#endif
