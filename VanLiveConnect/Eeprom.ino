
// Functions for (emulated) EEPROM

// Defined in PacketToJson.ino
extern const char emptyStr[]; 

// Needed to prevent unnecessary invocations to EEPROM.commit(), which needs to be surrounded by VanBusRx.Disable()
// resp. VanBusRx.Enable() to prevent crashes; VanBusRx.Disable() / VanBusRx.Enable() seem to increase the VAN packet
// CRC errors quite a bit... :-(
bool _eepromDirty = false;

void WriteEeprom(int const address, uint8_t const val, const char* message)
{
    EEPROM.write(address, val);
    _eepromDirty = true;

    Serial.printf_P(
        PSTR("==> %s: written value %u to EEPROM position %d (but not yet committed)\n"),
        message ? message : emptyStr,
        val,
        address);
} // WriteEeprom

void CommitEeprom()
{
    if (! _eepromDirty) return;

    Serial.print("==> Committing all values written to EEPROM\n");
    VanBusRx.Disable();
    EEPROM.commit();  // Will only write to flash if any data was actually changed
    VanBusRx.Enable();

    _eepromDirty = false;
} // CommitEeprom
