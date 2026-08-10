# SP11 audio power-lab one-shot boot

This isolated entry keeps `sp11-audio-clean` as the persistent GRUB default and
adds three bounded experiments:

- transport WSA8845 PBR and CPS sidebands using their existing dedicated
  SoundWire timing table;
- coalesce the left/right amplifiers' shared master ports 7 (PBR) and 13 (CPS)
  so each physical port is programmed once;
- probe the five firmware-described MAX34417 power accumulators with a driver
  that never writes CONTROL. Telemetry reads issue only the documented UPDATE
  snapshot command.

The device tree opts CPS in only for this entry. It also retains the accepted
Phase91 touchscreen and Wi-Fi properties. The observation build's bounded DSP
readback module is included so protection responses remain visible in dmesg.

The current live kernel received NACK at all five MAX34417 addresses. Their
nodes are retained to test enumeration from a complete boot, but absence is an
expected and non-fatal result. ACPI gates these optional devices behind a PACS
presence mask, and no firmware rail label identifies a speaker-amplifier rail.
