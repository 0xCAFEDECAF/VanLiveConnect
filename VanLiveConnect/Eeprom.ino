
// Functions for (emulated) EEPROM

// Needed to prevent unnecessary invocations to EEPROM.commit(), which needs to be surrounded by VanBusRx.Disable()
// resp. VanBusRx.Enable() to prevent crashes; VanBusRx.Disable() / VanBusRx.Enable() seem to increase the VAN packet
// CRC errors quite a bit... :-(
bool _eepromDirty = false;

void WriteEeprom(int const address, uint8_t const val)
{
    EEPROM.write(address, val);
    _eepromDirty = true;

    Serial.printf_P(PSTR("==> Written value %u to EEPROM position %d (but not yet committed)\n"), val, address);
} // WriteEeprom

void CommitEeprom()
{
    if (! _eepromDirty) return;

    Serial.println("==> Committing all values written to EEPROM");
    VanBusRx.Disable();
    EEPROM.commit();  // Will only write to flash if any data was actually changed
    VanBusRx.Enable();

    _eepromDirty = false;
} // CommitEeprom
