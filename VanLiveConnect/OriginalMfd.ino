
// Functions, types and variables involved in keeping track of what the original multi-function display (MFD) is 
// showing (the small left side panel, the larger right side panel, and any popup).
//
// The original MFD has pretty weird logic for showing its various screens.

// Defined in Eeprom.ino
void WriteEeprom(int const address, uint8_t const val);

// Defined in PacketToJson.ino
extern const char yesStr[];
extern const char noStr[];
extern const char notApplicable2Str[];
extern const char notApplicable3Str[];

// The following flags drive the behaviour of the MFD regarding the large (right side) screen
extern bool isSatnavGuidanceActive;
extern bool isCurrentStreetKnown;
extern bool isHeadUnitPowerOn;

// Index of small screen
enum MFD_SmallScreen_t
{
    SMALL_SCREEN_INVALID = 0xFF,
    SMALL_SCREEN_FIRST = 0,

    // Same order as the original MFD goes through as the driver short-presses the right stalk button
    SMALL_SCREEN_TRIP_INFO_1 = SMALL_SCREEN_FIRST, // When the original MFD is plugged in, this is what it starts with
    SMALL_SCREEN_TRIP_INFO_2,
    SMALL_SCREEN_GPS_INFO,
    SMALL_SCREEN_FUEL_CONSUMPTION,

    SMALL_SCREEN_LAST = SMALL_SCREEN_FUEL_CONSUMPTION,
    N_SMALL_SCREENS
}; // enum MFD_SmallScreen_t

// Index of large screen
enum MFD_LargeScreen_t
{
    LARGE_SCREEN_INVALID = 0xFF,
    LARGE_SCREEN_FIRST = 0,

    // Same order as the original MFD goes through as the driver presses the "MOD" button on the IR controller
    LARGE_SCREEN_CLOCK = LARGE_SCREEN_FIRST,  // When the original MFD is plugged in, this is what it starts with
    LARGE_SCREEN_GUIDANCE,  // Only when in guidance mode
    LARGE_SCREEN_HEAD_UNIT,  // Skipped when head unit is powered off
    LARGE_SCREEN_CURRENT_STREET,  // Skipped when in guidance mode
    LARGE_SCREEN_TRIP_COMPUTER,  // Only when in guidance mode

    LARGE_SCREEN_LAST = LARGE_SCREEN_TRIP_COMPUTER,
    N_LARGE_SCREENS
}; // enum MFD_LargeScreen_t

// Index of user interface language
enum MFD_Language_t
{
    MFD_LANGUAGE_INVALID = 0xFF,
    MFD_LANGUAGE_FIRST = 0,

    MFD_LANGUAGE_ENGLISH = MFD_LANGUAGE_FIRST,
    MFD_LANGUAGE_FRENCH,
    MFD_LANGUAGE_GERMAN,
    MFD_LANGUAGE_SPANISH,
    MFD_LANGUAGE_ITALIAN,
    MFD_LANGUAGE_DUTCH,

    MFD_LANGUAGE_LAST = MFD_LANGUAGE_DUTCH,
    N_LANGUAGES
}; // enum MFD_Language_t

#define SMALL_SCREEN_EEPROM_POS (1)
uint8_t smallScreen = SMALL_SCREEN_INVALID;  // Loaded from and saved in (emulated) EEPROM
uint8_t largeScreen = LARGE_SCREEN_CLOCK;
uint8_t largeScreenBeforeGoingIntoGuidanceMode = LARGE_SCREEN_CLOCK;

uint8_t mfdLanguage = MFD_LANGUAGE_ENGLISH;

unsigned long NotificationPopupShowingSince = 0;
unsigned long TripComputerPopupShowingSince = 0;
long popupDuration = 0;

// Index of tab selected in the trip computer popup (as shown during sat nav guidance mode, after pressing
// the right stalk button)
int tripComputerPopupTab = -1;

// Index of tab selected in the large screen trip computer (as shown during sat nav guidance mode, after pressing the
// 'MOD' button on the IR remote control)
int tripComputerLargeScreenTab = -1;

// As found in VAN bus packets with IDEN 0x5E4, data bytes 0 and 1
enum MfdStatus_t
{
    MFD_SCREEN_OFF = 0x00FF,
    MFD_SCREEN_ON = 0x20FF,
    TRIP_COUTER_1_RESET = 0xA0FF,
    TRIP_COUTER_2_RESET = 0x60FF
}; // enum MfdStatus_t

// Returns a PSTR (allocated in flash, saves RAM). In printf formatter use "%S" (capital S) instead of "%s".
PGM_P TripComputerShortStr()
{
    return
        tripComputerPopupTab == SMALL_SCREEN_TRIP_INFO_1 ? PSTR("TR1") :
        tripComputerPopupTab == SMALL_SCREEN_TRIP_INFO_2 ? PSTR("TR2") :
        tripComputerPopupTab == SMALL_SCREEN_GPS_INFO ? PSTR("GPS") :
        tripComputerPopupTab == SMALL_SCREEN_FUEL_CONSUMPTION ? PSTR("FUE") :
        notApplicable2Str;
} // TripComputerShortStr

// Returns a PSTR (allocated in flash, saves RAM). In printf formatter use "%S" (capital S) instead of "%s".
PGM_P TripComputerStr(uint8_t idx)
{
    return
        idx == SMALL_SCREEN_TRIP_INFO_1 ? PSTR("TRIP_INFO_1") :
        idx == SMALL_SCREEN_TRIP_INFO_2 ? PSTR("TRIP_INFO_2") :
        idx == SMALL_SCREEN_GPS_INFO ? PSTR("GPS_INFO") :  // Only in small screen, not in large screen or popup
        idx == SMALL_SCREEN_FUEL_CONSUMPTION ? PSTR("FUEL_CONSUMPTION") :
        notApplicable3Str;
} // TripComputerStr

// Returns a PSTR (allocated in flash, saves RAM). In printf formatter use "%S" (capital S) instead of "%s".
PGM_P TripComputerStr()
{
    return TripComputerStr(largeScreen == LARGE_SCREEN_TRIP_COMPUTER ? tripComputerLargeScreenTab : smallScreen);
} // TripComputerStr

// Returns a PSTR (allocated in flash, saves RAM). In printf formatter use "%S" (capital S) instead of "%s".
PGM_P SmallScreenStr()
{
    return TripComputerStr(isSatnavGuidanceActive ? SMALL_SCREEN_GPS_INFO : smallScreen);
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
    return LargeScreenStr(largeScreen);
} // LargeScreenStr

// Register the fact that no popup is showing
void NoPopup()
{
    NotificationPopupShowingSince = 0;
    TripComputerPopupShowingSince = 0;
    popupDuration = 0;

#ifdef DEBUG_ORIGINAL_MFD
    Serial.printf_P(PSTR("[originalMfd] NoPopup()\n"));
#endif // DEBUG_ORIGINAL_MFD
} // NoPopup

// Register the fact that a notification popup (not the trip computer popup) is showing
void NotificationPopupShowing(unsigned long since, long duration)
{
    NotificationPopupShowingSince = since;
    TripComputerPopupShowingSince = 0;
    popupDuration = duration;

#ifdef DEBUG_ORIGINAL_MFD
    Serial.printf_P(PSTR("[originalMfd] NotificationPopupShowing(%ld msec)\n"), popupDuration);
#endif // DEBUG_ORIGINAL_MFD
} // NotificationPopupShowing

// Return true if a notification popup (not the trip computer popup) is showing, otherwise return false
bool IsNotificationPopupShowing()
{
    // Arithmetic has safe roll-over
    return popupDuration != 0 && millis() - NotificationPopupShowingSince < popupDuration;
} // IsNotificationPopupShowing

// Register the fact that the trip computer popup showing
void TripComputerPopupShowing(unsigned long since, long duration)
{
    NotificationPopupShowingSince = 0;
    TripComputerPopupShowingSince = since;
    popupDuration = duration;

#ifdef DEBUG_ORIGINAL_MFD
    Serial.printf_P(
        PSTR("[originalMfd] TripComputerPopupShowing(%ld msec), tripComputerPopupTab := %S\n"),
        popupDuration,
        TripComputerStr(tripComputerPopupTab)
    );
#endif // DEBUG_ORIGINAL_MFD
} // TripComputerPopupShowing

// Return true if the trip computer popup is showing, otherwise return false
bool IsTripComputerPopupShowing()
{
    // Arithmetic has safe roll-over
    return popupDuration != 0 && millis() - TripComputerPopupShowingSince < popupDuration;
} // IsTripComputerPopupShowing

// Returns a PSTR (allocated in flash, saves RAM). In printf formatter use "%S" (capital S) instead of "%s".
PGM_P PopupStr()
{
    return
        IsTripComputerPopupShowing() ? TripComputerShortStr() :
        IsNotificationPopupShowing() ? PSTR("N_POP") :
        emptyStr;
} // PopupStr

// Copy the tab index from the small screen into the trip computer popup and the trip computer on the large screen
void SetAllTabIndexes()
{
    tripComputerPopupTab = smallScreen == SMALL_SCREEN_GPS_INFO ? SMALL_SCREEN_FUEL_CONSUMPTION : smallScreen;
    tripComputerLargeScreenTab = tripComputerPopupTab;
} // SetAllTabIndexesEqual

// Initialize value of 'smallScreen', if necessary
void InitSmallScreen()
{
    // Already initialized?
    if (smallScreen >= SMALL_SCREEN_FIRST && smallScreen <= SMALL_SCREEN_LAST) return;

    // Initialize from (emulated) EEPROM
    smallScreen = EEPROM.read(SMALL_SCREEN_EEPROM_POS);

#ifdef DEBUG_ORIGINAL_MFD
    Serial.printf_P(
        PSTR("[originalMfd] Read value %u (%S) from EEPROM position %d\n"),
        smallScreen,
        SmallScreenStr(),
        SMALL_SCREEN_EEPROM_POS);
#endif // DEBUG_ORIGINAL_MFD

    // Successfully read from EEPROM? Then copy tab index to trip computer popup and large screen.
    if (smallScreen >= SMALL_SCREEN_FIRST && smallScreen <= SMALL_SCREEN_LAST) return SetAllTabIndexes();

    // No valid value from EEPROM: fall back to default. When the original MFD is plugged in, this is what
    // it starts with.
    smallScreen = SMALL_SCREEN_TRIP_INFO_1;

    SetAllTabIndexes();  // Copy tab index to trip computer popup and large screen

    WriteEeprom(SMALL_SCREEN_EEPROM_POS, smallScreen);
} // InitSmallScreen

// Called when an MFD status is received indicating a reset of one of the trip computers
void ResetTripInfo(uint16_t mfdStatus)
{
    // Force change to the appropriate small screen (left hand side of the display)

    uint8_t newSmallScreenIndex = mfdStatus == TRIP_COUTER_1_RESET ? SMALL_SCREEN_TRIP_INFO_1 : SMALL_SCREEN_TRIP_INFO_2;
    bool mustWrite = smallScreen != newSmallScreenIndex;
    smallScreen = newSmallScreenIndex;  // Set tab index for trip computer on small screen
    SetAllTabIndexes();  // Copy tab index to trip computer popup and large screen

#ifdef DEBUG_ORIGINAL_MFD
    Serial.printf_P(
        PSTR("[originalMfd] Right stalk button long-press; smallScreen := %S\n"),
        SmallScreenStr()
    );
#endif // DEBUG_ORIGINAL_MFD

    if (mustWrite) WriteEeprom(SMALL_SCREEN_EEPROM_POS, smallScreen);
} // ResetTripInfo

// Keep track of the cycling through the trip computer tabs (either in the small screen, the large screen or the
// popup) in the original MFD. Called after a short-press of the right hand stalk button.
// Variables 'smallScreen', 'tripComputerLargeScreenTab', and 'tripComputerPopupTab' may be updated.
void CycleTripInfo()
{
    if (! isSatnavGuidanceActive)
    {
        // A trip computer tab is showing on the small screen

        smallScreen = (smallScreen + 1) % N_SMALL_SCREENS;  // Select next tab
        SetAllTabIndexes();  // Copy tab index to trip computer popup and large screen

    #ifdef DEBUG_ORIGINAL_MFD
        Serial.printf_P(
            PSTR("[originalMfd] Right stalk button short-press; smallScreen := %S\n"),
            SmallScreenStr()
        );
    #endif // DEBUG_ORIGINAL_MFD

        WriteEeprom(SMALL_SCREEN_EEPROM_POS, smallScreen);
    }
    else if (largeScreen == LARGE_SCREEN_TRIP_COMPUTER)
    {
        // A trip computer tab is showing on the large screen

        // Select the next tab, but skip the "GPS info" screen
        tripComputerLargeScreenTab = (tripComputerLargeScreenTab + 1) % N_SMALL_SCREENS;
        if (tripComputerLargeScreenTab == SMALL_SCREEN_GPS_INFO) tripComputerLargeScreenTab = SMALL_SCREEN_FUEL_CONSUMPTION;

        tripComputerPopupTab = tripComputerLargeScreenTab;  // Copy tab index to trip computer popup

    #ifdef DEBUG_ORIGINAL_MFD
        Serial.printf_P(
            PSTR("[originalMfd] Right stalk button short-press; tripComputerLargeScreenTab := %S\n"),
            TripComputerStr(tripComputerLargeScreenTab)
        );
    #endif // DEBUG_ORIGINAL_MFD

    }
    else // isSatnavGuidanceActive && largeScreen != LARGE_SCREEN_TRIP_COMPUTER
    {
        // The trip computer popup is triggered, or is already visible

        // The logic of the original MFD is as follows:
        // - The initial press of the stalk button simply triggers the trip computer popup.
        // - As long as the trip computer popup is visible, each next short press of the stalk button leads
        //   to the next tab.

        unsigned long now = millis();

        // Arithmetic has safe roll-over
        if (! IsTripComputerPopupShowing())
        {
            // The trip computer popup was not visible: the stalk button triggers the trip computer popup to appear

        #ifdef DEBUG_ORIGINAL_MFD
            Serial.printf_P(
                PSTR(
                    "[originalMfd] Right stalk button short-press; trip computer popup appeared;"
                    " tripComputerPopupTab := %S\n"
                ),
                TripComputerStr(tripComputerPopupTab)
            );
        #endif // DEBUG_ORIGINAL_MFD
        }
        else
        {
            // The trip computer popup is visible

            // Select the next tab, but skip the "GPS info" screen
            tripComputerPopupTab = (tripComputerPopupTab + 1) % N_SMALL_SCREENS;
            if (tripComputerPopupTab == SMALL_SCREEN_GPS_INFO) tripComputerPopupTab = SMALL_SCREEN_FUEL_CONSUMPTION;

            tripComputerLargeScreenTab = tripComputerPopupTab;  // Copy tab index to large screen

        #ifdef DEBUG_ORIGINAL_MFD
            Serial.printf_P(
                PSTR(
                    "[originalMfd] Right stalk button short-press; next tab in trip computer popup;"
                    " tripComputerPopupTab := %S\n"
                ),
                TripComputerStr(tripComputerPopupTab)
            );
        #endif // DEBUG_ORIGINAL_MFD
        } // if

        TripComputerPopupShowing(now, 8000);  // (Re-)start the timer
    } // if
} // CycleTripInfo

// Called when the current street becomes known. The original MFD switches to the current street.
void UpdateLargeScreenForCurrentStreetKnown()
{
    if (! isSatnavGuidanceActive)
    {
        if (largeScreen == LARGE_SCREEN_CLOCK)
        {
            // The original MFD also waits for something else to happen before it autonomously switches to the
            // curret street in the large screen. But it is not clear what that "something else" is.
            largeScreen = LARGE_SCREEN_CURRENT_STREET;
        } // if
    } // if

#ifdef DEBUG_ORIGINAL_MFD
    Serial.printf_P(
        PSTR("[originalMfd] current street known; largeScreen := %S\n"),
        LargeScreenStr()
    );
#endif // DEBUG_ORIGINAL_MFD
} // UpdateLargeScreenForCurrentStreetKnown

// Called when the head unit is powered on (tuner, tape, internal CD or CD changer). The original MFD switches to
// the head unit (audio) screen, or shows a small notification popup.
void UpdateLargeScreenForHeadUnitOn()
{
    if (isSatnavGuidanceActive)
    {
        if (largeScreen == LARGE_SCREEN_CLOCK)
        {
            largeScreen = LARGE_SCREEN_HEAD_UNIT;
        }
        else
        {
            // largeScreen == LARGE_SCREEN_GUIDANCE || largeScreen == LARGE_SCREEN_TRIP_COMPUTER
            // Head unit shows itself in a small notification popup
            NotificationPopupShowing(millis(), 8000);
        } // if
    }
    else
    {
        largeScreen = LARGE_SCREEN_HEAD_UNIT;
    } // if

#ifdef DEBUG_ORIGINAL_MFD
    char popupDurationStr[20];
    sprintf_P(popupDurationStr, PSTR(" (%ld msec)"), popupDuration);
    Serial.printf_P(
        PSTR("[originalMfd] Head unit powered on; NotificationPopupShowing = %S%S; largeScreen := %S\n"),
        IsNotificationPopupShowing() ? yesStr : noStr,
        IsNotificationPopupShowing() ? popupDurationStr : emptyStr,
        LargeScreenStr()
    );
#endif // DEBUG_ORIGINAL_MFD
} // UpdateLargeScreenForHeadUnitOn

// Called when the head unit is powered off
void UpdateLargeScreenForHeadUnitOff()
{
    NotificationPopupShowingSince = 0;

    if (largeScreen == LARGE_SCREEN_HEAD_UNIT)
    {
        if (isSatnavGuidanceActive) largeScreen = LARGE_SCREEN_TRIP_COMPUTER;
        else largeScreen = LARGE_SCREEN_CLOCK;

        if (largeScreen == LARGE_SCREEN_CLOCK && isCurrentStreetKnown)
        {
            largeScreen = LARGE_SCREEN_CURRENT_STREET;
        } // if
    } // if

#ifdef DEBUG_ORIGINAL_MFD
    Serial.printf_P(
        PSTR("[originalMfd] Head unit powered off; guidance=%S; curr_street_known=%S; largeScreen := %S\n"),
        isSatnavGuidanceActive ? yesStr : noStr,
        isCurrentStreetKnown ? yesStr : noStr,
        LargeScreenStr()
    );
#endif // DEBUG_ORIGINAL_MFD
} // UpdateLargeScreenForHeadUnitOff

// Called when going into sat nav guidance mode.  The original MFD switches to the guidance instruction screen.
void UpdateLargeScreenForGuidanceModeOn()
{
    largeScreenBeforeGoingIntoGuidanceMode = largeScreen;  // To return to later, when guidance ends
    largeScreen = LARGE_SCREEN_GUIDANCE;

#ifdef DEBUG_ORIGINAL_MFD
    Serial.printf_P(
        PSTR("[originalMfd] Going into guidance; largeScreenBefore=%S; largeScreen := %S\n"),
        LargeScreenStr(largeScreenBeforeGoingIntoGuidanceMode),
        LargeScreenStr()
    );
#endif // DEBUG_ORIGINAL_MFD
} // UpdateLargeScreenForGuidanceModeOn

// Called when going out of sat nav guidance mode
void UpdateLargeScreenForGuidanceModeOff()
{
    // If currently visible, the trip computer tab index on the large screen is copied to the small screen.
    // Bug in original MFD: if the large screen is *not* currently visible, the small screen tab index is *not* updated.
    if (largeScreen == LARGE_SCREEN_TRIP_COMPUTER)
    {
        if (tripComputerLargeScreenTab >= 0) smallScreen = tripComputerLargeScreenTab;

    #ifdef DEBUG_ORIGINAL_MFD
        Serial.printf_P(
            PSTR("[originalMfd] Going out of guidance; switched small screen to trip computer; smallScreen := %S\n"),
            SmallScreenStr()
        );
    #endif // DEBUG_ORIGINAL_MFD

        WriteEeprom(SMALL_SCREEN_EEPROM_POS, smallScreen);
    }
    else
    {
        SetAllTabIndexes();

    #ifdef DEBUG_ORIGINAL_MFD
        Serial.printf_P(
            PSTR("[originalMfd] Going out of guidance; tripComputerPopupTab := %S\n"),
            TripComputerStr(tripComputerPopupTab)
        );
    #endif // DEBUG_ORIGINAL_MFD
    } // if

    // Large screen stays in clock, if showing
    if (largeScreen != LARGE_SCREEN_CLOCK)
    {
        if (largeScreen == LARGE_SCREEN_GUIDANCE)
        {
            largeScreen = largeScreenBeforeGoingIntoGuidanceMode;
            if (largeScreen == LARGE_SCREEN_HEAD_UNIT && ! isHeadUnitPowerOn) largeScreen = LARGE_SCREEN_CLOCK;
        }
        else if (largeScreen == LARGE_SCREEN_TRIP_COMPUTER)
        {
            largeScreen = LARGE_SCREEN_HEAD_UNIT;
            if (! isHeadUnitPowerOn) largeScreen = LARGE_SCREEN_CLOCK;
        } // if
    } // if

#ifdef DEBUG_ORIGINAL_MFD
    Serial.printf_P(
        PSTR("[originalMfd] Going out of guidance; head_unit=%S; curr_street_known=%S; largeScreen := %S\n"),
        isHeadUnitPowerOn ? yesStr : noStr,
        isCurrentStreetKnown ? yesStr : noStr,
        LargeScreenStr()
    );
#endif // DEBUG_ORIGINAL_MFD
} // UpdateLargeScreenForGuidanceModeOff

// Called after receiving an MFD status "MFD_SCREEN_OFF" packet
void UpdateLargeScreenForMfdOff()
{
    largeScreen = LARGE_SCREEN_CLOCK;
} // UpdateLargeScreenForMfdOff

// Called after receiving a "MOD" button press from the IR remote control. Variable 'largeScreen' will be
// updated.
void CycleLargeScreen()
{
    // Ignore as long as any popup is showing
    if (IsNotificationPopupShowing()) return;
    if (IsTripComputerPopupShowing()) return;

    // Keep track of the cycling through the large screens in the original MFD
    largeScreen = (largeScreen + 1) % N_LARGE_SCREENS;

    if (largeScreen == LARGE_SCREEN_GUIDANCE && ! isSatnavGuidanceActive)
    {
        largeScreen = LARGE_SCREEN_HEAD_UNIT;
    } // if

    if (largeScreen == LARGE_SCREEN_HEAD_UNIT && ! isHeadUnitPowerOn)
    {
        largeScreen = LARGE_SCREEN_CURRENT_STREET;
    } // if

    if (largeScreen == LARGE_SCREEN_CURRENT_STREET && (isSatnavGuidanceActive || ! isCurrentStreetKnown))
    {
        // TODO - even when the current street is known, the original MFD does not always cycle through it.
        // And sometimes when the current street is not known, the original MFD does cycle through it (showing an
        // empty box). So what is the condition on which the original MFD decides to start showing the current street?
        largeScreen = LARGE_SCREEN_TRIP_COMPUTER;
    } // if

    if (largeScreen == LARGE_SCREEN_TRIP_COMPUTER && ! isSatnavGuidanceActive)
    {
        largeScreen = LARGE_SCREEN_CLOCK;
    } // if

#ifdef DEBUG_ORIGINAL_MFD
    Serial.printf_P(
        PSTR("[originalMfd] \"MOD\" button press; guidance=%S; head_unit=%S; curr_street_known=%S; largeScreen := %S\n"),
        isSatnavGuidanceActive ? yesStr : noStr,
        isHeadUnitPowerOn ? yesStr : noStr,
        isCurrentStreetKnown ? yesStr : noStr,
        LargeScreenStr()
    );
#endif // DEBUG_ORIGINAL_MFD
} // CycleLargeScreen
