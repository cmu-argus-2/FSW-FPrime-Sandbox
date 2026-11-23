# Telemetry packets for EpsMin, laid out for the least packetizer RAM rather
# than reusing the subtopologies' packet files. Each packet reserves a full
# 512-byte com buffer in the packetizer, so the channels are packed into one
# packet per update rate instead of one per component.
#
# Channels added to Base or Eps must be added here too (the build fails
# until they are), and config/TlmPacketizerCfg.hpp updated to match.

telemetry packets EpsMinPackets {

  @ Updated every EPS run (1 Hz)
  packet EveryRun id 1 group 1 {
    Eps.epsManager.RunCount
    Eps.epsManager.Alerts

    Eps.pmMainboard.VoltageCode
    Eps.pmMainboard.CurrentCode
    Eps.pmMainboard.VoltageMv
    Eps.pmMainboard.CurrentMa
    Eps.pmGps.VoltageCode
    Eps.pmGps.CurrentCode
    Eps.pmGps.VoltageMv
    Eps.pmGps.CurrentMa
    Eps.pmRadio.VoltageCode
    Eps.pmRadio.CurrentCode
    Eps.pmRadio.VoltageMv
    Eps.pmRadio.CurrentMa
    Eps.pmJetson.VoltageCode
    Eps.pmJetson.CurrentCode
    Eps.pmJetson.VoltageMv
    Eps.pmJetson.CurrentMa

    Eps.batteryHeaters.Heater0Enabled
    Eps.batteryHeaters.Heater1Enabled

    Base.rateGroup_1Hz.RgMaxTime
    Base.rateGroup_1Hz.RgCycleSlips
    Base.events.EventsDropped
    Base.comQueue.comQueueDepth
    Base.comQueue.buffQueueDepth
    Base.commsBufferManager.TotalBuffs
    Base.commsBufferManager.CurrBuffs
    Base.commsBufferManager.HiBuffs
    Base.commsBufferManager.NoBuffs
    Base.commsBufferManager.EmptyBuffs
  }

  @ Updated every fuel gauge cycle (every 5th EPS run)
  packet FuelGaugeCycle id 2 group 1 {
    Eps.epsManager.Data

    Eps.fuelGauge.Status
    Eps.fuelGauge.Voltage
    Eps.fuelGauge.Current
    Eps.fuelGauge.MidVoltage
    Eps.fuelGauge.StateOfCharge
    Eps.fuelGauge.RemainingCapacity
    Eps.fuelGauge.FullCapacity
    Eps.fuelGauge.Cycles
    Eps.fuelGauge.TimeToEmpty
    Eps.fuelGauge.TimeSincePowerUp
    Eps.fuelGauge.TimeToFull
    Eps.fuelGauge.Temperature
    Eps.fuelGauge.TemperatureAin1
    Eps.fuelGauge.TemperatureAin2
    Eps.fuelGauge.TemperatureDie

    Eps.epsManager.HeapAllocatedBytes
    Eps.epsManager.HeapFreeBytes
    Eps.epsManager.HeapPeakBytes
    Eps.epsManager.EpsStackUnusedBytes
    Eps.epsManager.MinTaskStackUnusedBytes
    Eps.epsManager.AllStacksPeakBytes
  }

} omit {
  # The packetizer's readback of its own group settings: only useful when
  # changing them, which needs commands this image cannot receive.
  Base.tlmSend.GroupConfigs
  Base.tlmSend.SectionEnabled
}
