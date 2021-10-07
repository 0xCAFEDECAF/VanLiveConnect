
// Functions, types and variables involved in keeping track of what the original multi-function display (MFD) is 
// showing (the small left side panel, the larger right side panel and any popup).
//
// The original MFD has pretty weird logic for showing its various screens.

// Defined in Eeprom.ino
void WriteEeprom(int const address, uint8_t const val);

// Defined in PacketToJson.ino
extern const char yesStr[];
extern const char noStr[];
extern const char notApplicable3Str[];
extern bool isHeadUnitPowerOn;
extern bool isSatnavGuidanceActive;
extern bool isCurrentStreetKnown;

// Index of small screen
enum SmallScreen_t
{
    SMALL_SCREEN_INVALID = 0xFF,
    SMALL_SCREEN_FIRST = 0,

    // Same order as the original MFD goes through as the driver short-presses the right stalk button
    SMALL_SCREEN_TRIP_INFO_1 = SMALL_SCREEN_FIRST, // When the original MFD is plugged in, this is what it starts with
    SMALL_SCREEN_TRIP_INFO_2,
    SMALL_SCREEN_GPS_INFO, // Skipped when in guidance mode
    SMALL_SCREEN_FUEL_CONSUMPTION,

    SMALL_SCREEN_LAST = SMALL_SCREEN_FUEL_CONSUMPTION,
    N_SMALL_SCREENS
}; // enum SmallScreen_t

// Index of large screen
enum LargeScreen_t
{
    LARGE_SCREEN_INVALID = 0xFF,
    LARGE_SCREEN_FIRST = 0,

    // Same order as the original MFD goes through as the driver presses the "MOD" button on the IR controller
    LARGE_SCREEN_CLOCK = LARGE_SCREEN_FIRST,  // When the original MFD is plugged in, this is what it starts with
    LARGE_SCREEN_GUIDANCE,  // Skipped when not in guidance mode
    LARGE_SCREEN_HEAD_UNIT,  // Skipped when head unit is powered off
    LARGE_SCREEN_CURRENT_STREET,  // Skipped when in guidance mode
    LARGE_SCREEN_TRIP_COMPUTER,  // Skipped when not in guidance mode

    LARGE_SCREEN_LAST = LARGE_SCREEN_TRIP_COMPUTER,
    N_LARGE_SCREENS
}; // enum LargeScreen_t

#define SMALL_SCREEN_EEPROM_POS (1)
uint8_t smallScreenIndex = SMALL_SCREEN_INVALID;  // Also stored in (emulated) EEPROM
uint8_t largeScreenIndex = LARGE_SCREEN_CLOCK;
uint8_t largeScreenIndexBeforeGoingIntoGuidanceMode = LARGE_SCREEN_CLOCK;

unsigned long popupShowingSince = 0;
long popupDuration = 0;

// Keeps track of the tab selected in the trip info popup (as shown during sat nav guidance mode, after pressing
// the right stalk button)
int tripInfoPopupTabIndex = -1;

// As found in VAN bus packets with IDEN 0x5E4, data bytes 0 and 1
enum MfdStatus_t
{
    MFD_SCREEN_OFF = 0x00FF,
    MFD_SCREEN_ON = 0x20FF,
    TRIP_COUTER_1_RESET = 0xA0FF,
    TRIP_COUTER_2_RESET = 0x60FF
}; // enum MfdStatus_t

// Returns a PSTR (allocated in flash, saves RAM). In printf formatter use "%S" (capital S) instead of "%s".
PGM_P SmallScreenStr(uint8_t idx)
{
    return
        idx == SMALL_SCREEN_TRIP_INFO_1 ? PSTR("TRIP_INFO_1") :
        idx == SMALL_SCREEN_TRIP_INFO_2 ? PSTR("TRIP_INFO_2") :
        idx == SMALL_SCREEN_GPS_INFO ? PSTR("GPS_INFO") :
        idx == SMALL_SCREEN_FUEL_CONSUMPTION ? PSTR("FUEL_CONSUMPTION") :
        notApplicable3Str;
} // SmallScreenStr

// Returns a PSTR (allocated in flash, saves RAM). In printf formatter use "%S" (capital S) instead of "%s".
PGM_P SmallScreenStr()
{
    return SmallScreenStr(smallScreenIndex);
} // SmallScreenStr

// Returns a PSTR (allocated in flash, saves RAM). In printf formatter use "%S" (capital S) instead of "%s".
PGM_P LargeScreenStr(uint8_t idx)
{
    return
        idx == LARGE_SCREEN_CLOCK ? PSTR("CLOCK") :
        idx == LARGE_SCREEN_GUIDANCE ? PSTR("GUIDANCE") :
        idx == LARGE_SCREEN_HEAD_UNIT ? PSTR("HEAD_UNIT") :
        idx == LARGE_SCREEN_CURRENT_STREET ? PSTR("CURRENT_STREET") :
        idx == LARGE_SCREEN_TRIP_COMPUTER ? PSTR("TRIP_COMPUTER") :
        notApplicable3Str;
} // LargeScreenStr

// Returns a PSTR (allocated in flash, saves RAM). In printf formatter use "%S" (capital S) instead of "%s".
PGM_P LargeScreenStr()
{
    return LargeScreenStr(largeScreenIndex);
} // LargeScreenStr

bool isPopupShowing()
{
    // Arithmetic has safe roll-over
    bool result = popupDuration != 0 && millis() - popupShowingSince < popupDuration;

#ifdef DEBUG_ORIGINAL_MFD
    if (result)
    {
        Serial.printf_P(PSTR("[originalMfd] isPopupShowing: YES (%ld msec)\n"), popupDuration);
    }
    else
    {
        Serial.printf_P(PSTR("[originalMfd] isPopupShowing: NO\n"));
    } // if
#endif // DEBUG_ORIGINAL_MFD

    return result;
} // isPopupShowing

// Initialize smallScreenIndex, if necessary
void InitSmallScreenIndex()
{
    if (smallScreenIndex >= SMALL_SCREEN_FIRST && smallScreenIndex <= SMALL_SCREEN_LAST) return;

    smallScreenIndex = EEPROM.read(SMALL_SCREEN_EEPROM_POS);

#ifdef DEBUG_ORIGINAL_MFD
    Serial.printf_P(
        PSTR("[originalMfd] Read value %u (%S) from EEPROM position %d\n"),
        smallScreenIndex,
        SmallScreenStr(),
        SMALL_SCREEN_EEPROM_POS);
#endif // DEBUG_ORIGINAL_MFD

    if (smallScreenIndex >= SMALL_SCREEN_FIRST && smallScreenIndex <= SMALL_SCREEN_LAST) return;

    // When the original MFD is plugged in, this is what it starts with
    smallScreenIndex = SMALL_SCREEN_TRIP_INFO_1;

    WriteEeprom(SMALL_SCREEN_EEPROM_POS, smallScreenIndex);
} // InitSmallScreenIndex

void ResetTripInfo(uint16_t mfdStatus)
{
    // Force change to the appropriate small screen (left hand side of the display)

    uint8_t newSmallScreenIndex = mfdStatus == TRIP_COUTER_1_RESET ? SMALL_SCREEN_TRIP_INFO_1 : SMALL_SCREEN_TRIP_INFO_2;
    if (smallScreenIndex == newSmallScreenIndex) return;

    smallScreenIndex = newSmallScreenIndex;

#ifdef DEBUG_ORIGINAL_MFD
    Serial.printf_P(
        PSTR("[originalMfd] Right stalk button long-press; smallScreenIndex := %S\n"),
        SmallScreenStr()
    );
#endif // DEBUG_ORIGINAL_MFD

    WriteEeprom(SMALL_SCREEN_EEPROM_POS, smallScreenIndex);
} // ResetTripInfo

// Keep track of the cycling through the trip computer screens in the original MFD. Called after a short-press
// of the right hand stalk button. Variable 'smallScreenIndex' will be updated.
void CycleTripInfo()
{
    if (! isSatnavGuidanceActive || largeScreenIndex == LARGE_SCREEN_TRIP_COMPUTER)
    {
        // On the the original MFD, a trip computer screen is showing (either in the small screen or in the
        // large screen)

        // Short-press of right stalk button selects next small screen
        smallScreenIndex = (smallScreenIndex + 1) % N_SMALL_SCREENS;

        if (largeScreenIndex == LARGE_SCREEN_TRIP_COMPUTER)
        {
            // Trip computer is showing on the large screen

            // On the the original MFD, the trip computer in the large screen will copy the value selected in the
            // trip computer popup
            if (tripInfoPopupTabIndex >= 0) smallScreenIndex = tripInfoPopupTabIndex;

            // Skip the "GPS info" screen
            if (smallScreenIndex == SMALL_SCREEN_GPS_INFO) smallScreenIndex = SMALL_SCREEN_FUEL_CONSUMPTION;
        } // if

#ifdef DEBUG_ORIGINAL_MFD
        Serial.printf_P(
            PSTR("[originalMfd] Right stalk button short-press; smallScreenIndex := %u (%S)\n"),
            smallScreenIndex,
            SmallScreenStr()
        );
#endif // DEBUG_ORIGINAL_MFD

        WriteEeprom(SMALL_SCREEN_EEPROM_POS, smallScreenIndex);

        tripInfoPopupTabIndex = -1;
    }
    else // isSatnavGuidanceActive && largeScreenIndex != LARGE_SCREEN_TRIP_COMPUTER
    {
        // On the the original MFD, the trip computer popup is triggered, or is already visible

        // The logic of the original MFD is as follows:
        // - The initial button-press simply triggers the trip computer popup.
        // - As long as the trip computer popup is visible, each next button-press leads to the next tab.
        // - When leaving guidance mode, the small screen (left on the MFD) goes back to the tab it was
        //   originally showing.

        static unsigned long popupLastAppeared;
        unsigned long now = millis();

        // Arithmetic has safe roll-over
        if (now - popupLastAppeared > 8000)
        {
            // The popup is not visible: the stalk button triggers the trip computer popup to appear

            // Initialize the index of the selected tab in the popup. If the GPS info screen was showing, then
            // select the fuel consumption tab.
            tripInfoPopupTabIndex =
                smallScreenIndex == SMALL_SCREEN_GPS_INFO ? SMALL_SCREEN_FUEL_CONSUMPTION : smallScreenIndex;

#ifdef DEBUG_ORIGINAL_MFD
            Serial.printf_P(
                PSTR(
                    "[originalMfd] Right stalk button short-press; trip computer popup appeared;"
                    " tripInfoPopupTabIndex := %u (%S)\n"
                ),
                tripInfoPopupTabIndex,
                SmallScreenStr(tripInfoPopupTabIndex)
            );
#endif // DEBUG_ORIGINAL_MFD
        }
        else
        {
            // When the trip computer popup is visible, the stalk button cycles through the 2 trip counters
            // and the fuel consumption tab
            tripInfoPopupTabIndex = (tripInfoPopupTabIndex + 1) % N_SMALL_SCREENS;

            // Skip GPS info screen
            if (tripInfoPopupTabIndex == SMALL_SCREEN_GPS_INFO) tripInfoPopupTabIndex = SMALL_SCREEN_FUEL_CONSUMPTION;

#ifdef DEBUG_ORIGINAL_MFD
            Serial.printf_P(
                PSTR(
                    "[originalMfd] Right stalk button short-press; next tab in trip computer popup;"
                    " tripInfoPopupTabIndex := %u (%S)\n"
                ),
                tripInfoPopupTabIndex,
                SmallScreenStr(tripInfoPopupTabIndex)
            );
#endif // DEBUG_ORIGINAL_MFD
        } // if

        popupLastAppeared = now;
    } // if
} // CycleTripInfo

void UpdateLargeScreenForHeadUnitOn()
{
    if (isSatnavGuidanceActive)
    {
        if (largeScreenIndex == LARGE_SCREEN_CLOCK)
        {
            largeScreenIndex = LARGE_SCREEN_HEAD_UNIT;
        }
        else
        {
            // LARGE_SCREEN_GUIDANCE or LARGE_SCREEN_TRIP_COMPUTER
            popupShowingSince = millis();
            popupDuration = 8000;
        } // if
    }
    else
    {
        largeScreenIndex = LARGE_SCREEN_HEAD_UNIT;
    } // if

#ifdef DEBUG_ORIGINAL_MFD
    Serial.printf_P(
        PSTR("[originalMfd] Head unit powered on; popupShowing = %S (%ld msec); largeScreenIndex := %S\n"),
        isPopupShowing() ? yesStr : noStr,
        popupDuration,
        LargeScreenStr()
    );
#endif // DEBUG_ORIGINAL_MFD
} // UpdateLargeScreenForHeadUnitOn

void UpdateLargeScreenForHeadUnitOff()
{
    popupShowingSince = 0;

    if (largeScreenIndex == LARGE_SCREEN_HEAD_UNIT)
    {
        if (isSatnavGuidanceActive) largeScreenIndex = LARGE_SCREEN_TRIP_COMPUTER;
        else largeScreenIndex = LARGE_SCREEN_CLOCK;

        if (largeScreenIndex == LARGE_SCREEN_CLOCK && isCurrentStreetKnown)
        {
            largeScreenIndex = LARGE_SCREEN_CURRENT_STREET;
        } // if
    } // if

#ifdef DEBUG_ORIGINAL_MFD
    Serial.printf_P(
        PSTR("[originalMfd] Head unit powered off; guidance=%S; curr_street_known=%S; largeScreenIndex := %S\n"),
        isSatnavGuidanceActive ? yesStr : noStr,
        isCurrentStreetKnown ? yesStr : noStr,
        LargeScreenStr()
    );
#endif // DEBUG_ORIGINAL_MFD
} // UpdateLargeScreenForHeadUnitOff

void UpdateLargeScreenForGuidanceModeOn()
{
    largeScreenIndexBeforeGoingIntoGuidanceMode = largeScreenIndex;  // To return to later, when guidance ends
    largeScreenIndex = LARGE_SCREEN_GUIDANCE;

#ifdef DEBUG_ORIGINAL_MFD
    Serial.printf_P(
        PSTR("[originalMfd] Going into guidance; largeScreenIndexBefore=%S; largeScreenIndex := %S\n"),
        LargeScreenStr(largeScreenIndexBeforeGoingIntoGuidanceMode),
        LargeScreenStr()
    );
#endif // DEBUG_ORIGINAL_MFD
} // UpdateLargeScreenForGuidanceModeOn

void UpdateLargeScreenForGuidanceModeOff()
{
    // Stay in clock, if showing
    if (largeScreenIndex != LARGE_SCREEN_CLOCK)
    {
        if (largeScreenIndex == LARGE_SCREEN_GUIDANCE)
        {
            largeScreenIndex = largeScreenIndexBeforeGoingIntoGuidanceMode;
            if (largeScreenIndex == LARGE_SCREEN_HEAD_UNIT && ! isHeadUnitPowerOn) largeScreenIndex = LARGE_SCREEN_CLOCK;
            if (largeScreenIndex == LARGE_SCREEN_CLOCK && isCurrentStreetKnown) largeScreenIndex = LARGE_SCREEN_CURRENT_STREET;
        }
        else if (largeScreenIndex == LARGE_SCREEN_TRIP_COMPUTER)
        {
            largeScreenIndex = LARGE_SCREEN_HEAD_UNIT;
            if (largeScreenIndex == LARGE_SCREEN_HEAD_UNIT && ! isHeadUnitPowerOn) largeScreenIndex = LARGE_SCREEN_CLOCK;
            // Bug in original MFD: in this case, if current street is known, it does not switch to that from
            // the clock screen.
        } // if
    } // if

#ifdef DEBUG_ORIGINAL_MFD
    Serial.printf_P(
        PSTR("[originalMfd] Going out of guidance; head_unit=%S; curr_street_known=%S; largeScreenIndex := %S\n"),
        isHeadUnitPowerOn ? yesStr : noStr,
        isCurrentStreetKnown ? yesStr : noStr,
        LargeScreenStr()
    );
#endif // DEBUG_ORIGINAL_MFD
} // UpdateLargeScreenForGuidanceModeOff

// Called after receiving a "MOD" button press from the IR remote control. Variables 'largeScreenIndex' will be
// updated; variable 'smallScreenIndex' may also be updated.
void CycleLargeScreen()
{
    // Ignore as long as any popup is showing
    if (isPopupShowing()) return;

    // Keep track of the cycling through the large screens in the original MFD
    largeScreenIndex = (largeScreenIndex + 1) % N_LARGE_SCREENS;

    if (largeScreenIndex == LARGE_SCREEN_GUIDANCE && ! isSatnavGuidanceActive)
    {
        largeScreenIndex = LARGE_SCREEN_HEAD_UNIT;
    } // if

    if (largeScreenIndex == LARGE_SCREEN_HEAD_UNIT && ! isHeadUnitPowerOn)
    {
        largeScreenIndex = LARGE_SCREEN_CURRENT_STREET;
    } // if

    if (largeScreenIndex == LARGE_SCREEN_CURRENT_STREET && (isSatnavGuidanceActive || ! isCurrentStreetKnown))
    {
        // TODO - even when the current street is known, the original MFD does not always cycle through it.
        // And sometimes when the current street is not known, the original MFD does cycle through it (showing an
        // empty box). So what is the condition on which the original MFD decides to start showing the current street?
        largeScreenIndex = LARGE_SCREEN_TRIP_COMPUTER;
    } // if

    if (largeScreenIndex == LARGE_SCREEN_TRIP_COMPUTER && ! isSatnavGuidanceActive)
    {
        largeScreenIndex = LARGE_SCREEN_CLOCK;
    } // if

#ifdef DEBUG_ORIGINAL_MFD
    Serial.printf_P(
        PSTR("[originalMfd] \"MOD\" button press; guidance=%S; head_unit=%S; curr_street_known=%S; largeScreenIndex := %S\n"),
        isSatnavGuidanceActive ? yesStr : noStr,
        isHeadUnitPowerOn ? yesStr : noStr,
        isCurrentStreetKnown ? yesStr : noStr,
        LargeScreenStr()
    );
#endif // DEBUG_ORIGINAL_MFD

    if (largeScreenIndex == LARGE_SCREEN_TRIP_COMPUTER)
    {
        // On the the original MFD, the trip computer in the large screen is copied into the small screen
        if (tripInfoPopupTabIndex >= 0) smallScreenIndex = tripInfoPopupTabIndex;
        tripInfoPopupTabIndex = -1;

        if (smallScreenIndex == SMALL_SCREEN_GPS_INFO) smallScreenIndex = SMALL_SCREEN_FUEL_CONSUMPTION;

#ifdef DEBUG_ORIGINAL_MFD
        Serial.printf_P(
            PSTR("[originalMfd] Switched large screen to trip computer; smallScreenIndex := %S\n"),
            SmallScreenStr()
        );
#endif // DEBUG_ORIGINAL_MFD

        WriteEeprom(SMALL_SCREEN_EEPROM_POS, smallScreenIndex);
    } // if
} // CycleLargeScreen
