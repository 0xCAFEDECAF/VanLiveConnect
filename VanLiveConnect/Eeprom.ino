
// Functions for (emulated) EEPROM

// Needed to prevent unnecessary invocations to EEPROM.commit(), which needs to be surrounded by VanBusRx.Disable()
// resp. VanBusRx.Enable() to prevent crashes; VanBusRx.Disable() / VanBusRx.Enable() seem to increase the VAN packet
// CRC errors quite a bit... :-(
bool _eepromDirty = false;

void WriteEeprom(int const address, uint8_t const val)
{
    EEPROM.write(address, val);
    _eepromDirty = true;

    Serial.printf_P(PSTR("=====> Written value %u to EEPROM\n"), val);  // TODO - remove
} // WriteEeprom

void CommitEeprom()
{
    if (! _eepromDirty) return;

    Serial.println("=====> Committing EEPROM");  // TODO - remove
    VanBusRx.Disable();
    EEPROM.commit();
    VanBusRx.Enable();

    _eepromDirty = false;
} // CommitEeprom
