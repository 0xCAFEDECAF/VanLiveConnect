/*
 * VanBus packet parser - Shrink-wraps the data of recognized packets neatly into a JSON format
 *
 * Written by Erik Tromp
 *
 * MIT license, all text above must be included in any redistribution.
 */

#include "Notifications.h"

enum VanPacketParseResult_t
{
    VAN_PACKET_NO_CONTENT = 2,  // Packet is OK but contains no useful (new) content
    VAN_PACKET_DUPLICATE = 1,  // Packet was the same as the last with this IDEN field
    VAN_PACKET_PARSE_OK = 0,  // Packet was parsed OK
    VAN_PACKET_PARSE_CRC_ERROR = -1,  // Packet had a CRC error
    VAN_PACKET_PARSE_UNEXPECTED_LENGTH = -2,  // Packet had unexpected length
    VAN_PACKET_PARSE_UNRECOGNIZED_IDEN = -3,  // Packet had unrecognized IDEN field
    VAN_PACKET_PARSE_TO_BE_DECODED = -4,  // IDEN recognized but the correct parsing of this packet is not yet known
    VAN_PACKET_PARSE_JSON_TOO_LONG = -5,  // IDEN recognized but the parsing into JSON overflows 'jsonBuffer'
    VAN_PACKET_PARSE_FRAGMENT_MISSED = -6  // Missed at least one fragment of a multi-fragment message
}; // enum VanPacketParseResult_t

// Returns a PSTR (allocated in flash, saves RAM)
PGM_P VanPacketParseResultStr(int result)
{
    return
        result == VAN_PACKET_NO_CONTENT ? PSTR("NO NEW DATA CONTENT") :
        result == VAN_PACKET_DUPLICATE ? PSTR("DUPLICATE") :
        result == VAN_PACKET_PARSE_OK ? PSTR("OK") :
        result == VAN_PACKET_PARSE_CRC_ERROR ? PSTR("CRC ERROR") :
        result == VAN_PACKET_PARSE_UNEXPECTED_LENGTH ? PSTR("UNEXPECTED LENGTH") :
        result == VAN_PACKET_PARSE_UNRECOGNIZED_IDEN ? PSTR("UNRECOGNIZED IDEN VALUE") :
        result == VAN_PACKET_PARSE_TO_BE_DECODED ? PSTR("FORMAT YET TO BE DECODED") :
        result == VAN_PACKET_PARSE_JSON_TOO_LONG ? PSTR("JSON BUFFER OVERFLOW") :
        result == VAN_PACKET_PARSE_FRAGMENT_MISSED ? PSTR("MULTI-FRAGMENT DATA: FRAGMENT MISSED") :
        "ERROR_??";
} // ResultStr

typedef VanPacketParseResult_t (*TPacketParser)(TVanPacketRxDesc&, char*, const int);

struct IdenHandler_t
{
    uint16_t iden;
    const char* idenStr;
    int dataLen;
    bool ignoreDups;
    TPacketParser parser;
    int prevDataLen;
    uint8_t* prevData;
}; // struct IdenHandler_t

// Often used string constants
const char PROGMEM emptyStr[] = "";
const char PROGMEM commaStr[] = ",";
const char PROGMEM spaceStr[] = " ";
const char PROGMEM onStr[] = "ON";
const char PROGMEM offStr[] = "OFF";
const char PROGMEM yesStr[] = "YES";
const char PROGMEM noStr[] = "NO";
const char PROGMEM openStr[] = "OPEN";
const char PROGMEM closedStr[] = "CLOSED";
const char PROGMEM presentStr[] = "PRESENT";
const char PROGMEM notPresentStr[] = "NOT_PRESENT";
const char PROGMEM styleDisplayBlockStr[] = "block";  // To set 'style="display:block;"' in HTML
const char PROGMEM styleDisplayNoneStr[] = "none";  // To set 'style="display:none;"' in HTML
const char PROGMEM noneStr[] = "NONE";
const char PROGMEM updatedStr[] = "(UPD)";
const char PROGMEM notApplicable1Str[] = "-";
const char PROGMEM notApplicable2Str[] = "--";
const char PROGMEM notApplicable3Str[] = "---";
const char PROGMEM notApplicableFloatStr[] = "--.-";
PGM_P dashStr = notApplicable1Str;

// Defined in WebSocket.ino
extern bool inMenu;
extern bool satnavDisclaimerAccepted;

// Defined in PacketFilter.ino
bool IsPacketSelected(uint16_t iden, VanPacketFilter_t filter);

// Defined in OriginalMfd.ino
extern uint8_t largeScreen;
extern uint8_t mfdLanguage;
PGM_P TripComputerStr();
PGM_P SmallScreenStr();
PGM_P LargeScreenStr();
void NotificationPopupShowing(unsigned long since, unsigned long duration);
bool IsTripComputerPopupShowing();
void InitSmallScreen();
void ResetTripInfo(uint16_t mfdStatus);
void UpdateSmallScreenAfterStoppingGuidance();
void UpdateLargeScreenForCurrentStreetKnown();
void UpdateLargeScreenForHeadUnitOn();
void UpdateLargeScreenForHeadUnitOff();
void UpdateLargeScreenForGuidanceModeOn();
void UpdateLargeScreenForGuidanceModeOff();
void UpdateLargeScreenForMfdOff();
void CycleTripInfo();

// Defined in Eeprom.ino
void WriteEeprom(int const address, uint8_t const val, const char* message = 0);
void CommitEeprom();

// Defined in DateTime.ino
const char* TimeStamp();

// Forward declaration
void ResetPacketPrevData();

// Default, and unless otherwise specified, the following units are used:
// - Distance: kilometers
// - Contents: litres
// - Temperature: degrees Celsius

// Index of user-selected distance unit
enum MFD_DistanceUnit_t
{
    MFD_DISTANCE_UNIT_METRIC,
    MFD_DISTANCE_UNIT_IMPERIAL
}; // enum MFD_DistanceUnit_t

uint8_t mfdDistanceUnit = MFD_DISTANCE_UNIT_METRIC;

// Index of user-selected temperature unit
enum MFD_TemperatureUnit_t
{
    MFD_TEMPERATURE_UNIT_CELSIUS,
    MFD_TEMPERATURE_UNIT_FAHRENHEIT
}; // enum MFD_TemperatureUnit_t

uint8_t mfdTemperatureUnit = MFD_TEMPERATURE_UNIT_CELSIUS;

// Index of user-selected time unit
enum MFD_TimeUnit_t
{
    MFD_TIME_UNIT_24H,
    MFD_TIME_UNIT_12H
}; // enum MFD_TimeUnit_t

uint8_t mfdTimeUnit = MFD_TIME_UNIT_24H;

// Round downwards (even if negative) to nearest multiple of d. Safe for negative values of n and d, and for
// values of n around 0.
int32_t _floor(int32_t n, int32_t d)
{
    return (n / d - (((n > 0) ^ (d > 0)) && (n % d))) * d;
} // _floor

uint16_t ToMiles(uint16_t km)
{
    return ((uint32_t)km * 2000 + 1609) / 1609 / 2;  // Adding 1609 for correct rounding
} // ToMiles

int32_t ToMiles(int32_t km)
{
    return ((int32_t)km * 2000 + 1609) / 1609 / 2;  // Adding 1609 for correct rounding
} // ToMiles

float ToMiles(float km)
{
    return km / 1.609;
} // ToMiles

float ToYards(float m)
{
    return m / 1.094;
} // ToYards

// Note: cannot handle 0 as parameter!
float ToMilesPerGallon(float consumptionLt100_x10)
{
    return 2824.8 / consumptionLt100_x10;  // Assuming imperial gallons
} // ToMilesPerGallon

float ToFahrenheit(float temp_C)
{
    return temp_C * 1.8 + 32.0;
} // ToFahrenheit

int16_t ToFahrenheit(int16_t temp_C)
{
    return (temp_C * 90 + 50 / 2) / 50 + 32;  // Adding 50 / 2 for correct rounding
} // ToFahrenheit

float ToBar(uint8_t condenserPressure)
{
    return condenserPressure / 4.0;
} // ToBar

float ToPsi(uint8_t condenserPressure)
{
    return condenserPressure * 14.5037738 / 4.0;
} // ToPsi

float ToGallons(float litres)
{
    return litres / 4.546;  // Assuming imperial gallons
} // ToGallons

// Uses statically allocated buffer, so don't call twice within the same printf invocation
char* ToStr(uint8_t data)
{
    #define MAX_UINT8_STR_SIZE 4
    static char buffer[MAX_UINT8_STR_SIZE];
    sprintf_P(buffer, PSTR("%u"), data);

    return buffer;
} // ToStr

// Uses statically allocated buffer, so don't call twice within the same printf invocation
char* ToStr(uint16_t data)
{
    #define MAX_UINT16_STR_SIZE 6
    static char buffer[MAX_UINT16_STR_SIZE];
    sprintf_P(buffer, PSTR("%u"), data);

    return buffer;
} // ToStr

// Uses statically allocated buffer, so don't call twice within the same printf invocation
char* ToStr(int16_t data)
{
    #define MAX_INT16_STR_SIZE 7
    static char buffer[MAX_INT16_STR_SIZE];
    sprintf_P(buffer, PSTR("%d"), data);

    return buffer;
} // ToStr

// Uses statically allocated buffer, so don't call twice within the same printf invocation
char* ToBcdStr(uint8_t data)
{
    #define MAX_UINT8_BCD_STR_SIZE 3
    static char buffer[MAX_UINT8_BCD_STR_SIZE];
    sprintf_P(buffer, PSTR("%X"), data);

    return buffer;
} // ToBcdStr

// Uses statically allocated buffer, so don't call twice within the same printf invocation
char* ToHexStr(uint8_t data)
{
    #define MAX_UINT8_HEX_STR_SIZE 5
    static char buffer[MAX_UINT8_HEX_STR_SIZE];
    sprintf_P(buffer, PSTR("0x%02X"), data);

    return buffer;
} // ToHexStr

// Uses statically allocated buffer, so don't call twice within the same printf invocation
char* ToHexStr(uint8_t data1, uint8_t data2)
{
    #define MAX_2_UINT8_HEX_STR_SIZE 10
    static char buffer[MAX_2_UINT8_HEX_STR_SIZE];
    sprintf_P(buffer, PSTR("0x%02X-0x%02X"), data1, data2);

    return buffer;
} // ToHexStr

// Uses statically allocated buffer, so don't call twice within the same printf invocation
char* ToHexStr(uint16_t data)
{
    #define MAX_UINT16_HEX_STR_SIZE 7
    static char buffer[MAX_UINT16_HEX_STR_SIZE];
    sprintf_P(buffer, PSTR("0x%04X"), data);

    return buffer;
} // ToHexStr

// Generate a string representation of a float value.
// Note: passed buffer size must be (at least) MAX_FLOAT_SIZE bytes, e.g. declare like this:
//   char buffer[MAX_FLOAT_SIZE];
char decimalSeparatorChar = '.';
char* ToFloatStr(char* buffer, float f, int prec, bool useLocalizedDecimalSeparatorChar = true)
{
    dtostrf(f, MAX_FLOAT_SIZE - 1, prec, buffer);

    // Strip leading spaces
    char* strippedStr = buffer;
    while (isspace(*strippedStr)) strippedStr++;

    if (prec > 0 && useLocalizedDecimalSeparatorChar && decimalSeparatorChar != '.')
    {
        char* p  = strchr(buffer, '.');
        if (p) *p = decimalSeparatorChar;
    } // if

    return strippedStr;
} // ToFloatStr

// Replace special (e.g. extended Ascii) characters by their HTML-safe representation.
// See also: https://www.ascii-code.com/ .
void AsciiToHtml(String& in)
{
    for (unsigned int i = 0; i < in.length(); i++)
    {
        char c = in.c_str()[i];

        // Vast majority of characters is printable, so this if-statement is entered seldomly
        if (c > 127)
        {
            // Hope this is somewhat efficient...
            String from(c);
            String to((int)c, DEC);
            to = "&#" + to + ";";
            in.replace(from, to);  // in.length() will become larger; no problem
            i += 5;  // We can skip the just added characters
        } // if
    } // for
} // ToHtml

// Pretty-print a JSON formatted string, adding indentation
void PrintJsonText(const char* jsonBuffer)
{
    // Number of spaces to add for each indentation level
    #define PRETTY_PRINT_JSON_INDENT 2

    size_t j = 0;
    int indent = 0;
    while (j < strlen(jsonBuffer))
    {
        const char* subString = jsonBuffer + j;

        if (subString[0] == '}' || subString[0] == ']') indent -= PRETTY_PRINT_JSON_INDENT;

        size_t n = strcspn(subString, "\n");
        if (n != strlen(subString)) Serial.printf("%*s%.*s\n", indent, "", n, subString);
        j = j + n + 1;

        if (subString[0] == '{' || subString[0] == '[') indent += PRETTY_PRINT_JSON_INDENT;
    } // while
} // PrintJsonText

// Tuner band
enum TunerBand_t
{
    TB_NONE = 0,
    TB_FM1,
    TB_FM2,
    TB_FM3,  // Never seen, just guessing
    TB_FMAST,  // Auto-station
    TB_AM,
    TB_PTY_SELECT = 7
}; // enum TunerBand_t

// Returns a PSTR (allocated in flash, saves RAM)
PGM_P TunerBandStr(uint8_t data)
{
    return
        data == TB_NONE ? noneStr :
        data == TB_FM1 ? PSTR("FM1") :
        data == TB_FM2 ? PSTR("FM2") :
        data == TB_FM3 ? PSTR("FM3") :  // Never seen, just guessing
        data == TB_FMAST ? PSTR("FMAST") :
        data == TB_AM ? PSTR("AM") :
        data == TB_PTY_SELECT ? PSTR("PTY_SELECT") :  // When selecting PTY to search for
        notApplicable3Str;
} // TunerBandStr

// Tuner search mode
// Bits:
//  2  1  0
// ---+--+---
//  0  0  0 : Not searching
//  0  0  1 : Manual tuning
//  0  1  0 : Searching by frequency
//  0  1  1 :
//  1  0  0 : Searching for station with matching PTY
//  1  0  1 :
//  1  1  0 :
//  1  1  1 : Auto-station search in the FMAST band (long-press "Radio Band" button)
enum TunerSearchMode_t
{
    TS_NOT_SEARCHING = 0,
    TS_MANUAL = 1,
    TS_BY_FREQUENCY = 2,
    TS_BY_MATCHING_PTY = 4,
    TS_FM_AST = 7
}; // enum TunerSearchMode_t

// Returns a PSTR (allocated in flash, saves RAM)
PGM_P TunerSearchModeStr(uint8_t data)
{
    return
        data == TS_NOT_SEARCHING ? PSTR("NOT_SEARCHING") :
        data == TS_MANUAL ? PSTR("MANUAL_TUNING") :
        data == TS_BY_FREQUENCY ? PSTR("SEARCHING_BY_FREQUENCY") :
        data == TS_BY_MATCHING_PTY ? PSTR("SEARCHING_MATCHING_PTY") : // Searching for station with matching PTY
        data == TS_FM_AST ? PSTR("FM_AST_SCAN") : // AutoSTore scan in the FMAST band (long-press "Radio Band" button)
        notApplicable3Str;
} // TunerSearchModeStr

// "Full" PTY string
// Returns a PSTR (allocated in flash, saves RAM)
PGM_P PtyStrFull(uint8_t ptyCode)
{
    // See also:
    // https://www.electronics-notes.com/articles/audio-video/broadcast-audio/rds-radio-data-system-pty-codes.php
    return
        ptyCode ==  0 ? PSTR("Not defined") :
        ptyCode ==  1 ? PSTR("News") :
        ptyCode ==  2 ? PSTR("Current affairs") :
        ptyCode ==  3 ? PSTR("Information") :
        ptyCode ==  4 ? PSTR("Sport") :
        ptyCode ==  5 ? PSTR("Education") :
        ptyCode ==  6 ? PSTR("Drama") :
        ptyCode ==  7 ? PSTR("Culture") :
        ptyCode ==  8 ? PSTR("Science") :
        ptyCode ==  9 ? PSTR("Varied") :
        ptyCode == 10 ? PSTR("Pop Music") :
        ptyCode == 11 ? PSTR("Rock Music") :
        ptyCode == 12 ? PSTR("Easy Listening") :  // also: "Middle of the road music"
        ptyCode == 13 ? PSTR("Light Classical") :
        ptyCode == 14 ? PSTR("Serious Classical") :
        ptyCode == 15 ? PSTR("Other Music") :
        ptyCode == 16 ? PSTR("Weather") :
        ptyCode == 17 ? PSTR("Finance") :
        ptyCode == 18 ? PSTR("Children’s Programmes") :
        ptyCode == 19 ? PSTR("Social Affairs") :
        ptyCode == 20 ? PSTR("Religion") :
        ptyCode == 21 ? PSTR("Phone-in") :
        ptyCode == 22 ? PSTR("Travel") :
        ptyCode == 23 ? PSTR("Leisure") :
        ptyCode == 24 ? PSTR("Jazz Music") :
        ptyCode == 25 ? PSTR("Country Music") :
        ptyCode == 26 ? PSTR("National Music") :
        ptyCode == 27 ? PSTR("Oldies Music") :
        ptyCode == 28 ? PSTR("Folk Music") :
        ptyCode == 29 ? PSTR("Documentary") :
        ptyCode == 30 ? PSTR("Alarm Test") :
        ptyCode == 31 ? PSTR("Alarm") :
        notApplicable3Str;
} // PtyStrFull

// 16-character PTY string
// See: poupa.cz/rds/r98_009_2.pdf, page 2
// Returns a PSTR (allocated in flash, saves RAM)
PGM_P PtyStr16(uint8_t ptyCode)
{
    // See also:
    // https://www.electronics-notes.com/articles/audio-video/broadcast-audio/rds-radio-data-system-pty-codes.php
    return
        ptyCode ==  0 ? PSTR("None") :
        ptyCode ==  1 ? PSTR("News") :
        ptyCode ==  2 ? PSTR("Current Affairs") :
        ptyCode ==  3 ? PSTR("Information") :
        ptyCode ==  4 ? PSTR("Sport") :
        ptyCode ==  5 ? PSTR("Education") :
        ptyCode ==  6 ? PSTR("Drama") :
        ptyCode ==  7 ? PSTR("Cultures") :
        ptyCode ==  8 ? PSTR("Science") :
        ptyCode ==  9 ? PSTR("Varied Speech") :
        ptyCode == 10 ? PSTR("Pop Music") :
        ptyCode == 11 ? PSTR("Rock Music") :
        ptyCode == 12 ? PSTR("Easy Listening") :
        ptyCode == 13 ? PSTR("Light Classics M") :
        ptyCode == 14 ? PSTR("Serious Classics") :
        ptyCode == 15 ? PSTR("Other Music") :
        ptyCode == 16 ? PSTR("Weather & Metr") :
        ptyCode == 17 ? PSTR("Finance") :
        ptyCode == 18 ? PSTR("Children’s Progs") :
        ptyCode == 19 ? PSTR("Social Affairs") :
        ptyCode == 20 ? PSTR("Religion") :
        ptyCode == 21 ? PSTR("Phone In") :
        ptyCode == 22 ? PSTR("Travel & Touring") :
        ptyCode == 23 ? PSTR("Leisure & Hobby") :
        ptyCode == 24 ? PSTR("Jazz Music") :
        ptyCode == 25 ? PSTR("Country Music") :
        ptyCode == 26 ? PSTR("National Music") :
        ptyCode == 27 ? PSTR("Oldies Music") :
        ptyCode == 28 ? PSTR("Folk Music") :
        ptyCode == 29 ? PSTR("Documentary") :
        ptyCode == 30 ? PSTR("Alarm Test") :
        ptyCode == 31 ? PSTR("Alarm-Alarm!") :
        notApplicable3Str;
} // PtyStr16

// 8-character PTY string
// See: poupa.cz/rds/r98_009_2.pdf, page 2
// Returns a PSTR (allocated in flash, saves RAM)
PGM_P PtyStr8(uint8_t ptyCode)
{
    // See also:
    // https://www.electronics-notes.com/articles/audio-video/broadcast-audio/rds-radio-data-system-pty-codes.php
    return
        ptyCode ==  0 ? PSTR("None") :
        ptyCode ==  1 ? PSTR("News") :
        ptyCode ==  2 ? PSTR("Affairs") :
        ptyCode ==  3 ? PSTR("Info") :
        ptyCode ==  4 ? PSTR("Sport") :
        ptyCode ==  5 ? PSTR("Educate") :
        ptyCode ==  6 ? PSTR("Drama") :
        ptyCode ==  7 ? PSTR("Culture") :
        ptyCode ==  8 ? PSTR("Science") :
        ptyCode ==  9 ? PSTR("Varied") :
        ptyCode == 10 ? PSTR("Pop M") :
        ptyCode == 11 ? PSTR("Rock M") :
        ptyCode == 12 ? PSTR("Easy M") :
        ptyCode == 13 ? PSTR("Light M") :
        ptyCode == 14 ? PSTR("Classics") :
        ptyCode == 15 ? PSTR("Other M") :
        ptyCode == 16 ? PSTR("Weather") :
        ptyCode == 17 ? PSTR("Finance") :
        ptyCode == 18 ? PSTR("Children") :
        ptyCode == 19 ? PSTR("Social") :
        ptyCode == 20 ? PSTR("Religion") :
        ptyCode == 21 ? PSTR("Phone In") :
        ptyCode == 22 ? PSTR("Travel") :
        ptyCode == 23 ? PSTR("Leisure") :
        ptyCode == 24 ? PSTR("Jazz") :
        ptyCode == 25 ? PSTR("Country") :
        ptyCode == 26 ? PSTR("Nation M") :
        ptyCode == 27 ? PSTR("Oldies") :
        ptyCode == 28 ? PSTR("Folk M") :
        ptyCode == 29 ? PSTR("Document") :
        ptyCode == 30 ? PSTR("TEST") :
        ptyCode == 31 ? PSTR("Alarm") :
        notApplicable3Str;
} // PtyStr8

// Returns a PSTR (allocated in flash, saves RAM)
PGM_P RadioPiCountry(uint8_t countryCode)
{
    // See also:
    // - http://poupa.cz/rds/countrycodes.htm
    // - https://radio-tv-nederland.nl/rds/PI%20codes%20Europe.jpg
    // Note: more than one country is assigned to the same code, just listing the most likely.
    return
        countryCode == 0x01 || countryCode == 0x0D ? PSTR("DE") :  // Germany
        countryCode == 0x02 ? PSTR("IE") :  // Ireland
        countryCode == 0x03 ? PSTR("PL") :  // Poland
        countryCode == 0x04 ? PSTR("CH") :  // Switzerland
        countryCode == 0x05 ? PSTR("IT") :  // Italy
        countryCode == 0x06 ? PSTR("BEL") :  // Belgium
        countryCode == 0x07 ? PSTR("LUX") :  // Luxemburg
        countryCode == 0x08 ? PSTR("NL") :  // Netherlands
        countryCode == 0x09 ? PSTR("DNK") :  // Denmark
        countryCode == 0x0A ? PSTR("AUT") :  // Austria
        countryCode == 0x0B ? PSTR("HU") :  // Hungary
        countryCode == 0x0C ? PSTR("GB") :  // United Kingdom
        countryCode == 0x0E ? PSTR("ES") :  // Spain
        countryCode == 0x0F ? PSTR("FR") :  // France
        notApplicable2Str;
} // RadioPiCountry

// Returns a PSTR (allocated in flash, saves RAM)
PGM_P RadioPiAreaCoverage(uint8_t coverageCode)
{
    // https://www.pira.cz/rds/show.asp?art=rds_encoder_support
    return
        coverageCode == 0x00 ? PSTR("local") :
        coverageCode == 0x01 ? PSTR("international") :
        coverageCode == 0x02 ? PSTR("national") :
        coverageCode == 0x03 ? PSTR("supra-regional") :
        PSTR("regional");
} // RadioPiAreaCoverage

// Seems to be used in bus packets with IDEN:
//
// - SATNAV_REPORT_IDEN (0x6CE): data[1]. Following values of data[1] seen:
//   0x02, 0x05, 0x08, 0x09, 0x0E, 0x0F, 0x10, 0x11, 0x13, 0x1B, 0x1D
// - SATNAV_TO_MFD_IDEN (0x74E): data[1]. Following values of data[1] seen:
//   0x02, 0x05, 0x08, 0x09, 0x1B, 0x1C
// - MFD_TO_SATNAV_IDEN (0x94E): data[0]. Following values of data[1] seen:
//   0x02, 0x05, 0x06, 0x08, 0x09, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x1B, 0x1C, 0x1D
//
enum SatNavRequest_t
{
    SR_ENTER_COUNTRY = 0x00,  // Never seen, just guessing
    SR_ENTER_PROVINCE = 0x01,  // Never seen, just guessing
    SR_ENTER_CITY = 0x02,
    SR_ENTER_DISTRICT = 0x03,  // Never seen, just guessing
    SR_ENTER_NEIGHBORHOOD = 0x04,  // Never seen, just guessing
    SR_ENTER_STREET = 0x05,
    SR_ENTER_HOUSE_NUMBER = 0x06,  // Range of house numbers to choose from
    SR_ENTER_HOUSE_NUMBER_LETTER = 0x07,  // Never seen, just guessing
    SR_SERVICE_LIST = 0x08,
    SR_SERVICE_ADDRESS = 0x09,
    SR_ARCHIVE_IN_DIRECTORY = 0x0B,
    SR_RENAME_DIRECTORY_ENTRY = 0x0D,
    SR_LAST_DESTINATION = 0x0E,
    SR_NEXT_STREET = 0x0F,  // Shown during SatNav guidance in the (solid line) top box
    SR_CURRENT_STREET = 0x10,  // Shown during SatNav guidance in the (dashed line) bottom box
    SR_PERSONAL_ADDRESS = 0x11,
    SR_PROFESSIONAL_ADDRESS = 0x12,
    SR_SOFTWARE_MODULE_VERSIONS = 0x13,
    SR_PERSONAL_ADDRESS_LIST = 0x1B,
    SR_PROFESSIONAL_ADDRESS_LIST = 0x1C,
    SR_DESTINATION = 0x1D
}; // enum SatNavRequest_t

// Returns a PSTR (allocated in flash, saves RAM)
PGM_P SatNavRequestStr(uint8_t data)
{
    return
        data == SR_ENTER_COUNTRY ? PSTR("enter_country") :
        data == SR_ENTER_PROVINCE ? PSTR("enter_province") :
        data == SR_ENTER_CITY ? PSTR("enter_city") :
        data == SR_ENTER_DISTRICT ? PSTR("enter_district") :
        data == SR_ENTER_NEIGHBORHOOD ? PSTR("enter_neighborhood") :
        data == SR_ENTER_STREET ? PSTR("enter_street") :
        data == SR_ENTER_HOUSE_NUMBER ? PSTR("enter_number") :
        data == SR_ENTER_HOUSE_NUMBER_LETTER ? PSTR("enter_letter") :
        data == SR_SERVICE_LIST ? PSTR("service") :
        data == SR_SERVICE_ADDRESS ? PSTR("service_address") :
        data == SR_ARCHIVE_IN_DIRECTORY ? PSTR("archive_in_directory") :
        data == SR_RENAME_DIRECTORY_ENTRY ? PSTR("rename_directory_entry") :
        data == SR_LAST_DESTINATION ? PSTR("last_destination") :  // TODO - change to SR_CURR_DESTINATION - "current_destination"
        data == SR_NEXT_STREET ? PSTR("next_street") :
        data == SR_CURRENT_STREET ? PSTR("current_street") :
        data == SR_PERSONAL_ADDRESS ? PSTR("personal_address") :
        data == SR_PROFESSIONAL_ADDRESS ? PSTR("professional_address") :
        data == SR_SOFTWARE_MODULE_VERSIONS ? PSTR("software_module_versions") :
        data == SR_PERSONAL_ADDRESS_LIST ? PSTR("personal_address_list") :
        data == SR_PROFESSIONAL_ADDRESS_LIST ? PSTR("professional_address_list") :
        data == SR_DESTINATION ? PSTR("current_destination") :  // TODO - change to SR_NEW_DESTINATION - "new_destination"
        ToHexStr(data);
} // SatNavRequestStr

enum SatNavRequestType_t
{
    SRT_REQ_N_ITEMS = 0,  // Request for number of items
    SRT_REQ_ITEMS = 1,  // Request (single or list of) item(s)
    SRT_SELECT = 2,  // Select item
    SRT_SELECT_CITY_CENTER = 3  // Select city center
}; // enum SatNavRequestType_t

// Returns a PSTR (allocated in flash, saves RAM)
PGM_P SatNavRequestTypeStr(uint8_t data)
{
    return
        data == SRT_REQ_N_ITEMS ? PSTR("REQ_N_ITEMS") :
        data == SRT_REQ_ITEMS ? PSTR("REQ_ITEMS") :
        data == SRT_SELECT ? PSTR("SELECT") :
        data == SRT_SELECT_CITY_CENTER ? PSTR("SELECT_CITY_CENTER") :
        ToStr(data);
} // SatNavRequestTypeStr

enum SatNavGuidancePreference_t
{
    SGP_FASTEST_ROUTE = 0x01,
    SGP_SHORTEST_DISTANCE = 0x04,
    SGP_AVOID_HIGHWAY = 0x12,
    SGP_COMPROMISE_FAST_SHORT = 0x02,

    SGP_INVALID = 0x00
}; // enum SatNavGuidancePreference_t

bool IsValidSatNavGuidancePreferenceValue(uint8_t value)
{
    return
        value == SGP_FASTEST_ROUTE
        || value == SGP_SHORTEST_DISTANCE
        || value == SGP_AVOID_HIGHWAY
        || value == SGP_COMPROMISE_FAST_SHORT;
} // IsValidSatNavGuidancePreferenceValue

// Returns a PSTR (allocated in flash, saves RAM)
PGM_P SatNavGuidancePreferenceStr(uint8_t data)
{
    return
        data == SGP_FASTEST_ROUTE ? PSTR("FASTEST_ROUTE") :
        data == SGP_SHORTEST_DISTANCE ? PSTR("SHORTEST_DISTANCE") :
        data == SGP_AVOID_HIGHWAY ? PSTR("AVOID_HIGHWAY") :
        data == SGP_COMPROMISE_FAST_SHORT ? PSTR("COMPROMISE_FAST_SHORT") :
        notApplicable3Str;
} // SatNavGuidancePreferenceStr

// Convert SatNav guidance instruction icon details to JSON
//
// A detailed SatNav guidance instruction consists of 8 bytes:
// * 0   : turn angle in increments of 22.5 degrees, measured clockwise, starting with 0 at 6 o-clock.
//         E.g.: 0x4 == 90 degrees left, 0x8 = 180 degrees = straight ahead, 0xC = 270 degrees = 90 degrees right.
// * 1   : always 0x00 ??
// * 2, 3: bit pattern indicating which legs are present in the junction or roundabout. Each bit set is for one leg.
//         Lowest bit of byte 3 corresponds to the leg of 0 degrees (straight down, which is
//         always there, because that is where we are currently driving), running clockwise up to the
//         highest bit of byte 2, which corresponds to a leg of 337.5 degrees (very sharp right).
// * 4, 5: bit pattern indicating which legs in the junction are "no entry". The coding of the bits is the same
//         as for bytes 2 and 3.
// * 6   : always 0x00 ??
// * 7   : always 0x00 ??
//
void GuidanceInstructionIconJson(const char* iconName, const uint8_t data[8], char* buf, int& at, const int n)
{
    // Show all the legs in the junction

    // Use "namespace" notation
    at += at >= n ? 0 : snprintf_P(buf + at, n - at, PSTR(",\n\"%s_leg_\":\n{\n"), iconName);

    uint16_t legBits = (uint16_t)data[2] << 8 | data[3];
    for (int legBit = 1; legBit < 16; legBit++)
    {
        uint16_t degrees10 = legBit * 225;
        at += at >= n ? 0 :
            snprintf_P(buf + at, n - at, PSTR("%s\"%u_%u\": \"%s\""),
                legBit == 1 ? emptyStr : PSTR(",\n"),
                degrees10 / 10,
                degrees10 % 10,
                legBits & (1 << legBit) ? onStr : offStr
            );
    } // for

    // Show all the "no-entry" legs in the junction

    // Use "namespace" notation
    at += at >= n ? 0 : snprintf_P(buf + at, n - at, PSTR("\n},\n\"%s_no_entry_\":\n{\n"), iconName);

    uint16_t noEntryBits = (uint16_t)data[4] << 8 | data[5];
    for (int noEntryBit = 1; noEntryBit < 16; noEntryBit++)
    {
        uint16_t degrees10 = noEntryBit * 225;
        at += at >= n ? 0 :
            snprintf_P(buf + at, n - at, PSTR("%s\"%u_%u\": \"%s\""),
                noEntryBit == 1 ? emptyStr : PSTR(",\n"),
                degrees10 / 10,
                degrees10 % 10,
                noEntryBits & (1 << noEntryBit) ? onStr : offStr
            );
    } // for

    // Show the direction to go

    uint16_t direction = data[0] * 225;

    const static char jsonFormatter[] PROGMEM =
        "\n"
        "},\n"
        "\"%s_direction_as_text\": \"%u.%u\",\n"  // degrees
        "\"%s_direction\":\n"
        "{\n"
            "\"style\":\n"
            "{\n"
                "\"transform\": \"rotate(%u.%udeg)\"\n"
            "}\n"
        "}";

    at += at >= n ? 0 :
        snprintf_P(buf + at, n - at, jsonFormatter,
            iconName,
            direction / 10,
            direction % 10,
            iconName,
            direction / 10,
            direction % 10
        );
} // GuidanceInstructionIconJson

enum Fuel_t
{
    FUEL_PETROL,
    FUEL_DIESEL
}; // Fuel_t

#define VIN_NUMBER_LENGTH 17
char vinNumber[VIN_NUMBER_LENGTH + 1] = {0};
int fuelType = FUEL_PETROL;

VanPacketParseResult_t ParseVinPkt(TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#E24
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#E24

    const uint8_t* data = pkt.Data();

    // Only continue if content is valid
    int i = 0;
    while (data[i] != 0 && i < VIN_NUMBER_LENGTH)
    {
        if (! ((data[i] >= 'A' && data[i] <= 'Z') || (data[i] >= '0' && data[i] <= '9'))) return VAN_PACKET_NO_CONTENT;
        i++;
    } // for

    // Engine (fuel) types:
    // - "8" = XUD: diesel
    // - "F" = ES9, EW10: petrol
    // - "H" = XU10, DW10 (HDI): diesel
    //
    // See also: http://www.peugeotlogic.com/workshop/wshtml/specs/vincode.htm
    char engineType = data[6];

    strncpy(vinNumber, (const char*) data, VIN_NUMBER_LENGTH);
    vinNumber[VIN_NUMBER_LENGTH] = 0;

    switch (engineType)
    {
        case '8':
        case 'H': fuelType = FUEL_DIESEL; break;
        case 'F': fuelType = FUEL_PETROL; break;
        // Default already set to FUEL_PETROL
    } // switch

    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"vin\": \"%-17.17s\"\n"
        "}\n"
    "}\n";

    int at = snprintf_P(buf, n, jsonFormatter, data);

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseVinPkt

// Returns a PSTR (allocated in flash, saves RAM)
PGM_P ContactKeyPositionStr(int data)
{
    return
        data == CKP_OFF ? offStr :
        data == CKP_ACC ? PSTR("ACC") :
        data == CKP_START ? PSTR("START") :
        data == CKP_ON ? onStr :
        notApplicable3Str;
} // TunerBandStr

// Set to true to disable (once) duplicate detection. For use when switching to other units.
bool SkipEnginePktDupDetect = false;

int contactKeyPosition = CKP_OFF;
bool economyMode = false;

VanPacketParseResult_t ParseEnginePkt(TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#8A4
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#8A4

    const uint8_t* data = pkt.Data();

    // Avoid continuous updates if the exterior temperature value is constantly toggling between 2 values, while
    // the rest of the data is the same.

    static uint16_t prevDashLightStatus = 0xFFFF;  // Value that can never come from a packet itself
    uint8_t dashLightStatus = data[0];

    static uint16_t prevStatusBits = 0xFFFF;  // Value that can never come from a packet itself
    uint8_t statusBits = data[1];

    static uint16_t prevCoolantTempRaw = 0xFFFF;  // Value that can never come from a packet itself
    static uint8_t coolantTempRaw = 0xFF;

    // Coolant water temperature value often falls back to 'invalid' (0xFF), even if a valid value was previously
    // found. Keep the last reported valid value.
    if (data[2] != 0xFF) coolantTempRaw = data[2];  // Copy only if packet value is valid

    static uint32_t prevOdometerRaw = 0xFFFFFFFF;  // Value that can never come from a packet itself
    uint32_t odometerRaw = (uint32_t)data[3] << 16 | (uint32_t)data[4] << 8 | data[5];

    static uint16_t lastReportedExteriorTempRaw = 0xFFFF;  // Value that can never come from a packet itself
    uint8_t exteriorTempRaw = data[6];
    static uint8_t prevExteriorTempRaw = exteriorTempRaw;

    bool hasNewData =
        SkipEnginePktDupDetect
        || dashLightStatus != prevDashLightStatus
        || statusBits != prevStatusBits
        || coolantTempRaw != prevCoolantTempRaw
        || odometerRaw != prevOdometerRaw
        || (exteriorTempRaw != lastReportedExteriorTempRaw && exteriorTempRaw == prevExteriorTempRaw);

    SkipEnginePktDupDetect = false;
    prevDashLightStatus = dashLightStatus;
    prevStatusBits = statusBits;
    prevCoolantTempRaw = coolantTempRaw;
    prevOdometerRaw = odometerRaw;
    prevExteriorTempRaw = exteriorTempRaw;

    if (! hasNewData) return VAN_PACKET_NO_CONTENT;

    lastReportedExteriorTempRaw = exteriorTempRaw;

    contactKeyPosition = statusBits & 0x03;
    economyMode = statusBits & 0x10;
    bool isCoolantTempValid = coolantTempRaw != 0xFF;
    int16_t coolantTemp = (uint16_t)coolantTempRaw - 38;
    float odometer = odometerRaw / 10.0;
    float exteriorTemp = exteriorTempRaw / 2.0 - 40;

    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"dash_light\": \"%s\",\n"
            "\"dash_actual_brightness\": \"%u\",\n"
            "\"contact_key_position\": \"%s\",\n"
            "\"engine_running\": \"%s\",\n"
            "\"economy_mode\": \"%s\",\n"
            "\"in_reverse\": \"%s\",\n"
            "\"trailer\": \"%s\",\n"
            "\"coolant_temp\": \"%s\",\n"
            "\"coolant_temp_perc\":\n"
            "{\n"
                "\"style\":\n"
                "{\n"
                    "\"transform\": \"scaleX(%s)\"\n"
                "}\n"
            "},\n"
            "\"odometer_1\": \"%s\",\n"
            "\"exterior_temp\": \"%s\",\n"  // Machine format, e.g. "3.0"
            "\"exterior_temp_loc\": \"%s\"\n"  // Localized, e.g. "3,5" or "3.5", depending on language
        "}\n"
    "}\n";

    char floatBuf[4][MAX_FLOAT_SIZE];
    int at = snprintf_P(buf, n, jsonFormatter,

        dashLightStatus & 0x80 ? PSTR("FULL") : PSTR("DIMMED (LIGHTS ON)"),
        dashLightStatus & 0x0F,

        ContactKeyPositionStr(contactKeyPosition),

        statusBits & 0x04 ? yesStr : noStr,
        economyMode ? onStr : offStr,
        statusBits & 0x20 ? yesStr : noStr,
        statusBits & 0x40 ? presentStr : notPresentStr,

        ! isCoolantTempValid ? notApplicable3Str :
            mfdTemperatureUnit == MFD_TEMPERATURE_UNIT_CELSIUS ?
                ToStr(coolantTemp) :
                ToStr(ToFahrenheit(coolantTemp)),

        // TODO - hard coded value 130 degrees Celsius for 100%
        #define MAX_COOLANT_TEMP (130)
        ! isCoolantTempValid || coolantTemp <= 0 ? PSTR("0") :
            coolantTemp >= MAX_COOLANT_TEMP ? PSTR("1") :
                ToFloatStr(floatBuf[0], (float)coolantTemp / MAX_COOLANT_TEMP, 2, false),

        mfdDistanceUnit == MFD_DISTANCE_UNIT_METRIC ?
            ToFloatStr(floatBuf[1], odometer, 1) :
            ToFloatStr(floatBuf[1], ToMiles(odometer), 1),

        mfdTemperatureUnit == MFD_TEMPERATURE_UNIT_CELSIUS ?
            ToFloatStr(floatBuf[2], exteriorTemp, 1, false) :
            ToFloatStr(floatBuf[2], ToFahrenheit(exteriorTemp), 0, false),

        mfdTemperatureUnit == MFD_TEMPERATURE_UNIT_CELSIUS ?
            ToFloatStr(floatBuf[3], exteriorTemp, 1) :
            ToFloatStr(floatBuf[3], ToFahrenheit(exteriorTemp), 0)
    );

    // TODO? - if contactKeyPosition changed to 0x00 ("OFF"), and satnavStatus2 == 0x05 ("IN_GUIDANCE_MODE"), then
    // store a boolean indicating to ask for continuation of guidance.

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseEnginePkt

VanPacketParseResult_t ParseHeadUnitStalkPkt(TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#9C4
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#9C4

    const uint8_t* data = pkt.Data();

    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"head_unit_stalk_button_next\": \"%s\",\n"
            "\"head_unit_stalk_button_prev\": \"%s\",\n"
            "\"head_unit_stalk_button_volume_up\": \"%s\",\n"
            "\"head_unit_stalk_button_volume_down\": \"%s\",\n"
            "\"head_unit_stalk_button_source\": \"%s\",\n"
            "\"head_unit_stalk_wheel\": \"%d\",\n"
            "\"head_unit_stalk_wheel_rollover\": \"%u\"\n"
        "}\n"
    "}\n";

    int at = snprintf_P(buf, n, jsonFormatter,

        data[0] & 0x80 ? onStr : offStr,
        data[0] & 0x40 ? onStr : offStr,
        data[0] & 0x08 ? onStr : offStr,
        data[0] & 0x04 ? onStr : offStr,
        data[0] & 0x02 ? onStr : offStr,

        data[1] - 0x80,
        data[0] >> 4 & 0x03
    );

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseHeadUnitStalkPkt

#define FUEL_LEVEL_PERCENTAGE_INVALID (-1)
int fuelLevelPercentage = FUEL_LEVEL_PERCENTAGE_INVALID;

const char PROGMEM fuelLevelPercentageFormatter[] =
    ",\n"
    "\"fuel_level\": \"%u\",\n"
    "\"fuel_level_unit\": \"%%\",\n"
    "\"fuel_level_perc\":\n"
    "{\n"
        "\"style\":\n"
        "{\n"
            "\"transform\": \"scaleX(%s)\"\n"
        "}\n"
    "}";

bool doorOpen = false;

// At most "DIPPED_BEAM HIGH_BEAM FOG_FRONT FOG_REAR "
#define LIGHTS_STRING_LEN 50
char lightsStr[LIGHTS_STRING_LEN] = "";

VanPacketParseResult_t ParseLightsStatusPkt(TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#4FC
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#4FC_1
    // https://github.com/morcibacsi/PSAVanCanBridge/blob/master/src/Van/Structs/VanInstrumentClusterV1Structs.h

    int dataLen = pkt.DataLen();
    if (dataLen != 11 && dataLen != 14) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

    const uint8_t* data = pkt.Data();

    doorOpen = data[1] & 0x01;

    uint16_t remainingKmToService20 = (uint16_t)(data[2] & 0x7F) << 8 | data[3];
    bool remainingKmToServiceOverdue = data[2] & 0x80;

    int32_t remainingKmToService = remainingKmToService20 * 20;
    if (remainingKmToServiceOverdue) remainingKmToService = - remainingKmToService;

    snprintf_P(lightsStr, LIGHTS_STRING_LEN, PSTR("%s%s%s%s"),
        data[5] & 0x80 ? PSTR("DIPPED_BEAM ") : emptyStr,
        data[5] & 0x40 ? PSTR("HIGH_BEAM ") : emptyStr,
        data[5] & 0x20 ? PSTR("FOG_FRONT ") : emptyStr,
        data[5] & 0x10 ? PSTR("FOG_REAR ") : emptyStr
    );

    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"instrument_cluster\": \"%sENALED\",\n"
            "\"speed_regulator_wheel\": \"%s\",\n"
            "\"hazard_lights\": \"%s\",\n"
            "\"diesel_glow_plugs\": \"%s\",\n"
            "\"door_open\": \"%s\",\n"
            "\"distance_to_service\": \"%" PRId32 "\",\n"
            "\"distance_to_service_dash\": \"%" PRId32 "\",\n"
            "\"distance_to_service_perc\":\n"
            "{\n"
                "\"style\":\n"
                "{\n"
                    "\"transform\": \"scaleX(%s)\"\n"
                "}\n"
            "},\n"
            "\"lights\": \"%s%s%s\"";

    char floatBuf[MAX_FLOAT_SIZE];
    int at = snprintf_P(buf, n, jsonFormatter,

        data[0] & 0x80 ? emptyStr : PSTR("NOT "),
        data[0] & 0x40 ? onStr : offStr,
        data[0] & 0x20 ? onStr : offStr,
        data[0] & 0x04 ? onStr : offStr,
        doorOpen ? yesStr : noStr,

        mfdDistanceUnit == MFD_DISTANCE_UNIT_METRIC ?
            remainingKmToService :
            ToMiles(remainingKmToService),

        // Round downwards (even if negative) to nearest multiple of 100
        // TODO - not sure if the "miles" type instrument cluster also rounds down like this; I don't have one
        mfdDistanceUnit == MFD_DISTANCE_UNIT_METRIC ?
            _floor(remainingKmToService, 100) :
            _floor(ToMiles(remainingKmToService), 100),

        // TODO - hard coded value 20,000 kms for 100%
        #define SERVICE_INTERVAL (20000)
        remainingKmToService <= 0 ? PSTR("0") :
            remainingKmToService >= SERVICE_INTERVAL ? PSTR("1") :
                ToFloatStr(floatBuf, (float)remainingKmToService / SERVICE_INTERVAL, 2, false),

        lightsStr,
        data[5] & 0x08 ? PSTR("INDICATOR_RIGHT ") : emptyStr,
        data[5] & 0x04 ? PSTR("INDICATOR_LEFT ") : emptyStr
    );

    if (data[5] & 0x02)
    {
        at += at >= n ? 0 :
            snprintf_P(buf + at, n - at,
                PSTR(",\n\"auto_gearbox\": \"%s%s%s%s\""),
                (data[4] & 0x70) == 0x00 ? PSTR("P") :
                (data[4] & 0x70) == 0x10 ? PSTR("R") :
                (data[4] & 0x70) == 0x20 ? PSTR("N") :
                (data[4] & 0x70) == 0x30 ? PSTR("D") :
                (data[4] & 0x70) == 0x40 ? PSTR("4") :
                (data[4] & 0x70) == 0x50 ? PSTR("3") :
                (data[4] & 0x70) == 0x60 ? PSTR("2") :
                (data[4] & 0x70) == 0x70 ? PSTR("1") :
                ToHexStr((uint8_t)(data[4] & 0x70)),

                data[4] & 0x08 ? PSTR(" - Snow") : emptyStr,
                data[4] & 0x04 ? PSTR(" - Sport") : emptyStr,
                data[4] & 0x80 ? PSTR(" (blinking)") : emptyStr
            );
    } // if

    if (data[6] != 0xFF)
    {
        at += at >= n ? 0 :
            snprintf_P(buf + at, n - at, PSTR(",\n\"oil_temp\": \"%d\""), (int)data[6] - 40);  // Never seen this
    } // if

    if (data[7] <= 120)
    {
        // Never seen this on my 406 HDI, only reported on 206 / petrol ?

        fuelLevelPercentage = data[7] <= 100 ? data[7] : 100;

        char floatBuf[MAX_FLOAT_SIZE];
        at += at >= n ? 0 :
            snprintf_P(buf + at, n - at, fuelLevelPercentageFormatter,
                fuelLevelPercentage,
                ToFloatStr(floatBuf, fuelLevelPercentage / 100.0, 2, false)
            );
    } // if

    at += at >= n ? 0 :
        snprintf_P(buf + at, n - at, PSTR(",\n\"oil_level_raw\": \"%u\""), data[8]);

    at += at >= n ? 0 :
        snprintf_P(buf + at, n - at, PSTR(
                ",\n"
                "\"oil_level_raw_perc\":\n"
                "{\n"
                    "\"style\":\n"
                    "{\n"
                        "\"transform\": \"scaleX(%s)\"\n"
                    "}\n"
                "}"
            ),

            #define MAX_OIL_LEVEL (85)
            data[8] >= MAX_OIL_LEVEL ? PSTR("1") :
                ToFloatStr(floatBuf, (float)data[8] / MAX_OIL_LEVEL, 2, false)
        );

    at += at >= n ? 0 :
        snprintf_P(buf + at, n - at, PSTR(",\n\"oil_level_dash\": \"%s\""),
            data[8] <= 11 ? PSTR("------") :
            data[8] <= 25 ? PSTR("O-----") :
            data[8] <= 39 ? PSTR("OO----") :
            data[8] <= 53 ? PSTR("OOO---") :
            data[8] <= 67 ? PSTR("OOOO--") :
            data[8] <= 81 ? PSTR("OOOOO-") :
            PSTR("OOOOOO")
        );

    if (data[10] != 0xFF)
    {
        // Never seen this; I don't have LPG
        at += at >= n ? 0 :
            snprintf_P(buf + at, n - at, PSTR(",\n\"lpg_fuel_level\": \"%s\""),
                data[10] <= 8 ? PSTR("1") :
                data[10] <= 17 ? PSTR("2") :
                data[10] <= 33 ? PSTR("3") :
                data[10] <= 50 ? PSTR("4") :
                data[10] <= 67 ? PSTR("5") :
                data[10] <= 83 ? PSTR("6") :
                PSTR("7")
            );
    } // if

    if (dataLen == 14)
    {
        // Vehicles made in/after 2004?

        // http://pinterpeti.hu/psavanbus/PSA-VAN.html#4FC_2

        at += at >= n ? 0 :
            snprintf_P(buf + at, n - at, PSTR(",\n\"cruise_control\": \"%s\""),
                data[11] == 0x41 ? offStr :
                data[11] == 0x49 ? PSTR("Cruise") :
                data[11] == 0x59 ? PSTR("Cruise - speed") :
                data[11] == 0x81 ? PSTR("Limiter") :
                data[11] == 0x89 ? PSTR("Limiter - speed") :
                ToHexStr(data[11])
            );

        at += at >= n ? 0 :
            snprintf_P(buf + at, n - at, PSTR(",\n\"cruise_control_speed\": \"%u\""), data[12]);
    } // if

    at += at >= n ? 0 : snprintf_P(buf + at, n - at, PSTR("\n}\n}\n"));

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseLightsStatusPkt

VanPacketParseResult_t ParseDeviceReportPkt(TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#8C4
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#8C4

    int dataLen = pkt.DataLen();
    if (dataLen < 1 || dataLen > 3) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

    const uint8_t* data = pkt.Data();
    int at = 0;

    if (data[0] == 0x8A)
    {
        if (dataLen != 3) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

        const static char jsonFormatter[] PROGMEM =
        "{\n"
            "\"event\": \"display\",\n"
            "\"data\":\n"
            "{\n"
                "\"head_unit_report\": \"%s\"";

        at = snprintf_P(buf, n, jsonFormatter,

            data[1] == 0x20 ? PSTR("TUNER_REPLY") :
            data[1] == 0x21 ? PSTR("AUDIO_SETTINGS_ANNOUNCE") :
            data[1] == 0x22 ? PSTR("BUTTON_PRESS_ANNOUNCE") :
            data[1] == 0x24 ? PSTR("TUNER_ANNOUNCEMENT") :
            data[1] == 0x28 ? PSTR("TAPE_PRESENCE_ANNOUNCEMENT") :
            data[1] == 0x30 ? PSTR("CD_PRESENT") :
            data[1] == 0x40 ? PSTR("TUNER_PRESETS_REPLY") :
            data[1] == 0x42 ? PSTR("TUNER_PRESET_MEMORIZE_BUTTON_PRESS_ANNOUNCE") :
            data[1] == 0x60 ? PSTR("TAPE_INFO_REPLY") :
            data[1] == 0x61 ? PSTR("TAPE_PLAYING_AUDIO_SETTINGS_ANNOUNCE") :
            data[1] == 0x62 ? PSTR("TAPE_PLAYING_BUTTON_PRESS_ANNOUNCE") :
            data[1] == 0x64 ? PSTR("TAPE_PLAYING_STARTING") :
            data[1] == 0x68 ? PSTR("TAPE_PLAYING_INFO") :
            data[1] == 0xC0 ? PSTR("INTERNAL_CD_TRACK_INFO_REPLY") :
            data[1] == 0xC1 ? PSTR("INTERNAL_CD_PLAYING_AUDIO_SETTINGS_ANNOUNCE") :
            data[1] == 0xC2 ? PSTR("INTERNAL_CD_PLAYING_BUTTON_PRESS_ANNOUNCE") :
            data[1] == 0xC4 ? PSTR("INTERNAL_CD_PLAYING_SEARCHING") :
            data[1] == 0xD0 ? PSTR("INTERNAL_CD_PLAYING_TRACK_INFO") :
            ToHexStr(data[1])
        );

        // Button-press announcement?
        if ((data[1] & 0x0F) == 0x02)
        {
            at += at >= n ? 0 :
                snprintf_P(buf + at, n - at,
                    PSTR(
                        ",\n"
                        "\"head_unit_button_pressed\": \"%s%s\""
                    ),

                    (data[2] & 0x1F) == 0x01 ? PSTR("1") :
                    (data[2] & 0x1F) == 0x02 ? PSTR("2") :
                    (data[2] & 0x1F) == 0x03 ? PSTR("3") :
                    (data[2] & 0x1F) == 0x04 ? PSTR("4") :
                    (data[2] & 0x1F) == 0x05 ? PSTR("5") :
                    (data[2] & 0x1F) == 0x06 ? PSTR("6") :
                    (data[2] & 0x1F) == 0x11 ? PSTR("AUDIO_DOWN") :
                    (data[2] & 0x1F) == 0x12 ? PSTR("AUDIO_UP") :
                    (data[2] & 0x1F) == 0x13 ? PSTR("SEEK_BACKWARD") :
                    (data[2] & 0x1F) == 0x14 ? PSTR("SEEK_FORWARD") :
                    (data[2] & 0x1F) == 0x16 ? PSTR("AUDIO") :
                    (data[2] & 0x1F) == 0x17 ? PSTR("MAN") :  // Not seen
                    (data[2] & 0x1F) == 0x1B ? PSTR("TUNER") :
                    (data[2] & 0x1F) == 0x1C ? PSTR("TAPE") :
                    (data[2] & 0x1F) == 0x1D ? PSTR("CD") :
                    (data[2] & 0x1F) == 0x1E ? PSTR("CD_CHANGER") :
                    ToHexStr(data[2]),

                    (data[2] & 0xC0) == 0xC0 ? PSTR(" (held)") :
                    data[2] & 0x40 ? PSTR(" (released)") :
                    data[2] & 0x80 ? PSTR(" (repeat)") :
                    emptyStr
                );
        } // if

        if (largeScreen == LARGE_SCREEN_GUIDANCE || largeScreen == LARGE_SCREEN_TRIP_COMPUTER)
        {
            // The following keys result in head unit popup during sat nav guidance or trip computer screen:
            if (
                (data[2] & 0x1F) == 0x13 ||  // SEEK_BACKWARD
                (data[2] & 0x1F) == 0x14 ||  // SEEK_FORWARD
                (data[2] & 0x1F) == 0x17 ||  // MAN
                (data[2] & 0x1F) == 0x1B ||  // TUNER
                (data[2] & 0x1F) == 0x1C ||  // TAPE
                (data[2] & 0x1F) == 0x1D ||  // CD
                (data[2] & 0x1F) == 0x1E     // CD_CHANGER
               )
            {
                NotificationPopupShowing(millis(), 8000);

                at += at >= n ? 0 :
                    snprintf_P(buf + at, n - at,
                        PSTR(
                            ",\n"
                            "\"mfd_popup\": \"%s\""
                        ),
                        PopupStr()
                    );
            } // if
        } // if

        at += at >= n ? 0 : snprintf_P(buf + at, n - at, PSTR("\n}\n}\n"));
    }
    else if (data[0] == 0x96)
    {
        if (dataLen != 1) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

        const static char jsonFormatter[] PROGMEM =
        "{\n"
            "\"event\": \"display\",\n"
            "\"data\":\n"
            "{\n"
                "\"cd_changer_announce\": \"STATUS_UPDATE_ANNOUNCE\"\n"
            "}\n"
        "}\n";

        at = snprintf_P(buf, n, jsonFormatter);
    }
    else if (data[0] == 0x07)
    {
        if (dataLen != 3) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

        // MFD request to sat nav (can e.g. be triggered by the user selecting a button on a sat nav selection
        // screen by using the remote control).
        //
        // Found combinations:
        //
        // 07-00-01
        // 07-00-02 - MFD requests "satnav_status_3"
        // 07-00-03 - MFD requests "satnav_status_3" and "satnav_status_2" ?
        // 07-01-00 - MFD requests "satnav_status_1" ?
        // 07-01-01 - User selected street from list. MFD requests "satnav_status_1" ?
        // 07-01-03
        // 07-06-00 - MFD requests "satnav_guidance_data" and "satnav_guidance"
        // 07-10-00 - User pressed "Val" on remote control
        // 07-11-00 - User pressed "Val" on remote control
        // 07-20-00 - MFD requests next "satnav_report" packet
        // 07-21-00 - MFD requests "satnav_status_1" and next "satnav_report" packet ?
        // 07-21-01 - User selected city from list. MFD requests "satnav_status_1" and next "satnav_report" packet ?
        // 07-40-00 - MFD requests "satnav_status_2" ?
        // 07-40-02 - MFD requests "satnav_status_2"
        // 07-41-00 - MFD requests "satnav_status_1" and "satnav_status_2" ?
        // 07-44-00 - MFD requests "satnav_status_2" and "satnav_guidance_data"
        // 07-47-00 - MFD requests "satnav_status_1", "satnav_status_2", "satnav_guidance_data" and "satnav_guidance"
        // 07-60-00
        //
        // So it looks like data[1] and data[2] are in fact bitfields:
        //
        // data[1]
        // & 0x01: Requesting "satnav_status_1" (IDEN 0x54E)
        // & 0x02: Requesting "satnav_guidance" (IDEN 0x64E)
        // & 0x04: Requesting "satnav_guidance_data" (IDEN 0x9CE)
        // & 0x10: Requesting "satnav_to_mfd_response" (IDEN 0x74E) (happens when user presses "Val" on remote control)
        // & 0x20: Requesting next "satnav_report" (IDEN 0x6CE) in sequence
        // & 0x40: Requesting "satnav_status_2" (IDEN 0x7CE)
        //
        // data[2]
        // & 0x01: User selecting
        // & 0x02: Requesting "satnav_status_3" (IDEN 0x8CE) ?

        const static char jsonFormatter[] PROGMEM =
        "{\n"
            "\"event\": \"display\",\n"
            "\"data\":\n"
            "{\n"
                "\"mfd_to_satnav_instruction\": \"%s\",\n"
                "\"mfd_to_satnav_status_1_request\": \"%s\",\n"
                "\"mfd_to_satnav_status_2_request\": \"%s\",\n"
                "\"mfd_to_satnav_status_3_request\": \"%s\",\n"
                "\"mfd_to_satnav_guidance_request\": \"%s\",\n"
                "\"mfd_to_satnav_guidance_data_request\": \"%s\",\n"
                "\"mfd_to_satnav_report_request\": \"%s\",\n"
                "\"mfd_to_satnav_response_request\": \"%s\",\n"
                "\"mfd_to_satnav_user_selection\": \"%s\"";

        uint16_t code = (uint16_t)data[1] << 8 | data[2];

        at = snprintf_P(buf, n, jsonFormatter,

            // User clicks on "Accept" button (usually bottom left of dialog screen)
            code == 0x0001 ? PSTR("Accept") :

            code == 0x0002 ? PSTR("Stop_navigation") :  // User stops navigation via menu on MFD
            code == 0x0003 ? PSTR("Change_destination") :  // User changes navigation destination via menu on MFD

            // Always follows 0x1000
            code == 0x0100 ? PSTR("End_of_button_press") :

            // User selects street from list
            // TODO - also when user selects service from list of services
            code == 0x0101 ? PSTR("Selected_street_from_list") :

            code == 0x0200 ? PSTR("Guidance_request") :
            code == 0x0600 ? PSTR("Guidance_data_request") :

            // User selects a menu entry or letter? User pressed "Val" (middle button on IR remote control).
            // Always followed by 0x0100.
            code == 0x1000 || code == 0x1100 ? PSTR("Val") :

            // MFD requests sat nav for next report packet (IDEN 0x6CE) in sequence
            // (Or: select from list using "Val"?)
            code == 0x2000 ? PSTR("Request_next_sat_nav_report_packet") :

            code == 0x2100 ? ToHexStr(code) :  // ??

            // User selects city from list
            code == 0x2101 ? PSTR("Selected_city_from_list") :

            code == 0x4000 ? PSTR("Request_status_2") :

            code == 0x4100 ? PSTR("Request_status_1_and_2") :

            // MFD asks for sat nav guidance data packet (IDEN 0x9CE) and satnav_status_2 (IDEN 0x7CE)
            code == 0x4400 ? PSTR("Request_sat_nav_guidance_data") :

            code == 0x4700 ? PSTR("Route_computed") :
            code == 0x6000 ? ToHexStr(code) :  // ??

            ToHexStr(code),

            data[1] & 0x01 ? yesStr : noStr,
            data[1] & 0x40 ? yesStr : noStr,
            data[2] & 0x02 ? yesStr : noStr,
            data[1] & 0x02 ? yesStr : noStr,
            data[1] & 0x04 ? yesStr : noStr,
            data[1] & 0x20 ? yesStr : noStr,
            data[1] & 0x10 ? yesStr : noStr,
            data[2] & 0x01 ? yesStr : noStr
        );

        if (code == 0x0002)
        {
            // In this specific situation, the small screen the original MFD is reverted to its value
            // before the guidance started
            UpdateSmallScreenAfterStoppingGuidance();

            at += at >= n ? 0 :
                snprintf_P(buf + at, n - at, PSTR(",\n\"small_screen\": \"%s\""),
                    SmallScreenStr()
                );
        } // if

        at += at >= n ? 0 : snprintf_P(buf + at, n - at, PSTR("\n}\n}\n"));
    }
    else if (data[0] == 0x52)
    {
        if (dataLen != 2) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

        // Unknown what this is
        //
        // Found combinations:
        //
        // 52-08
        // 52-20
        //
        return VAN_PACKET_PARSE_TO_BE_DECODED;
    }
    else
    {
        return VAN_PACKET_PARSE_TO_BE_DECODED;
    } // if

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseDeviceReportPkt

uint16_t vehicleSpeed_x100 = 0xFFFF;  // in units of 0.01 km/h, i.e. 9000 = 90 km/h
inline bool IsVehicleSpeedValid()
{
    return vehicleSpeed_x100 != 0xFFFF;
} // IsVehicleSpeedValid

uint16_t engineRpm_x8 = 0xFFFF;  // in units of 0.125 rpm, i.e. 24000 = 3000 rpm
inline bool IsEngineRpmValid()
{
    return engineRpm_x8 != 0xFFFF;
} // IsEngineRpmValid

bool isSatnavGuidanceActive = false;
bool isCurrentStreetKnown = false;

// Set to true to disable (once) duplicate detection. For use when switching to other units.
bool SkipCarStatus1PktDupDetect = false;

VanPacketParseResult_t ParseCarStatus1Pkt(TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#564
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#564

    const uint8_t* data = pkt.Data();
    int dataLen = pkt.DataLen();

    // Only continue parsing if actual content differs, not just the sequence number
    static uint8_t prevData[VAN_MAX_DATA_BYTES];  // Previous packet data
    if (memcmp(data + 1, prevData, dataLen - 2) == 0)
    {
        if (! SkipCarStatus1PktDupDetect) return VAN_PACKET_NO_CONTENT;
    } // if
    memcpy(prevData, data + 1, dataLen - 2);
    SkipCarStatus1PktDupDetect = false;

    InitSmallScreen();

    // Keep track when the stalk was pressed, so that we can distinguish between short press and long press
    static unsigned long stalkLastPressed;
    static bool stalkWasPressed = false;
    bool stalkIsPressed = data[10] & 0x01;

    // Note: stalk button will be processed even if a notification popup is currently showing
    // (IsNotificationPopupShowing())
    while (! economyMode && ! inMenu)
    {
        // Try to follow the original MFD in what it is currently showing, so that long-press (trip counter reset)
        // happens on the correct trip counter

        unsigned long now = pkt.Millis();  // Retrieve packet reception time stamp from ISR

        // Just pressed?
        if (! stalkWasPressed && stalkIsPressed)
        {
            stalkLastPressed = now;
            break;
        } // if

        // Only continue if just released
        if (! stalkWasPressed || stalkIsPressed) break;

        // Only continue if it was a short-press
        // Note: short-press = cycle screen; long-press = reset trip counter currently shown (if any)
        if (now - stalkLastPressed >= 1000) break;

        // May update the values as reported by 'TripComputerStr()', 'SmallScreenStr()' and 'PopupStr()' below
        CycleTripInfo();

        break;
    } // while

    stalkWasPressed = stalkIsPressed;

    uint16_t avgSpeedTrip1 = data[11];
    uint16_t avgSpeedTrip2 = data[12];
    uint16_t distanceTrip1 = (uint16_t)data[14] << 8 | data[15];
    uint16_t avgConsumptionLt100Trip1_x10 = (uint16_t)data[16] << 8 | data[17];
    uint16_t distanceTrip2 = (uint16_t)data[18] << 8 | data[19];
    uint16_t avgConsumptionLt100Trip2_x10 = (uint16_t)data[20] << 8 | data[21];
    uint16_t instConsumptionLt100_x10 = (uint16_t)data[22] << 8 | data[23];
    uint16_t distanceToEmpty = (uint16_t)data[24] << 8 | data[25];

    bool instConsumptionValid = instConsumptionLt100_x10 != 0xFFFF;

    float deliveredPower = -1.0;
    if (instConsumptionValid && IsVehicleSpeedValid())
    {
        // Rough calculation of power (HP) from fuel consumption data
        // (No idea if this is of any use or value...)
        //
        // Base data:
        //
        // - Energy per litre (acc. https://en.wikipedia.org/wiki/Fuel_efficiency ):
        //   * Diesel: 38.6 MegaJoule = 38600 KiloJoule
        //   * Petrol (gasoline: 34.8 MegaJoule = 34800 KiloJoule
        //
        // - 1 HP (horsepower) = 745.70 Joules / second (Watt) = 0.7457 KiloWatt
        //
        // - Fuel to wheel efficiency (acc. https://en.wikipedia.org/wiki/Fuel_efficiency ):
        //   * Diesel: 30%
        //   * Petrol: 20%
        //   ==> Of course, these "efficiency" percentages are highly debatable: could be more, could be less,
        //       depending on circumstances such as engine, air and fuel temperature, etc. Maybe the turbocharger
        //       in the HDI and XUD engines results in a higher efficiency? Let's just stick to these values for
        //       now; we can improve later.
        //
        if (fuelType == FUEL_DIESEL)
        {
            deliveredPower =
                (instConsumptionLt100_x10 / 10.0) * 38600 // KiloJoules per 100 km (burnt)
                * (vehicleSpeed_x100 / 100.0) / 100.0 // KiloJoules per hour
                / 3600.0  // KiloJoules per second == KiloWatt
                / 0.7457  // HPs burnt
                * 0.3;  // HPs delivered
        }
        else // fuelType == FUEL_PETROL
        {
            deliveredPower =
                (instConsumptionLt100_x10 / 10.0) * 34800  // KiloJoules per 100 km (burnt)
                * (vehicleSpeed_x100 / 100.0) / 100.0  // KiloJoules per hour
                / 3600.0  // KiloJoules per second == KiloWatt
                / 0.7457  // HPs burnt
                * 0.2;  // HPs delivered
        } // if
    } // if

    float deliveredTorque = -1.0;
    if (deliveredPower >= 0.0 && IsEngineRpmValid())
    {
        // Torque (N.m) = 9548.8 x Power (kW) / Speed (RPM) = 7120.54 x Power (HP) / Speed (RPM)
        deliveredTorque = 7120.54 * deliveredPower / (engineRpm_x8 / 8.0);
    } // if

    const static char jsonFormatter1[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"right_stalk_button\": \"%s\",\n"
            "\"small_screen\": \"%s\",\n"
            "\"trip_computer_screen_tab\": \"%s\",\n"
            "\"mfd_popup\": \"%s\"";

    int at = snprintf_P(buf, n, jsonFormatter1,
        stalkIsPressed ? PSTR("PRESSED") : PSTR("RELEASED"),
        SmallScreenStr(),
        TripComputerStr(),
        PopupStr()
    );

    if (IsTripComputerPopupShowing())
    {
        at += at >= n ? 0 :
            snprintf_P(buf + at, n - at,
                PSTR(
                    ",\n"
                    "\"trip_computer_popup_tab\": \"%s\""
                ),
                TripComputerStr()
            );
    } // if

    const static char jsonFormatter2[] PROGMEM =
            ",\n"
            "\"door_front_right\": \"%s\",\n"
            "\"door_front_left\": \"%s\",\n"
            "\"door_rear_right\": \"%s\",\n"
            "\"door_rear_left\": \"%s\",\n"
            "\"door_boot\": \"%s\",\n"
            "\"exp_moving_avg_speed\": \"%u\",\n"
            "\"delivered_power\": \"%s\",\n"
            "\"delivered_torque\": \"%s\",\n"
            "\"inst_consumption\": \"%s\",\n"
            "\"distance_to_empty\": \"%u\",\n"
            "\"avg_speed_1\": \"%u\",\n"
            "\"distance_1\": \"%s\",\n"
            "\"avg_consumption_1\": \"%s\"";

    char floatBuf[4][MAX_FLOAT_SIZE];
    at += at >= n ? 0 :
        snprintf_P(buf + at, n - at, jsonFormatter2,
            data[7] & 0x80 ? openStr : closedStr,
            data[7] & 0x40 ? openStr : closedStr,
            data[7] & 0x20 ? openStr : closedStr,
            data[7] & 0x10 ? openStr : closedStr,
            data[7] & 0x08 ? openStr : closedStr,

            // When engine running but stopped (actual vehicle speed is 0), this value counts down by 1 every
            // 10 - 20 seconds or so. When driving, this goes up and down slowly toward the current speed.
            // Looking at the time stamps when this value changes, this seems to be an exponential moving
            // average (EMA) of the recent vehicle speed. When the actual speed is 0, the value is seen to decrease
            // about 12% per minute. If the actual vehicle speed is sampled every second, then, in the
            // following formula, K would be around 12% / 60 = 0.2% = 0.002 :
            //
            //   exp_moving_avg_speed := exp_moving_avg_speed * (1 − K) + actual_vehicle_speed * K
            //
            // Often used in EMA is the constant N, where K = 2 / (N + 1). That means N would be around 1000 (given
            // a sampling time of 1 second).
            //
            mfdDistanceUnit == MFD_DISTANCE_UNIT_METRIC ? (uint16_t)data[13] : ToMiles((uint16_t)data[13]),

            deliveredPower >= 0.0 ? ToFloatStr(floatBuf[0], deliveredPower, 1) : notApplicableFloatStr,
            deliveredTorque >= 0.0 ? ToFloatStr(floatBuf[1], deliveredTorque, 1) : notApplicableFloatStr,

            mfdDistanceUnit == MFD_DISTANCE_UNIT_METRIC ?
                ! instConsumptionValid ? notApplicableFloatStr :
                    ToFloatStr(floatBuf[2], (float)instConsumptionLt100_x10 / 10.0, 1) :
                ! instConsumptionValid ? notApplicable2Str :
                    instConsumptionLt100_x10 <= 2 ? PSTR("&infin;") :
                    ToFloatStr(floatBuf[2], ToMilesPerGallon(instConsumptionLt100_x10), 0),

            mfdDistanceUnit == MFD_DISTANCE_UNIT_METRIC ?
                distanceToEmpty :
                ToMiles(distanceToEmpty),

            mfdDistanceUnit == MFD_DISTANCE_UNIT_METRIC ? avgSpeedTrip1 : ToMiles(avgSpeedTrip1),

            distanceTrip1 == 0xFFFF ? notApplicable2Str :
                mfdDistanceUnit == MFD_DISTANCE_UNIT_METRIC ?
                    ToStr(distanceTrip1) :
                    ToStr(ToMiles(distanceTrip1)),

            mfdDistanceUnit == MFD_DISTANCE_UNIT_METRIC ?
                avgConsumptionLt100Trip1_x10 == 0xFFFF ? notApplicableFloatStr :
                    ToFloatStr(floatBuf[3], (float)avgConsumptionLt100Trip1_x10 / 10.0, 1) :
                avgConsumptionLt100Trip1_x10 == 0xFFFF ? notApplicable2Str :
                    avgConsumptionLt100Trip1_x10 <= 1 ? PSTR("&infin;") :
                    ToFloatStr(floatBuf[3], ToMilesPerGallon(avgConsumptionLt100Trip1_x10), 0)
        );

    const static char jsonFormatter3[] PROGMEM =
            ",\n"
            "\"avg_speed_2\": \"%u\",\n"
            "\"distance_2\": \"%s\",\n"
            "\"avg_consumption_2\": \"%s\"\n"
        "}\n"
    "}\n";

    at += at >= n ? 0 :
        snprintf_P(buf + at, n - at, jsonFormatter3,

            mfdDistanceUnit == MFD_DISTANCE_UNIT_METRIC ? avgSpeedTrip2 : ToMiles(avgSpeedTrip2),

            distanceTrip2 == 0xFFFF ? notApplicable2Str :
                mfdDistanceUnit == MFD_DISTANCE_UNIT_METRIC ?
                    ToStr(distanceTrip2) :
                    ToStr(ToMiles(distanceTrip2)),

            mfdDistanceUnit == MFD_DISTANCE_UNIT_METRIC ?
                avgConsumptionLt100Trip2_x10 == 0xFFFF ? notApplicableFloatStr :
                    ToFloatStr(floatBuf[0], (float)avgConsumptionLt100Trip2_x10 / 10.0, 1) :
                avgConsumptionLt100Trip2_x10 == 0xFFFF ? notApplicable2Str :
                    avgConsumptionLt100Trip2_x10 <= 1 ? PSTR("&infin;") :
                    ToFloatStr(floatBuf[0], ToMilesPerGallon(avgConsumptionLt100Trip2_x10), 0)
        );

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseCarStatus1Pkt

VanPacketParseResult_t ParseCarStatus2Pkt(TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#524
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#524
    // https://github.com/morcibacsi/PSAVanCanBridge/blob/master/src/Van/Structs/VanDisplayStructsV1.h
    // https://github.com/morcibacsi/PSAVanCanBridge/blob/master/src/Van/Structs/VanDisplayStructsV2.h

    int dataLen = pkt.DataLen();
    if (dataLen != 14 && dataLen != 16) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"alarm_list\":\n"
            "[";

    int at = snprintf_P(buf, n, jsonFormatter);

    const uint8_t* data = pkt.Data();

    // Select the correct table, based on the chosen MFD language
    const char* const* msgTable =
        mfdLanguage == MFD_LANGUAGE_FRENCH ? msgTable_fr :
        mfdLanguage == MFD_LANGUAGE_GERMAN ? msgTable_ger :
        mfdLanguage == MFD_LANGUAGE_SPANISH ? msgTable_spa :
        mfdLanguage == MFD_LANGUAGE_ITALIAN ? msgTable_ita :
        mfdLanguage == MFD_LANGUAGE_DUTCH ? msgTable_dut :
        msgTable_eng;

    bool first = true;
    for (int byte = 0; byte < dataLen; byte++)
    {
        // Skip byte 9; it is the index of the current message
        if (byte == 9) byte++;

        for (int bit = 0; bit < 8; bit++)
        {
            if (data[byte] >> bit & 0x01)
            {
                char alarmText[80];  // Make sure this is large enough for the largest string it must hold; see above
                strncpy_P(alarmText, (char *)pgm_read_dword(&(msgTable[byte * 8 + bit])), sizeof(alarmText) - 1);
                alarmText[sizeof(alarmText) - 1] = 0;

                // TODO - with lots of alarms set, this packet could become very large and overflow the JSON buffer
                at += at >= n ? 0 :
                    snprintf_P(buf + at, n - at, PSTR("%s\n\"%s\""), first ? emptyStr : commaStr, alarmText);
                first = false;
            } // if
        } // for
    } // for

    at += at >= n ? 0 : snprintf_P(buf + at, n - at, PSTR("\n]"));

    uint8_t currentMsg = data[9];

    // The message to be shown in the popup on the MFD
    at += at >= n ? 0 :
        snprintf_P(buf + at, n - at,
            PSTR(",\n\"notification_message_on_mfd\": \"%s\""),

            // Relying on short-circuit boolean evaluation
            currentMsg <= 0x7F && strlen_P(msgTable[currentMsg]) > 0 ? msgTable[currentMsg] : emptyStr
        );

    at += at >= n ? 0 :
        snprintf_P(buf + at, n - at,
            PSTR(",\n\"notification_icon_on_mfd\": \"%s\""),

            // Relying on short-circuit boolean evaluation
            currentMsg <= 0x7F
            && notificationIconTable[currentMsg] != nullptr
            && strlen_P(notificationIconTable[currentMsg]) > 0
                ? notificationIconTable[currentMsg]
                : emptyStr
        );

    // Separately report "doors locked" status
    bool doorsLocked = data[8] & 0x01;
    at += at >= n ? 0 :
        snprintf_P(buf + at, n - at, PSTR(",\n\"doors_locked\": \"%s\""), doorsLocked ? yesStr : noStr);

    at += at >= n ? 0 : snprintf_P(buf + at, n - at, PSTR("\n}\n}\n"));

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseCarStatus2Pkt

VanPacketParseResult_t ParseDashboardPkt(TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#824
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#824

    const uint8_t* data = pkt.Data();

    engineRpm_x8 = (uint16_t)data[0] << 8 | data[1];
    vehicleSpeed_x100 = (uint16_t)data[2] << 8 | data[3];

    static long prevEngineRpm_x8 = 0;
    static long prevVehicleSpeed_x100 = 0;

    // With engine running, there are about 20 or so of these packets per second. Limit the rate somewhat.
    // Send only if any of the reported values changes with more than 20 rpm (engine_rpm), 1 km/h (vehicle_speed),
    // or after 1 second.

    static unsigned long lastUpdated = 0;
    unsigned long now = millis();

    long diffEngineRpm_x8 = engineRpm_x8 - prevEngineRpm_x8;
    long diffVehicleSpeed_x100 = vehicleSpeed_x100 - prevVehicleSpeed_x100;

    // Arithmetic has safe roll-over
    if (abs(diffEngineRpm_x8) < 20 * 8 && abs(diffVehicleSpeed_x100) < 100 * 1 && now - lastUpdated < 1000)
    {
        return VAN_PACKET_NO_CONTENT;
    } // if

    lastUpdated = now;

    prevEngineRpm_x8 = engineRpm_x8;
    prevVehicleSpeed_x100 = vehicleSpeed_x100;

    float vehicleSpeed = vehicleSpeed_x100 / 100.0;

    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"engine_rpm\": \"%s\",\n"
            "\"vehicle_speed\": \"%s\"\n"
        "}\n"
    "}\n";

    char floatBuf[2][MAX_FLOAT_SIZE];
    int at = snprintf_P(buf, n, jsonFormatter,
        ! IsEngineRpmValid() ?
            notApplicable3Str :
            ToFloatStr(floatBuf[0], engineRpm_x8 / 8.0, 0),
        ! IsVehicleSpeedValid() ?
            notApplicable2Str :
            mfdDistanceUnit == MFD_DISTANCE_UNIT_METRIC ?
                ToFloatStr(floatBuf[1], vehicleSpeed, 0) :
                ToFloatStr(floatBuf[1], ToMiles(vehicleSpeed), 0)
    );

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseDashboardPkt

VanPacketParseResult_t ParseDashboardButtonsPkt(TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#664
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#664

    int dataLen = pkt.DataLen();
    if (dataLen != 11 && dataLen != 12) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

    const uint8_t* data = pkt.Data();

    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"hazard_lights_button\": \"%s\",\n"  // Not sure
            "\"door_lock\": \"%s\",\n"
            "\"dashboard_programmed_brightness\": \"%u\",\n"
            "\"esp\": \"%s\"";

    // data[6..10] - always 00-FF-00-FF-00

    char floatBuf[3][MAX_FLOAT_SIZE];
    int at = snprintf_P(buf, n, jsonFormatter,
        data[0] & 0x02 ? onStr : offStr,
        data[2] & 0x40 ? onStr : offStr,
        data[2] & 0x0F,
        data[3] & 0x02 ? onStr : offStr);

    if (data[4] > 0 && data[4] <= 200)
    {
        // Surely fuel level. Test with tank full shows definitely level is in litres.
        float fuelLevelFiltered = data[4] / 2.0;  // Averaged over time

        const static char jsonFormatterFuel[] PROGMEM = ",\n"
            "\"fuel_level\": \"%s\",\n"
            "\"fuel_level_unit\": \"%s\",\n"
            "\"fuel_level_perc\":\n"
            "{\n"
                "\"style\":\n"
                "{\n"
                    "\"transform\": \"scaleX(%s)\"\n"
                "}\n"
            "}";

        at += at >= n ? 0 :
            snprintf_P(buf + at, n - at, jsonFormatterFuel,

                mfdDistanceUnit == MFD_DISTANCE_UNIT_METRIC ?
                    ToFloatStr(floatBuf[0], fuelLevelFiltered, 1) :
                    ToFloatStr(floatBuf[0], ToGallons(fuelLevelFiltered), 1),

                mfdDistanceUnit == MFD_DISTANCE_UNIT_METRIC ? PSTR("lt") : PSTR("gl"),

              #define FULL_TANK_LITRES (73.0)
                fuelLevelFiltered >= FULL_TANK_LITRES ? PSTR("1") :
                    ToFloatStr(floatBuf[1], fuelLevelFiltered / FULL_TANK_LITRES, 2, false)
            );
    } // if

    if (data[5] != 0xFF && data[5] != 0x00)
    {
        float fuelLevelRaw = data[5] / 2.0;  // Instantaneous value; can vary wildly

        at += at >= n ? 0 :
            snprintf_P(buf + at, n - at, ",\n\"fuel_level_raw\": \"%s\"",
                mfdDistanceUnit == MFD_DISTANCE_UNIT_METRIC ?
                    ToFloatStr(floatBuf[2], fuelLevelRaw, 1) :
                    ToFloatStr(floatBuf[2], ToGallons(fuelLevelRaw), 1)
            );
    } // if

    at += at >= n ? 0 : snprintf_P(buf + at, n - at, PSTR("\n}\n}\n"));

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseDashboardButtonsPkt

VanPacketParseResult_t ParseHeadUnitPkt(TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#554

    // These packets are sent by the head unit

    // Head unit info types
    enum HeadUnitInfoType_t
    {
        INFO_TYPE_TUNER = 0xD1,
        INFO_TYPE_TAPE,
        INFO_TYPE_PRESET,
        INFO_TYPE_CDCHANGER = 0xD5, // TODO - Not sure
        INFO_TYPE_CD,
    }; // enum HeadUnitInfoType_t

    const uint8_t* data = pkt.Data();
    uint8_t infoType = data[1];
    int dataLen = pkt.DataLen();
    int at = 0;

    switch (infoType)
    {
        case INFO_TYPE_TUNER:
        {
            // Message when the head unit is in "tuner" (radio) mode

            // http://pinterpeti.hu/psavanbus/PSA-VAN.html#554_1

            // Note: all packets as received from the following head units:
            // - Clarion RM2-00 - PU-1633A(E) - Cassette tape
            // - Clarion RD3-01 - PU-2473A(K) - CD player
            // Other head units may have different packets.

            if (dataLen != 22) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

            // data[2]: radio band and preset position
            uint8_t band = data[2] & 0x07;
            uint8_t presetMemory = data[2] >> 3 & 0x0F;
            char presetMemoryBuffer[3];
            sprintf_P(presetMemoryBuffer, PSTR("%1u"), presetMemory);

            // data[3]: search bits
            bool dxSensitivity = data[3] & 0x02;  // Tuner sensitivity: distant (Dx) or local (Lo)
            bool ptyStandbyMode = data[3] & 0x04;

            uint8_t searchMode = data[3] >> 3 & 0x07;
            bool anySearchBusy = searchMode != TS_NOT_SEARCHING;
            bool automaticSearchBusy = anySearchBusy && searchMode != TS_MANUAL;  // Any search except manual

            // data[4] and data[5]: frequency being scanned or tuned in to
            uint16_t frequency = (uint16_t)data[5] << 8 | data[4];
            static uint16_t prevFrequency = 0x07FF;  // To detect change during manual search
            if (prevFrequency == 0x07FF) prevFrequency = frequency;

            // Special processing of search direction bit

            enum TunerSearchDirection_t
            {
                TSD_NONE,
                TSD_UP,
                TSD_DOWN
            }; // enum TunerSearchDirection_t

            TunerSearchDirection_t searchDirection = data[3] & 0x80 ? TSD_UP : TSD_DOWN;
            static TunerSearchDirection_t prevSearchDirection = TSD_NONE;

            // In manual search mode, change the reported direction only if the frequency actually changed.
            if (searchMode == TS_MANUAL && frequency == prevFrequency)
            {
                // Search mode is "manual" and no change in frequency? Then report same value as previously.
                searchDirection = prevSearchDirection;
            } // if

            if (searchMode == TS_NOT_SEARCHING) prevSearchDirection = TSD_NONE;
            else prevSearchDirection = searchDirection;

            prevFrequency = frequency;

            // data[6] - Reception status? Like: receiving station? Stereo? RDS bits like MS, TP, TA, AF?
            //
            // & 0xF0:
            //   - Usually 0x00 when tuned in to a "normal" station.
            //   - One or more bits stay '1' when tuned in to a "crappy" station (e.g. pirate).
            //   - During the process of tuning in to another station, switches to e.g. 0x20, 0x60, but
            //     (usually) ends up 0x00.
            //   Just guessing for the possible meaning of the bits:
            //   - Mono (not stereo) bit
            //   - Music/Speech (MS) bit
            //   - No AF (Alternative Frequencies) available
            //   - Number (0..15) indicating the quality of the RDS stream
            //
            // & 0x0F = signal strength: increases with antenna plugged in and decreases with antenna plugged
            //          out. Updated when a station is being tuned in to, or when the MAN button is pressed.
            uint8_t signalStrength = data[6] & 0x0F;
            char signalStrengthBuffer[3];
            sprintf_P(signalStrengthBuffer, PSTR("%u"), signalStrength);

            const static char jsonFormatterCommon[] PROGMEM =
            "{\n"
                "\"event\": \"display\",\n"
                "\"data\":\n"
                "{\n"
                    "\"tuner_band\": \"%s\",\n"
                    "\"fm_band\": \"%s\",\n"
                    "\"fm_band_1\": \"%s\",\n"
                    "\"fm_band_2\": \"%s\",\n"
                    "\"fm_band_ast\": \"%s\",\n"
                    "\"am_band\": \"%s\",\n"
                    "\"tuner_memory\": \"%s\",\n"
                    "\"frequency\": \"%s\",\n"
                    "\"frequency_h\": \"%s\",\n"
                    "\"frequency_unit\": \"%s\",\n"
                    "\"frequency_khz\": \"%s\",\n"
                    "\"frequency_mhz\": \"%s\",\n"
                    "\"signal_strength\": \"%s\",\n"
                    "\"search_mode\": \"%s\",\n"
                    "\"search_manual\": \"%s\",\n"
                    "\"search_sensitivity\": \"%s\",\n"
                    "\"search_sensitivity_lo\": \"%s\",\n"
                    "\"search_sensitivity_dx\": \"%s\",\n"
                    "\"search_direction\": \"%s\",\n"
                    "\"search_direction_up\": \"%s\",\n"
                    "\"search_direction_down\": \"%s\"";

            char floatBuf[MAX_FLOAT_SIZE];
            at = snprintf_P(buf, n, jsonFormatterCommon,
                TunerBandStr(band),
                band == TB_FM1 || band == TB_FM2 || band == TB_FM3 || band == TB_FMAST || band == TB_PTY_SELECT ?
                    onStr : offStr,
                band == TB_FM1 ? onStr : offStr,
                band == TB_FM2 ? onStr : offStr,
                band == TB_FMAST ? onStr : offStr,
                band == TB_AM ? onStr : offStr,
                presetMemory == 0 ? notApplicable1Str : presetMemoryBuffer,

                frequency == 0x07FF ? notApplicable3Str :
                    band == TB_AM
                        ? ToFloatStr(floatBuf, frequency, 0)  // AM and LW bands

                        // Note: want decimal point here as 'DSEG7Classic' font cannot elegantly show decimal comma
                        : ToFloatStr(floatBuf, (frequency / 2 + 500) / 10.0, 1, false),  // FM bands

                frequency == 0x07FF ? notApplicable1Str :
                    band == TB_AM
                        ? emptyStr  // AM and LW bands
                        : frequency % 2 == 0 ? PSTR("0") : PSTR("5"),  // FM bands
                band == TB_AM ? PSTR("KHz") : PSTR("MHz"),
                band == TB_AM ? onStr : offStr,  // For retro-type "LED" display
                band == TB_AM ? offStr : onStr,  // For retro-type "LED" display

                // Also applicable in AM mode
                signalStrength == 15 && (searchMode == TS_BY_FREQUENCY || searchMode == TS_BY_MATCHING_PTY)
                    ? notApplicable2Str
                    : signalStrengthBuffer,

                TunerSearchModeStr(searchMode),
                searchMode == TS_MANUAL ? onStr : offStr,

                // Sensitivity of automatic search: distant (Dx) or local (Lo)
                ! automaticSearchBusy ? emptyStr : dxSensitivity ? PSTR("Dx") : PSTR("Lo"),
                ! automaticSearchBusy ? offStr : dxSensitivity ? offStr : onStr,
                ! automaticSearchBusy ? offStr : dxSensitivity ? onStr : offStr,

                ! anySearchBusy || searchDirection == TSD_NONE ?
                    emptyStr :
                    searchDirection == TSD_UP ? PSTR("UP") : PSTR("DOWN"),
                anySearchBusy && searchDirection == TSD_UP ? onStr : offStr,
                anySearchBusy && searchDirection == TSD_DOWN ? onStr : offStr
            );

            if (band != TB_AM)
            {
                // FM bands only

                // data[7]: TA, RDS and REG (regional) bits
                bool rdsSelected = data[7] & 0x01;
                bool taSelected = data[7] & 0x02;
                bool regional = data[7] & 0x04;  // Long-press "RDS" button
                bool rdsNotAvailable = (data[7] & 0x20) == 0;
                bool taNotAvailable = (data[7] & 0x40) == 0;
                bool taAnnounce = data[7] & 0x80;

                // data[8] and data[9]: PI code
                // See also:
                // - https://en.wikipedia.org/wiki/Radio_Data_System#Program_Identification_Code_(PI_Code),
                // - https://radio-tv-nederland.nl/rds/rds.html
                // - https://people.uta.fi/~jk54415/dx/pi-codes.html
                // - http://poupa.cz/rds/countrycodes.htm
                // - https://www.pira.cz/rds/p232man.pdf
                uint16_t piCode = (uint16_t)data[8] << 8 | data[9];
                uint8_t countryCode = piCode >> 12 & 0x0F;
                uint8_t coverageCode = piCode >> 8 & 0x0F;
                char piBuffer[5];
                sprintf_P(piBuffer, PSTR("%04X"), piCode);

                // data[10]: for PTY-based search mode
                // & 0x1F: PTY code to search
                // & 0x20: 0 = PTY of station matches selected; 1 = no match
                // & 0x40: 1 = "Select PTY" dialog visible (long-press "TA" button; press "<<" or ">>" to change)
                uint8_t selectedPty = data[10] & 0x1F;
                bool ptyMatch = (data[10] & 0x20) == 0;  // PTY of station matches selected PTY
                bool ptySelectionMenu = data[10] & 0x40;

                // data[11]: PTY code of current station
                uint8_t currPty = data[11] & 0x1F;

                // data[12]...data[20]: RDS text
                char rdsTxt[9];
                strncpy(rdsTxt, (const char*) data + 12, 8);
                rdsTxt[8] = 0;

                const static char jsonFormatterFmBand[] PROGMEM = ",\n"
                    "\"pty_selection_menu\": \"%s\",\n"
                    "\"selected_pty_8\": \"%s\",\n"
                    "\"selected_pty_16\": \"%s\",\n"
                    "\"selected_pty_full\": \"%s\",\n"
                    "\"pty_standby_mode\": \"%s\",\n"
                    "\"pty_match\": \"%s\",\n"
                    "\"pty_8\": \"%s\",\n"
                    "\"pty_16\": \"%s\",\n"
                    "\"pty_full\": \"%s\",\n"
                    "\"pi_code\": \"%s\",\n"
                    "\"pi_country\": \"%s\",\n"
                    "\"pi_area_coverage\": \"%s\",\n"
                    "\"regional\": \"%s\",\n"
                    "\"ta_selected\": \"%s\",\n"
                    "\"ta_not_available\": \"%s\",\n"
                    "\"rds_selected\": \"%s\",\n"
                    "\"rds_not_available\": \"%s\",\n"
                    "\"rds_text\": \"%s\",\n"
                    "\"info_traffic\": \"%s\"";

                at += at >= n ? 0 :
                    snprintf_P(buf + at, n - at, jsonFormatterFmBand,
                        ptySelectionMenu ? onStr : offStr,

                        selectedPty == 0x00 ? notApplicable3Str : PtyStr8(selectedPty),
                        selectedPty == 0x00 ? notApplicable3Str : PtyStr16(selectedPty),
                        selectedPty == 0x00 ? notApplicable3Str : PtyStrFull(selectedPty),

                        ptyStandbyMode ? yesStr : noStr,
                        ptyMatch ? yesStr : noStr,
                        currPty == 0x00 ? notApplicable3Str : PtyStr8(currPty),
                        currPty == 0x00 ? notApplicable3Str : PtyStr16(currPty),
                        currPty == 0x00 ? notApplicable3Str : PtyStrFull(currPty),

                        piCode == 0xFFFF ? notApplicable3Str : piBuffer,
                        piCode == 0xFFFF ? notApplicable2Str : RadioPiCountry(countryCode),
                        piCode == 0xFFFF ? notApplicable3Str : RadioPiAreaCoverage(coverageCode),

                        regional ? onStr : offStr,
                        taSelected ? yesStr : noStr,
                        taNotAvailable ? yesStr : noStr,
                        rdsSelected ? yesStr : noStr,
                        rdsNotAvailable ? yesStr : noStr,
                        rdsTxt,

                        taAnnounce ? yesStr : noStr
                    );
            } // if

            at += at >= n ? 0 : snprintf_P(buf + at, n - at, PSTR("\n}\n}\n"));
        }
        break;

        case INFO_TYPE_TAPE:
        {
            // http://pinterpeti.hu/psavanbus/PSA-VAN.html#554_2

            if (dataLen != 5) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

            uint8_t status = data[2] & 0x3C;

            const static char jsonFormatter[] PROGMEM =
            "{\n"
                "\"event\": \"display\",\n"
                "\"data\":\n"
                "{\n"
                    "\"tape_side\": \"%s\",\n"
                    "\"tape_status\": \"%s\",\n"
                    "\"tape_status_stopped\": \"%s\",\n"
                    "\"tape_status_loading\": \"%s\",\n"
                    "\"tape_status_play\": \"%s\",\n"
                    "\"tape_status_fast_forward\": \"%s\",\n"
                    "\"tape_status_next_track\": \"%s\",\n"
                    "\"tape_status_rewind\": \"%s\",\n"
                    "\"tape_status_previous_track\": \"%s\"\n"
                "}\n"
            "}\n";

            at = snprintf_P(buf, n, jsonFormatter,

                data[2] & 0x01 ? PSTR("2") : PSTR("1"),

                status == 0x00 ? PSTR("STOPPED") :
                status == 0x04 ? PSTR("LOADING") :
                status == 0x0C ? PSTR("PLAY") :
                status == 0x10 ? PSTR("FAST_FORWARD") :
                status == 0x14 ? PSTR("NEXT_TRACK") :
                status == 0x30 ? PSTR("REWIND") :
                status == 0x34 ? PSTR("PREVIOUS_TRACK") :
                ToHexStr(status),

                status == 0x00 ? onStr : offStr,
                status == 0x04 ? onStr : offStr,
                status == 0x0C ? onStr : offStr,
                status == 0x10 ? onStr : offStr,
                status == 0x14 ? onStr : offStr,
                status == 0x30 ? onStr : offStr,
                status == 0x34 ? onStr : offStr
            );
        }
        break;

        case INFO_TYPE_PRESET:
        {
            // http://pinterpeti.hu/psavanbus/PSA-VAN.html#554_3

            if (dataLen != 12) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

            uint8_t tunerBand = data[2] >> 4 & 0x07;
            uint8_t tunerMemory = data[2] & 0x0F;

            char rdsOrFreqTxt[9];
            strncpy(rdsOrFreqTxt, (const char*) data + 3, 8);
            rdsOrFreqTxt[8] = 0;

            const static char jsonFormatter[] PROGMEM =
            "{\n"
                "\"event\": \"display\",\n"
                "\"data\":\n"
                "{\n"
                    "\"radio_preset_%s_%u\": \"%s%s\"\n"
                "}\n"
            "}\n";

            at = snprintf_P(buf, n, jsonFormatter,
                TunerBandStr(tunerBand),
                tunerMemory,
                rdsOrFreqTxt,
                tunerBand == TB_AM ? PSTR(" KHz") : data[2] & 0x80 ? emptyStr : PSTR(" MHz")
            );
        }
        break;

        case INFO_TYPE_CD:
        {
            // http://pinterpeti.hu/psavanbus/PSA-VAN.html#554_6

            if (dataLen != 19) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

            char trackTimeStr[7];
            sprintf_P(trackTimeStr, PSTR("%X:%02X"), data[5], data[6]);

            static bool loading = false;
            bool searching = data[3] & 0x10;

            // Keep on reporting "loading" until no longer "searching"
            if ((data[3] & 0x0F) == 0x01) loading = true;
            if (! searching) loading = false;

            uint8_t totalTracks = data[8];
            bool totalTracksValid = totalTracks != 0xFF;
            char totalTracksStr[3];
            if (totalTracksValid) sprintf_P(totalTracksStr, PSTR("%X"), totalTracks);

            char totalTimeStr[7];
            uint8_t totalTimeMin = data[9];
            uint8_t totalTimeSec = data[10];
            bool totalTimeValid = totalTimeMin != 0xFF && totalTimeSec != 0xFF;
            if (totalTimeValid) sprintf_P(totalTimeStr, PSTR("%X:%02X"), totalTimeMin, totalTimeSec);

            const static char jsonFormatter[] PROGMEM =
            "{\n"
                "\"event\": \"display\",\n"
                "\"data\":\n"
                "{\n"
                    "\"cd_status\": \"%s\",\n"
                    "\"cd_status_loading\": \"%s\",\n"
                    "\"cd_status_eject\": \"%s\",\n"
                    "\"cd_status_pause\": \"%s\",\n"
                    "\"cd_status_play\": \"%s\",\n"
                    "\"cd_status_fast_forward\": \"%s\",\n"
                    "\"cd_status_rewind\": \"%s\",\n"
                    "\"cd_status_searching\": \"%s\",\n"
                    "\"cd_track_time\": \"%s\",\n"
                    "\"cd_current_track\": \"%X\",\n"
                    "\"cd_total_tracks\": \"%s\",\n"
                    "\"cd_total_time\": \"%s\",\n"
                    "\"cd_random\": \"%s\"\n"
                "}\n"
            "}\n";

            at = snprintf_P(buf, n, jsonFormatter,

                data[3] == 0x00 ? PSTR("EJECT") :
                data[3] == 0x10 ? PSTR("ERROR") :  // E.g. disc inserted upside down
                data[3] == 0x11 ? PSTR("LOADING") :
                data[3] == 0x12 ? PSTR("PAUSE-SEARCHING") :
                data[3] == 0x13 ? PSTR("PLAY-SEARCHING") :
                data[3] == 0x02 ? PSTR("PAUSE") :
                data[3] == 0x03 ? PSTR("PLAY") :
                data[3] == 0x04 ? PSTR("FAST_FORWARD") :
                data[3] == 0x05 ? PSTR("REWIND") :
                ToHexStr(data[3]),

                loading ? onStr : offStr,
                data[3] == 0x00 ? onStr : offStr,
                (data[3] & 0x0F) == 0x02 && ! searching ? onStr : offStr,
                (data[3] & 0x0F) == 0x03 && ! searching ? onStr : offStr,
                (data[3] & 0x0F) == 0x04 ? onStr : offStr,
                (data[3] & 0x0F) == 0x05 ? onStr : offStr,

                (data[3] == 0x12 || data[3] == 0x13) && ! loading ? onStr : offStr,

                searching ? PSTR("--:--") : trackTimeStr,

                data[7],
                totalTracksValid ? totalTracksStr : notApplicable2Str,
                totalTimeValid ? totalTimeStr : PSTR("--:--"),

                data[2] & 0x01 ? onStr : offStr  // CD track shuffle: long-press "CD" button
            );
        }
        break;

        case INFO_TYPE_CDCHANGER:
        {
            return VAN_PACKET_PARSE_TO_BE_DECODED;
        }
        break;

        default:
        {
            return VAN_PACKET_PARSE_TO_BE_DECODED;
        }
        break;
    } // switch

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseHeadUnitPkt

VanPacketParseResult_t ParseMfdLanguageUnitsPkt(TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#984
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#984

    const uint8_t* data = pkt.Data();

    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"mfd_language\": \"%s\",\n"
            "\"mfd_temperature_unit\": \"%s\",\n"
            "\"mfd_distance_unit\": \"%s\",\n"
            "\"mfd_time_unit\": \"%s\"\n"
        "}\n"
    "}\n";

    int at = snprintf_P(buf, n, jsonFormatter,
        data[3] == 0x00 ? PSTR("FRENCH") :
        data[3] == 0x01 ? PSTR("ENGLISH") :
        data[3] == 0x02 ? PSTR("GERMAN") :
        data[3] == 0x03 ? PSTR("SPANISH") :
        data[3] == 0x04 ? PSTR("ITALIAN") :
        data[3] == 0x06 ? PSTR("DUTCH") :
        notApplicable3Str,

        data[4] & 0x02 ? PSTR("FAHRENHEIT") : PSTR("CELSIUS"),
        data[4] & 0x04 ? PSTR("MILES_YARDS") : PSTR("KILOMETRES_METRES"),
        data[4] & 0x08 ? PSTR("24_H") : PSTR("12_H")
    );

    mfdLanguage =
        data[3] == 0x00 ? MFD_LANGUAGE_FRENCH :
        data[3] == 0x02 ? MFD_LANGUAGE_GERMAN :
        data[3] == 0x03 ? MFD_LANGUAGE_SPANISH :
        data[3] == 0x04 ? MFD_LANGUAGE_ITALIAN :
        data[3] == 0x06 ? MFD_LANGUAGE_DUTCH :
        MFD_LANGUAGE_ENGLISH;

    decimalSeparatorChar =
        mfdLanguage == MFD_LANGUAGE_FRENCH ? ',' :
        mfdLanguage == MFD_LANGUAGE_GERMAN ? ',' :
        mfdLanguage == MFD_LANGUAGE_SPANISH ? ',' :
        mfdLanguage == MFD_LANGUAGE_ITALIAN ? ',' :
        mfdLanguage == MFD_LANGUAGE_DUTCH ? ',' :
        '.';

    uint8_t prevMfdTemperatureUnit = mfdTemperatureUnit;
    mfdTemperatureUnit = data[4] & 0x02 ? MFD_TEMPERATURE_UNIT_FAHRENHEIT : MFD_TEMPERATURE_UNIT_CELSIUS;

    uint8_t prevMfdDistanceUnit = mfdDistanceUnit;
    mfdDistanceUnit = data[4] & 0x04 ? MFD_DISTANCE_UNIT_IMPERIAL : MFD_DISTANCE_UNIT_METRIC;

    mfdTimeUnit = data[4] & 0x08 ? MFD_TIME_UNIT_12H : MFD_TIME_UNIT_24H;

    if (mfdTemperatureUnit != prevMfdTemperatureUnit || mfdDistanceUnit != prevMfdDistanceUnit) ResetPacketPrevData();

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseMfdLanguageUnitsPkt

bool isHeadUnitPowerOn = false;
bool seenTapePresence = false;
bool seenCdPresence = false;

VanPacketParseResult_t ParseAudioSettingsPkt(TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#4D4
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#4D4
    // https://github.com/morcibacsi/PSAVanCanBridge/blob/master/src/Van/Structs/VanRadioInfoStructs.h

    const uint8_t* data = pkt.Data();
    isHeadUnitPowerOn = data[2] & 0x01;
    bool isTapePresent = data[4] & 0x20;
    bool isCdPresent = data[4] & 0x40;
    seenTapePresence = seenTapePresence || isTapePresent;
    seenCdPresence = seenCdPresence || isCdPresent;

    uint8_t volume = data[5] & 0x7F;
    int8_t balance = (int8_t)(0x3F) - (data[6] & 0x7F);
    int8_t fader = (int8_t)(0x3F) - (data[7] & 0x7F);

    static bool washeadUnitPowerOn = false;
    if (! washeadUnitPowerOn && isHeadUnitPowerOn)
    {
        // Turning on head unit
        UpdateLargeScreenForHeadUnitOn();
    }
    else if (washeadUnitPowerOn && ! isHeadUnitPowerOn)
    {
        // Turning off head unit
        UpdateLargeScreenForHeadUnitOff();
    } // if

    washeadUnitPowerOn = isHeadUnitPowerOn;

    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"head_unit_power\": \"%s\",\n"
            "\"tape_present\": \"%s\",\n"
            "\"cd_present\": \"%s\",\n"
            "\"audio_source\": \"%s\",\n"
            "\"ext_mute\": \"%s\",\n"
            "\"mute\": \"%s\",\n"
            "\"volume\": \"%u\",\n"
            "\"volume_update\": \"%s\",\n"
            "\"volume_perc\":\n"
            "{\n"
                "\"style\":\n"
                "{\n"
                    "\"transform\": \"scaleX(%s)\"\n"
                "}\n"
            "},\n"
            "\"audio_menu\": \"%s\",\n"
            "\"bass\": \"%+d\",\n"
            "\"bass_update\": \"%s\",\n"
            "\"treble\": \"%+d\",\n"
            "\"treble_update\": \"%s\",\n"
            "\"loudness\": \"%s\",\n"
            "\"fader\": \"%s%d\",\n"
            "\"fader_update\": \"%s\",\n"
            "\"balance\": \"%s%d\",\n"
            "\"balance_update\": \"%s\",\n"
            "\"auto_volume\": \"%s\",\n"
            "\"large_screen\": \"%s\",\n"
            "\"mfd_popup\": \"%s\"\n"
        "}\n"
    "}\n";

    char floatBuf[MAX_FLOAT_SIZE];

    int at = snprintf_P(buf, n, jsonFormatter,
        isHeadUnitPowerOn ? onStr : offStr,
        isTapePresent ? yesStr : noStr,
        isCdPresent ? yesStr : noStr,

        (data[4] & 0x0F) == 0x00 ? noneStr :  // Source of audio
        (data[4] & 0x0F) == 0x01 ? PSTR("TUNER") :
        (data[4] & 0x0F) == 0x02 ?
            seenTapePresence ? PSTR("TAPE") :
            seenCdPresence ? PSTR("CD") :
            PSTR("INTERNAL_CD_OR_TAPE") :
        (data[4] & 0x0F) == 0x03 ? PSTR("CD_CHANGER") :

        // This is the "default" mode for the head unit, to sit there and listen to the navigation
        // audio. The navigation audio volume is also always set (usually a lot higher than the radio)
        // whenever this source is chosen.
        (data[4] & 0x0F) == 0x05 ? PSTR("NAVIGATION") :

        ToHexStr((uint8_t)(data[4] & 0x0F)),

        // External mute. Activated when head unit ISO connector A pin 1 ("Phone mute") is pulled LOW (to Ground).
        data[1] & 0x02 ? onStr : offStr,

        // Mute. To activate: press both VOL_UP and VOL_DOWN buttons on stalk.
        data[1] & 0x01 ? onStr : offStr,

        volume,
        data[5] & 0x80 ? yesStr : noStr,

        // Factory head unit has fixed maximum volume value of 30
        #define MAX_AUDIO_VOLUME (30.0)
        ToFloatStr(floatBuf, (float)volume / MAX_AUDIO_VOLUME, 2),

        // Audio menu. Bug: if CD changer is playing, this one is always "OPEN" (even if it isn't).
        data[1] & 0x20 ? openStr : closedStr,

        (int8_t)(data[8] & 0x7F) - 0x3F,  // Bass
        data[8] & 0x80 ? yesStr : noStr,
        (int8_t)(data[9] & 0x7F) - 0x3F,  // Treble
        data[9] & 0x80 ? yesStr : noStr,
        data[1] & 0x10 ? onStr : offStr,  // Loudness
        fader == 0 ? "   " : fader > 0 ? "F +" : "R ", fader,
        data[7] & 0x80 ? yesStr : noStr,
        balance == 0 ? "   " : balance > 0 ? "R +" : "L ", balance,
        data[6] & 0x80 ? yesStr : noStr,
        data[1] & 0x04 ? onStr : offStr,  // Auto volume
        LargeScreenStr(),
        PopupStr()
    );

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseAudioSettingsPkt

// Saved equipment status (volatile)
int16_t satnavServiceListSize = -1;

VanPacketParseResult_t ParseMfdStatusPkt(TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#5E4
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#5E4

    const uint8_t* data = pkt.Data();
    uint16_t mfdStatus = (uint16_t)data[0] << 8 | data[1];
    static bool mfdWasOff = true;

    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"mfd_status\": \"%s\"";

    int at = snprintf_P(buf, n, jsonFormatter,

        // hmmm... MFD can also be ON if this is reported; this happens e.g. in the "minimal VAN network" test
        // setup with only the head unit (radio) and MFD. Maybe this is a status report: the MFD shows if has
        // received any packets that show connectivity to e.g. the BSI?
        mfdStatus == MFD_SCREEN_OFF ? PSTR("MFD_SCREEN_OFF") :

        mfdStatus == MFD_SCREEN_ON ? PSTR("MFD_SCREEN_ON") :
        mfdStatus == TRIP_COUTER_1_RESET ? PSTR("TRIP_COUTER_1_RESET") :
        mfdStatus == TRIP_COUTER_2_RESET ? PSTR("TRIP_COUTER_2_RESET") :
        ToHexStr(mfdStatus)
    );

    if (mfdStatus == TRIP_COUTER_1_RESET || mfdStatus == TRIP_COUTER_2_RESET)
    {
        ResetTripInfo(mfdStatus);

        at += at >= n ? 0 :
            snprintf_P(buf + at, n - at,
                PSTR(
                    ",\n"
                    "\"small_screen\": \"%s\",\n"
                    "\"trip_computer_screen_tab\": \"%s\""
                ),
                SmallScreenStr(),
                TripComputerStr()
            );

        if (IsTripComputerPopupShowing())
        {
            at += at >= n ? 0 :
                snprintf_P(buf + at, n - at,
                    PSTR(
                        ",\n"
                        "\"trip_computer_popup_tab\": \"%s\""
                    ),
                    TripComputerStr()
                );
        } // if
    } // if

    if (mfdWasOff && mfdStatus == MFD_SCREEN_ON)
    {
        mfdWasOff = false;

        // We do all this now, when mfdStatus goes from MFD_SCREEN_OFF to MFD_SCREEN_ON. It would be more logical
        // to do this when mfdStatus == MFD_SCREEN_OFF (below), but that would not work in the "minimal VAN network"
        // test setup with only the head unit (radio) and MFD.

        satnavServiceListSize = -1;

        satnavDisclaimerAccepted = false;
        at += at >= n ? 0 : snprintf(buf + at, n - at, PSTR(",\n\"satnav_disclaimer_accepted\": \"NO\""));

        UpdateLargeScreenForMfdOff();  // 'largeScreen' will become 'LARGE_SCREEN_CLOCK'
        at += at >= n ? 0 : snprintf_P(buf + at, n - at, PSTR(",\n\"large_screen\": \"%s\""), LargeScreenStr());
    } // if

    if (mfdStatus == MFD_SCREEN_OFF)
    {
        mfdWasOff = true;

        // The moment the MFD switches off seems to be the best time to check if the store must be saved; better than
        // when the MFD is active and VAN packets are being received. VAN bus and ESP8266 flash system (SPI based)
        // don't work well together.
        CommitEeprom();

        at += at >= n ? 0 :
            snprintf_P(buf + at, n - at,
                PSTR(
                    ",\n"
                    "\"small_screen\": \"%s\",\n"
                    "\"trip_computer_screen_tab\": \"%s\""
                ),
                SmallScreenStr(),
                TripComputerStr()
            );

        isSatnavGuidanceActive = false;
        isCurrentStreetKnown = false;
    } // if

    at += at >= n ? 0 : snprintf_P(buf + at, n - at, PSTR("\n}\n}\n"));

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseMfdStatusPkt

#define SET_FAN_SPEED_INVALID (0xFF)
uint8_t setFanSpeed = SET_FAN_SPEED_INVALID;

VanPacketParseResult_t ParseAirCon1Pkt(TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#464
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#464
    // https://github.com/morcibacsi/PSAVanCanBridge/blob/master/src/Van/Structs/VanAirConditioner1Structs.h

    const uint8_t* data = pkt.Data();

    // data[0] - bits:
    // - & 0x01: Rear window heater ON
    // - & 0x04: Air recirculation ON
    // - & 0x10: A/C icon?
    // - & 0x20: A/C icon? "YES" : "NO",  // TODO - what is this? This bit is always 0
    // - & 0x40: mode ? "AUTO" : "MANUAL",  // TODO - what is this? This bit is always 0
    // - & 0x0A: demist ? "ON" : "OFF",  // TODO - what is this? These bits are always 0

    // data[4] - reported fan speed
    //
    // Real-world tests: reported fan_speed values with various settings of the fan speed icon (0 = fan icon
    // not visible at all ... 7 = all four blades visible), under varying conditions:
    //
    // 1.) Recirculation = OFF, rear window heater = OFF, A/C = OFF:
    //     Fan icon not visible at all = 0
    //     Fan icon with all empty blades = 4
    //     One blade visible = 4 (same as previous!)
    //     Two blades - low = 6
    //     Two blades - high = 7
    //     Three blades - low = 9
    //     Three blades - high = 10
    //     Four blades = 19
    //
    // 2.) Recirculation = ON, rear window heater = OFF, A/C = OFF:
    //     Fan icon not visible at all = 0
    //     Fan icon with all empty blades = 4
    //     One blade visible = 4 (same as previous!)
    //     Two blades - low = 6
    //     Two blades - high = 7
    //     Three blades - low = 9
    //     Three blades - high = 10
    //     Four blades = 19
    //
    // 3.) Recirculation = OFF, rear window heater = OFF, A/C = ON (compressor running):
    //     Fan icon not visible at all = 0 (A/C icon will turn off)
    //     Fan icon with all empty blades = 4
    //     One blade visible = 4 (same as previous!)
    //     Two blades - low = 8
    //     Two blades - high = 9
    //     Three blades - low = 11
    //     Three blades - high = 12
    //     Four blades = 21
    //
    // 4.) Recirculation = OFF, rear window heater = ON, A/C = OFF:
    //     Fan icon not visible at all = 12
    //     Fan icon with all empty blades = 15
    //     One blade visible = 16
    //     Two blades - low = 17
    //     Two blades - high = 19
    //     Three blades - low = 20
    //     Three blades - high = 22
    //     Four blades = 30
    //
    // 5.) Recirculation = OFF, rear window heater = ON, = A/C ON:
    //     Fan icon not visible at all = 12 (A/C icon will turn off)
    //     Fan icon with all empty blades = 17
    //     One blade visible = 18
    //     Two blades - low = 19
    //     Two blades - high = 21
    //     Three blades - low = 22
    //     Three blades - high = 24
    //     Four blades = 32
    //
    // All above with demist ON --> makes no difference
    //
    // In "AUTO" mode, the fan speed varies gradually over time in increments or decrements of 1.

    bool rear_heater = data[0] & 0x01;
    bool ac_icon = data[0] & 0x10;
    setFanSpeed = data[4];
    if (rear_heater) setFanSpeed -= 12;
    if (ac_icon) setFanSpeed -= 2;
    setFanSpeed =
        setFanSpeed >= 18 ? 7 :  // Four blades
        setFanSpeed >= 10 ? 6 :  // Three blades - high
        setFanSpeed >= 8 ? 5 :  // Three blades - low
        setFanSpeed == 7 ? 4 :  // Two blades - high
        setFanSpeed >= 5 ? 3 :  // Two blades - low
        setFanSpeed == 4 ? 2 :  // One blade (2) or all empty blades (1)
        setFanSpeed == 3 ? 1 :  // All empty blades (1)
        0;  // Fan icon not visible at all

    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"ac_icon\": \"%s\",\n"
            "\"recirc\": \"%s\",\n"
            "\"rear_heater_1\": \"%s\",\n"
            "\"reported_fan_speed\": \"%u\",\n"
            "\"set_fan_speed\": \"%u\"\n"
        "}\n"
    "}\n";

    int at = snprintf_P(buf, n, jsonFormatter,
        ac_icon ? onStr : offStr,
        data[0] & 0x04 ? onStr : offStr,
        rear_heater ? onStr : offStr,
        data[4],
        setFanSpeed
    );

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseAirCon1Pkt

// Set to true to disable (once) duplicate detection. For use when switching to other units.
bool SkipAirCon2PktDupDetect = false;

#define CONDENSER_PRESSURE_INVALID (0xFF)
uint8_t condenserPressure = CONDENSER_PRESSURE_INVALID;

#define EVAPORATOR_TEMP_INVALID (0xFFFF)
uint16_t evaporatorTemp = EVAPORATOR_TEMP_INVALID;

VanPacketParseResult_t ParseAirCon2Pkt(TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#4DC
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#4DC
    // https://github.com/morcibacsi/PSAVanCanBridge/blob/master/src/Van/Structs/VanAirConditioner2Structs.h

    const uint8_t* data = pkt.Data();

    // Avoid continuous updates if a temperature value is constantly toggling between 2 values, while the rest of the
    // data is the same.

    static uint16_t prevStatusBits = 0xFFFF;  // Value that can never come from a packet itself
    uint8_t statusBits = data[0];

    static uint16_t prevContactKeyData = 0xFFFF;  // Value that can never come from a packet itself
    uint8_t contactKeyData = data[1];

    static uint16_t lastReportedCondenserPressure = 0xFFFF;  // Value that can never come from a packet itself
    static uint16_t prevReportedCondenserPressure = 0xFFFF;
    condenserPressure = data[2];
    static uint8_t prevCondenserPressure = condenserPressure;
    if (condenserPressure == CONDENSER_PRESSURE_INVALID) condenserPressure = prevCondenserPressure;

    static uint32_t lastReportedEvaporatorTemp = 0xFFFFFFFF;  // Value that can never come from a packet itself
    static uint32_t prevReportedEvaporatorTemp = 0xFFFFFFFF;
    evaporatorTemp = (uint16_t)data[3] << 8 | data[4];
    static uint16_t prevEvaporatorTemp = evaporatorTemp;
    if (evaporatorTemp == EVAPORATOR_TEMP_INVALID) evaporatorTemp = prevEvaporatorTemp;

    // New data if:
    // - valid value, and
    // - different from last reported, and either:
    //   - two times in a row the same value, or
    //   - different from previously reported
    bool hasNewData =
        SkipAirCon2PktDupDetect
        || statusBits != prevStatusBits
        || contactKeyData != prevContactKeyData

        || (condenserPressure != CONDENSER_PRESSURE_INVALID && condenserPressure != lastReportedCondenserPressure && condenserPressure == prevCondenserPressure)
        || (condenserPressure != CONDENSER_PRESSURE_INVALID && condenserPressure != lastReportedCondenserPressure && condenserPressure != prevReportedCondenserPressure)

        || (evaporatorTemp != EVAPORATOR_TEMP_INVALID && evaporatorTemp != lastReportedEvaporatorTemp && evaporatorTemp == prevEvaporatorTemp)
        || (evaporatorTemp != EVAPORATOR_TEMP_INVALID && evaporatorTemp != lastReportedEvaporatorTemp && evaporatorTemp != prevReportedEvaporatorTemp);

    SkipAirCon2PktDupDetect = false;
    prevStatusBits = statusBits;
    prevContactKeyData = contactKeyData;
    prevCondenserPressure = condenserPressure;
    prevEvaporatorTemp = evaporatorTemp;

    if (! hasNewData) return VAN_PACKET_DUPLICATE;

    prevReportedCondenserPressure = lastReportedCondenserPressure;
    lastReportedCondenserPressure = condenserPressure;
    prevReportedEvaporatorTemp = lastReportedEvaporatorTemp;
    lastReportedEvaporatorTemp = evaporatorTemp;

    float evaporatorTempCelsius = evaporatorTemp / 10.0 - 40.0;

    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"contact_key_on\": \"%s\",\n"
            "\"ac_enabled\": \"%s\",\n"
            "\"rear_heater_2\": \"%s\",\n"
            "\"ac_compressor\": \"%s\",\n"
            "\"contact_key_position_ac\": \"%s\",\n"
            "\"condenser_pressure_bar\": \"%s\",\n"
            "\"condenser_pressure_psi\": \"%s\",\n"
            "\"evaporator_temp\": \"%s\"\n"
        "}\n"
    "}\n";

    char floatBuf[3][MAX_FLOAT_SIZE];
    int at = snprintf_P(buf, n, jsonFormatter,
        statusBits & 0x80 ? yesStr : noStr,
        statusBits & 0x40 ? yesStr : noStr,
        statusBits & 0x20 ? onStr : offStr,
        statusBits & 0x01 ? onStr : offStr,

        contactKeyData == 0x1C ? PSTR("ACC_OR_OFF") :
        contactKeyData == 0x18 ? PSTR("ACC-->OFF") :
        contactKeyData == 0x04 ? PSTR("ON-->ACC") :
        contactKeyData == 0x00 ? onStr :
        ToHexStr(contactKeyData),

        condenserPressure == CONDENSER_PRESSURE_INVALID ? notApplicable2Str :
            ToFloatStr(floatBuf[0], ToBar(condenserPressure), 1),

        condenserPressure == CONDENSER_PRESSURE_INVALID ? notApplicable2Str :
            ToFloatStr(floatBuf[1], ToPsi(condenserPressure), 0),

        evaporatorTemp == EVAPORATOR_TEMP_INVALID ? notApplicable3Str :
            mfdTemperatureUnit == MFD_TEMPERATURE_UNIT_CELSIUS ?
                ToFloatStr(floatBuf[2], evaporatorTempCelsius, 1) :
                ToFloatStr(floatBuf[2], ToFahrenheit(evaporatorTempCelsius), 0)
    );

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseAirCon2Pkt

// Saved equipment status (volatile)
bool cdChangerCartridgePresent = false;

VanPacketParseResult_t ParseCdChangerPkt(TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#4EC
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#4EC
    // https://github.com/morcibacsi/PSAVanCanBridge/blob/master/src/Van/Structs/VanCdChangerStructs.h

    int dataLen = pkt.DataLen();
    if (dataLen == 0) return VAN_PACKET_NO_CONTENT; // "Request" packet; nothing to show
    if (dataLen != 12) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

    const uint8_t* data = pkt.Data();

    bool searching = data[2] & 0x10;
    bool loading = data[2] & 0x08;
    bool ejecting = data[2] == 0xC1 && data[10] == 0;  // Ejecting cartridge

    cdChangerCartridgePresent = data[3] == 0x16; // "not present" is 0x06

    uint8_t trackTimeMin = data[4];
    uint8_t trackTimeSec = data[5];
    bool trackTimeValid = trackTimeMin != 0xFF && trackTimeSec != 0xFF;
    char trackTimeStr[7];
    if (trackTimeValid) sprintf_P(trackTimeStr, PSTR("%X:%02X"), trackTimeMin, trackTimeSec);

    uint8_t totalTracks = data[8];
    bool totalTracksValid = totalTracks != 0xFF;
    char totalTracksStr[3];
    if (totalTracksValid) sprintf_P(totalTracksStr, PSTR("%X"), totalTracks);

    uint8_t currentTrack = data[6];
    uint8_t currentDisc = data[7];

    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"cd_changer_present\": \"%s\",\n"
            "\"cd_changer_status_loading\": \"%s\",\n"
            "\"cd_changer_status_eject\": \"%s\",\n"
            "\"cd_changer_status_operational\": \"%s\",\n"
            "\"cd_changer_status_searching\": \"%s\",\n"
            "\"cd_changer_status\": \"%s\",\n"
            "\"cd_changer_status_pause\": \"%s\",\n"
            "\"cd_changer_status_play\": \"%s\",\n"
            "\"cd_changer_status_fast_forward\": \"%s\",\n"
            "\"cd_changer_status_rewind\": \"%s\",\n"
            "\"cd_changer_cartridge_present\": \"%s\",\n"
            "\"cd_changer_track_time\": \"%s\",\n"
            "\"cd_changer_current_track\": \"%s\",\n"
            "\"cd_changer_total_tracks\": \"%s\",\n"
            "\"cd_changer_disc_1_present\": \"%s\",\n"
            "\"cd_changer_disc_2_present\": \"%s\",\n"
            "\"cd_changer_disc_3_present\": \"%s\",\n"
            "\"cd_changer_disc_4_present\": \"%s\",\n"
            "\"cd_changer_disc_5_present\": \"%s\",\n"
            "\"cd_changer_disc_6_present\": \"%s\",\n"
            "\"cd_changer_current_disc\": \"%s\",\n"
            "\"cd_changer_random\": \"%s\"\n"
        "}\n"
    "}\n";

    int at = snprintf_P(buf, n, jsonFormatter,

        data[2] & 0x40 ? yesStr : noStr,  // CD changer unit present
        loading ? yesStr : noStr,  // Loading disc
        ejecting ? yesStr : noStr,  // Ejecting cartridge

        // Head unit powered on; CD changer operational (either standby or selected as audio source)
        data[2] & 0x80 ? yesStr : noStr,

        searching ? onStr : offStr,

        data[2] == 0x40 ? PSTR("POWER_OFF") :  // Not sure
        data[2] == 0x41 ? PSTR("POWER_ON") :  // Not sure
        data[2] == 0x49 ? PSTR("INITIALIZE") :  // Not sure
        data[2] == 0x4B ? PSTR("LOADING") :
        data[2] == 0xC0 ? PSTR("POWER_ON_READY") :  // Not sure
        data[2] == 0xC1 ?
            data[10] == 0 ? PSTR("EJECT") :
            PSTR("PAUSE") :
        data[2] == 0xC3 ? PSTR("PLAY") :
        data[2] == 0xC4 ? PSTR("FAST_FORWARD") :
        data[2] == 0xC5 ? PSTR("REWIND") :
        data[2] == 0xD3 ?
            // "PLAY-SEARCHING" (data[2] == 0xD3) with invalid values for currentDisc and currentTrack indicates
            // an error condition, e.g. disc inserted wrong way round
            currentDisc == 0xFF && currentTrack == 0xFF ? PSTR("ERROR") :
            PSTR("PLAY-SEARCHING") :
        ToHexStr(data[2]),

        (data[2] & 0x07) == 0x01 && ! ejecting && ! loading ? onStr : offStr,  // Pause
        (data[2] & 0x07) == 0x03 && ! searching && ! loading ? onStr : offStr,  // Play
        (data[2] & 0x07) == 0x04 ? onStr : offStr,  // Fast forward
        (data[2] & 0x07) == 0x05 ? onStr : offStr,  // Rewind

        cdChangerCartridgePresent ? yesStr : noStr,

        trackTimeValid ? trackTimeStr : PSTR("--:--"),

        currentTrack == 0xFF ? notApplicable2Str : ToBcdStr(currentTrack),
        totalTracksValid ? totalTracksStr : notApplicable2Str,

        data[10] & 0x01 ? yesStr : noStr,
        data[10] & 0x02 ? yesStr : noStr,
        data[10] & 0x04 ? yesStr : noStr,
        data[10] & 0x08 ? yesStr : noStr,
        data[10] & 0x10 ? yesStr : noStr,
        data[10] & 0x20 ? yesStr : noStr,

        // Pass the number only if disc is actually present: if cartridge is present without any discs, then
        // 'currentDisc' (data[7]) will be incorrectly reported as '1'.
        currentDisc == 0xFF ? notApplicable2Str :
            currentDisc == 1 && data[10] & 0x01 ? PSTR("1") :
            currentDisc == 2 && data[10] & 0x02 ? PSTR("2") :
            currentDisc == 3 && data[10] & 0x04 ? PSTR("3") :
            currentDisc == 4 && data[10] & 0x08 ? PSTR("4") :
            currentDisc == 5 && data[10] & 0x10 ? PSTR("5") :
            currentDisc == 6 && data[10] & 0x20 ? PSTR("6") :
            notApplicable2Str,

        data[1] == 0x01 ? onStr : offStr
    );

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseCdChangerPkt

String satnavCurrentStreet = "";

VanPacketParseResult_t ParseSatNavStatus1Pkt(TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#54E
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#54E

    const uint8_t* data = pkt.Data();
    uint16_t status = (uint16_t)data[1] << 8 | data[2];
    bool noDiscPresent = data[4] == 0x0E;

    if (noDiscPresent)
    {
        satnavCurrentStreet = "";
        isCurrentStreetKnown = false;
    } // if

    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"satnav_status_1\": \"%s%s\",\n"
            "\"satnav_destination_not_accessible\": \"%s\",\n"
            "\"satnav_arrived_at_destination\": \"%s\",\n"

            // Send this before "satnav_new_guidance_instruction", for correct handling by MFD.js
            "\"satnav_guidance_display_can_be_dimmed\": \"%s\",\n"

            "\"satnav_new_guidance_instruction\": \"%s\",\n"
            "\"satnav_audio_start\": \"%s\",\n"
            "\"satnav_audio_end\": \"%s\",\n"
            "\"satnav_calculating_route\": \"%s\",\n"
            "\"satnav_guidance_ended\": \"%s\",\n"
            "\"satnav_calculating_route_2\": \"%s\",\n"

            "\"satnav_last_instruction_given\": \"%s\",\n"
            "\"satnav_entering_new_dest\": \"%s\"\n"
        "}\n"
    "}\n";

    int at = snprintf_P(buf, n, jsonFormatter,

        status == 0x0000 ? noneStr :
        status == 0x0001 ? PSTR("DESTINATION_NOT_ON_MAP") :
        status == 0x0020 ? ToHexStr(status) :  // Seen this but what is it?? Nearly at destination ??
        status == 0x0080 ? PSTR("READY") :
        status == 0x0101 ? ToHexStr(status) :  // Seen this but what is it??
        status == 0x0200 ? PSTR("READING_DISC") :
        status == 0x0220 ? PSTR("READING_DISC") :  // TODO - guessing
        status == 0x0300 ? PSTR("IN_GUIDANCE_MODE") :
        status == 0x0301 ? PSTR("IN_GUIDANCE_MODE") :
        status == 0x0320 ? PSTR("STOPPING_GUIDANCE") :
        status == 0x0400 ? PSTR("START_OF_AUDIO_MESSAGE") :
        status == 0x0410 ? PSTR("ARRIVED_AT_DESTINATION_AUDIO_ANNOUNCEMENT") :
        status == 0x0430 ? PSTR("ARRIVED_AT_DESTINATION_AUDIO_ANNOUNCEMENT") :
        status == 0x0600 ? PSTR("START_OF_AUDIO_MESSAGE") :  // TODO - guessing
        status == 0x0700 ? PSTR("INSTRUCTION_AUDIO_MESSAGE_START") :
        status == 0x0701 ? PSTR("INSTRUCTION_AUDIO_MESSAGE_START") :
        status == 0x0710 ? PSTR("ARRIVED_AT_DESTINATION_AUDIO_ANNOUNCEMENT") :  // TODO - guessing
        status == 0x0800 ? PSTR("END_OF_AUDIO_MESSAGE") :  // Follows 0x0400, 0x0700, 0x0701
        status == 0x4000 ? PSTR("GUIDANCE_STOPPED") :
        status == 0x4001 ? PSTR("DESTINATION_NOT_ACCESSIBLE_BY_ROAD") :  // And guidance ended immediately
        status == 0x4080 ? PSTR("GUIDANCE_STOPPED") :
        status == 0x4081 ? PSTR("GUIDANCE_STOPPED") :
        status == 0x4200 ? PSTR("ARRIVED_AT_DESTINATION_POPUP") :  // TODO - guessing
        status == 0x9000 ? PSTR("READING_DISC") :
        status == 0x9080 ? PSTR("START_COMPUTING_ROUTE") : // TODO - guessing
        status == 0xD001 ? PSTR("DESTINATION_NOT_ON_MAP") :  // TODO - guessing
        ToHexStr(status),

        data[4] == 0x0B ? emptyStr :  // Seen with status 0x4001 and 0xD001
        data[4] == 0x0C ? PSTR(" DISC_UNREADABLE") :
        noDiscPresent ? PSTR(" NO_DISC") :
        emptyStr,

        data[2] & 0x01 ? yesStr : noStr,  // satnav_destination_not_accessible
        data[2] & 0x10 ? yesStr : noStr,  // satnav_arrived_at_destination

        data[1] & 0x02 ? yesStr : noStr,  // satnav_guidance_display_can_be_dimmed (next instruction is far away)
        data[1] & 0x01 ? yesStr : noStr,  // satnav_new_guidance_instruction

        data[1] & 0x04 ? yesStr : noStr,  // satnav_audio_start
        data[1] & 0x08 ? yesStr : noStr,  // satnav_audio_end
        data[1] & 0x10 ? yesStr : noStr,  // satnav_calculating_route
        data[1] & 0x40 ? yesStr : noStr,  // satnav_guidance_ended ?
        data[1] & 0x80 ? yesStr : noStr,  // satnav_calculating_route_2

        data[2] & 0x20 ? yesStr : noStr,  // satnav_last_instruction_given
        data[2] & 0x80 ? yesStr : noStr   // satnav_entering_new_dest ?
    );

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseSatNavStatus1Pkt

// Sat nav equipment detection
bool satnavEquipmentDetected = true;

// Index of user-selected time unit
enum SATNAV_status_2_t
{
    SATNAV_STATUS_2_INITIALIZING = 0x00,
    SATNAV_STATUS_2_IDLE = 0x01,
    SATNAV_STATUS_2_IN_GUIDANCE_MODE = 0x05
}; // enum SATNAV_status_2_t

// Saved equipment status (volatile)
uint8_t satnavStatus2;
PGM_P satnavStatus2Str = emptyStr;
bool satnavDiscRecognized = false;
bool satnavInitialized = false;
bool reachedDestination = false;
uint32_t satnavDownloadProgress = 0;

VanPacketParseResult_t ParseSatNavStatus2Pkt(TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#7CE
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#7CE

    int dataLen = pkt.DataLen();
    if (dataLen != 0 && dataLen != 20) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

    // Count number of times a "read" packet did not get an in-frame response
    static int nSinglePacketAttempts = 0;
    static int nAttempts = 0;

    if (dataLen == 0)
    {
        // This is a "read" packet that did not get an in-frame response. If the sat nav unit is not there, this
        // packet is sent by the MFD over and over again.

        static unsigned long lastPacketReceived = 0;

        unsigned long now = pkt.Millis();  // Retrieve packet reception time stamp from ISR
        unsigned long packetInterval = now - lastPacketReceived;  // Arithmetic has safe roll-over
        lastPacketReceived = now;

        if (packetInterval < 100)
        {
            // As long as the MFD assumes the sat nav equipment is present (e.g. at boot time), it sends 5 bursts
            // of each 3 packets. Ater that it sends one packet per second.
            nSinglePacketAttempts = 0;

            return VAN_PACKET_NO_CONTENT;
        }
        else
        {
            // Already reported "no equipment present"?
            if (! satnavEquipmentDetected) return VAN_PACKET_NO_CONTENT;

            // Count 9 attempts: 5 bursty attempts followed by 4 single-packet attempts
            nAttempts++;
            nSinglePacketAttempts++;

            #define SATNAV_NO_ANSWER_AFTER_ATTEMPTS (9)
            #define SATNAV_NO_ANSWER_AFTER_SINGLE_PACKET_ATTEMPTS (4)
            if (nAttempts < SATNAV_NO_ANSWER_AFTER_ATTEMPTS && nSinglePacketAttempts < SATNAV_NO_ANSWER_AFTER_SINGLE_PACKET_ATTEMPTS)
            {
                return VAN_PACKET_NO_CONTENT;
            } // if

            satnavEquipmentDetected = false;
        } // if

        const static char jsonFormatter[] PROGMEM =
        "{\n"
            "\"event\": \"display\",\n"
            "\"data\":\n"
            "{\n"
                "\"satnav_equipment_present\": \"%s\"\n"
            "}\n"
        "}\n";

        int at = snprintf_P(buf, n, jsonFormatter, satnavEquipmentDetected ? yesStr : noStr);

        // JSON buffer overflow?
        if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

        return VAN_PACKET_PARSE_OK;
    } // if

    nAttempts = 0;
    nSinglePacketAttempts = 0;

    const uint8_t* data = pkt.Data();

    if (satnavEquipmentDetected)
    {
        // Ignore duplicates of the "long" (20 byte) packets
        static uint8_t packetData[VAN_MAX_DATA_BYTES];  // Previous packet data
        if (memcmp(data, packetData, dataLen) == 0) return VAN_PACKET_DUPLICATE;
        memcpy(packetData, data, dataLen);
    } // if

    satnavStatus2 = data[1] & 0x0F;

    bool downloadFinished = data[1] & 0x80;
    if (downloadFinished) satnavDownloadProgress = 0;

    satnavStatus2Str =
        satnavStatus2 == SATNAV_STATUS_2_INITIALIZING ? PSTR("INITIALIZING") :
        satnavStatus2 == SATNAV_STATUS_2_IDLE ? PSTR("IDLE") :
        satnavStatus2 == SATNAV_STATUS_2_IN_GUIDANCE_MODE ? PSTR("IN_GUIDANCE_MODE") :
        emptyStr;
    satnavInitialized = satnavStatus2 != SATNAV_STATUS_2_INITIALIZING;

    static bool wasSatnavGuidanceActive = false;
    isSatnavGuidanceActive = satnavStatus2 == SATNAV_STATUS_2_IN_GUIDANCE_MODE;
    satnavDiscRecognized = (data[2] & 0x70) == 0x30;

    // 0xE0 as boundary for "reverse": just guessing. Do we ever drive faster than 224 km/h?
    uint16_t gpsSpeedAbs = data[16] < 0xE0 ? data[16] : 0xFF - data[16] + 1;

    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"satnav_route_computed\": \"%s\",\n" // Report this first. TODO - explain why
            "\"satnav_status_2\": \"%s\",\n"
            "\"satnav_destination_reachable\": \"%s\",\n"
            "\"satnav_on_map\": \"%s\",\n"
            "\"satnav_download_finished\": \"%s\",\n"
            "\"satnav_disc_recognized\": \"%s\",\n"
            "\"satnav_gps_fix\": \"%s\",\n"
            "\"satnav_gps_fix_lost\": \"%s\",\n"
            "\"satnav_gps_scanning\": \"%s\",\n"
            "\"satnav_language\": \"%s\",\n"
            "\"satnav_gps_speed\": \"%s%s\"";

    char floatBuf[MAX_FLOAT_SIZE];
    int at = snprintf_P(buf, n, jsonFormatter,

        data[1] & 0x20 ? noStr : yesStr,

        satnavStatus2Str,

        data[1] & 0x10 ? yesStr : noStr,
        data[1] & 0x40 ? noStr : yesStr,
        downloadFinished ? yesStr : noStr,

        satnavDiscRecognized ? yesStr :
        (data[2] & 0x70) == 0x70 ? noStr :
        ToHexStr((uint8_t)(data[2] & 0x70)),

        data[2] & 0x01 ? yesStr : noStr,
        data[2] & 0x02 ? yesStr : noStr,
        data[2] & 0x04 ? yesStr : noStr,

        data[5] == 0x00 ? PSTR("FRENCH") :
        data[5] == 0x01 ? PSTR("ENGLISH") :
        data[5] == 0x02 ? PSTR("GERMAN") :
        data[5] == 0x03 ? PSTR("SPANISH") :
        data[5] == 0x04 ? PSTR("ITALIAN") :
        data[5] == 0x06 ? PSTR("DUTCH") :
        notApplicable3Str,

        data[16] >= 0xE0 ? dashStr : emptyStr,
        mfdDistanceUnit == MFD_DISTANCE_UNIT_METRIC ?
            ToFloatStr(floatBuf, gpsSpeedAbs, 0) :
            ToFloatStr(floatBuf, ToMiles(gpsSpeedAbs), 0)
    );

    // TODO - what is this?
    uint16_t zzz = (uint16_t)data[9] << 8 | data[10];
    if (zzz != 0x00)
    {
        at += at >= n ? 0 : snprintf_P(buf + at, n - at, PSTR(",\n\"satnav_zzz\": \"%u\""), zzz);
    } // if

    if (data[17] != 0x00)
    {
        reachedDestination = data[17] & 0x80;

        at += at >= n ? 0 :
            snprintf_P(buf + at, n - at, PSTR(",\n\"satnav_guidance_status\": \"%s%s%s%s%s%s%s\""),

                data[17] & 0x01 ? PSTR("LOADING_AUDIO_FRAGMENT ") : emptyStr,
                data[17] & 0x02 ? PSTR("AUDIO_OUTPUT ") : emptyStr,
                data[17] & 0x04 ? PSTR("NEW_GUIDANCE_INSTRUCTION ") : emptyStr,
                data[17] & 0x08 ? PSTR("READING_DISC ") : emptyStr,
                data[17] & 0x10 ? PSTR("COMPUTING_ROUTE ") : emptyStr,

                // TODO - not sure. This bit is also set when there is no disc present
                data[17] & 0x20 ? PSTR("DISC_PRESENT ") : emptyStr,

                reachedDestination ? PSTR("REACHED_DESTINATION ") : emptyStr
            );
    } // if

    if (! satnavEquipmentDetected)
    {
        // Large packet received, so sat nav equipment is obviously present. Add this to the JSON data.
        // Variable 'satnavEquipmentDetected' is set to 'true' after successful return; see below.
        at += at >= n ? 0 :
            snprintf_P(buf + at, n - at, PSTR(",\n\"satnav_equipment_present\": \"YES\""));
    } // if

    bool updateScreen = false;
    if (! wasSatnavGuidanceActive && isSatnavGuidanceActive)
    {
        // Going into guidance mode
        UpdateLargeScreenForGuidanceModeOn();
        updateScreen = true;
    }
    else if ((wasSatnavGuidanceActive && ! isSatnavGuidanceActive) || reachedDestination)
    {
        // Going out of guidance mode
        UpdateLargeScreenForGuidanceModeOff();
        updateScreen = true;
    } // if

    wasSatnavGuidanceActive = isSatnavGuidanceActive;

    if (updateScreen)
    {
        at += at >= n ? 0 :
            snprintf_P(buf + at, n - at,
                PSTR
                (
                    ",\n"
                    "\"small_screen\": \"%s\",\n"
                    "\"trip_computer_screen_tab\": \"%s\",\n"
                    "\"large_screen\": \"%s\""
                ),
                SmallScreenStr(),
                TripComputerStr(),
                LargeScreenStr()
            );
    } // if

    at += at >= n ? 0 : snprintf_P(buf + at, n - at, PSTR("\n}\n}\n"));

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    satnavEquipmentDetected = true;

    return VAN_PACKET_PARSE_OK;
} // ParseSatNavStatus2Pkt

// Value stored in (emulated) EEPROM
uint8_t satnavGuidancePreference = SGP_INVALID;
#define SATNAV_GUIDANCE_PREFERENCE_EEPROM_POS (2)

// Initialize satnavGuidancePreference, if necessary
void InitSatnavGuidancePreference()
{
    if (IsValidSatNavGuidancePreferenceValue(satnavGuidancePreference)) return;

    satnavGuidancePreference = EEPROM.read(SATNAV_GUIDANCE_PREFERENCE_EEPROM_POS);

    // TODO - remove
    Serial.printf_P(
        PSTR("satnavGuidancePreference: read value %u (%s) from EEPROM position %d\n"),
        satnavGuidancePreference,
        SatNavGuidancePreferenceStr(satnavGuidancePreference),
        SATNAV_GUIDANCE_PREFERENCE_EEPROM_POS);

    if (IsValidSatNavGuidancePreferenceValue(satnavGuidancePreference)) return;

    // When the original MFD is plugged in, this is what it starts with
    satnavGuidancePreference = SGP_FASTEST_ROUTE;

    WriteEeprom(SATNAV_GUIDANCE_PREFERENCE_EEPROM_POS, satnavGuidancePreference, PSTR("Sat nav guidance preference"));
} // InitSatnavGuidancePreference

VanPacketParseResult_t ParseSatNavStatus3Pkt(TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#8CE
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#8CE

    int dataLen = pkt.DataLen();
    if (dataLen != 2 && dataLen != 3 && dataLen != 17) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

    const uint8_t* data = pkt.Data();
    int at = 0;

    if (dataLen == 2)
    {
        satnavDownloadProgress = 0;

        uint16_t status = (uint16_t)data[0] << 8 | data[1];

        const static char jsonFormatter[] PROGMEM =
        "{\n"
            "\"event\": \"display\",\n"
            "\"data\":\n"
            "{\n"
                "\"satnav_status_3\": \"%s\"\n"
            "}\n"
        "}\n";

        at = snprintf_P(buf, n, jsonFormatter,

            status == 0x0000 ? PSTR("COMPUTING_ROUTE") :
            status == 0x0001 ? PSTR("STOPPING_NAVIGATION") :
            status == 0x0003 ? PSTR("READING_OUT_LAST_INSTRUCTION") :  // After pressing left stalk during guidance
            status == 0x0101 ? PSTR("VOCAL_SYNTHESIS_LEVEL_SETTING") :  // TODO - Not sure
            status == 0x0104 ? PSTR("VOCAL_SYNTHESIS_LEVEL_SETTING_VIA_HEAD_UNIT") :  // TODO - Not sure

            // This starts when the nag screen is accepted and is seen repeatedly when selecting a destination
            // and during guidance. It stops after a "STOPPING_NAVIGATION" status message.
            status == 0x0108 ? PSTR("SATNAV_IN_OPERATION") :

            status == 0x0110 ? PSTR("VOCAL_SYNTHESIS_LEVEL_SETTING") :
            status == 0x0120 ? PSTR("ACCEPTED_TERMS_AND_CONDITIONS") :
            status == 0x0140 ? PSTR("SYSTEM_ID_READ") : // TODO - not sure
            status == 0x0180 ? ToHexStr(status) :  // Changing distance unit (km <--> mi)?
            status == 0x0300 ? PSTR("SATNAV_LANGUAGE_SET_TO_FRENCH") :
            status == 0x0301 ? PSTR("SATNAV_LANGUAGE_SET_TO_ENGLISH") :
            status == 0x0302 ? PSTR("SATNAV_LANGUAGE_SET_TO_GERMAN") :
            status == 0x0303 ? PSTR("SATNAV_LANGUAGE_SET_TO_SPANISH") :
            status == 0x0304 ? PSTR("SATNAV_LANGUAGE_SET_TO_ITALIAN") :
            status == 0x0306 ? PSTR("SATNAV_LANGUAGE_SET_TO_DUTCH") :
            status == 0x0C01 ? PSTR("CD_ROM_FOUND") :
            status == 0x0C02 ? PSTR("POWERING_OFF") :
            ToHexStr(status)
        );
        if (status == 0x0C02)
        {
            satnavInitialized = false;
            satnavStatus2Str = emptyStr;
            satnavCurrentStreet = "";
            isCurrentStreetKnown = false;
        } // if

    }
    else if (dataLen == 3)
    {
        // Sat nav guidance preference

        satnavGuidancePreference = data[1];

        const static char jsonFormatter[] PROGMEM =
        "{\n"
            "\"event\": \"display\",\n"
            "\"data\":\n"
            "{\n"
                "\"satnav_guidance_preference\": \"%s\"\n"
            "}\n"
        "}\n";

        at = snprintf_P(buf, n, jsonFormatter, SatNavGuidancePreferenceStr(satnavGuidancePreference));

        WriteEeprom(SATNAV_GUIDANCE_PREFERENCE_EEPROM_POS, satnavGuidancePreference, PSTR("Sat nav guidance preference"));
    }
    else if (dataLen == 17 && data[0] == 0x20)
    {
        // Some set of ID strings. Stays the same even when the navigation CD is changed.

        const static char jsonFormatter[] PROGMEM =
        "{\n"
            "\"event\": \"display\",\n"
            "\"data\":\n"
            "{\n"
                "\"satnav_system_id\":\n"
                "[";

        at = snprintf_P(buf, n, jsonFormatter);

        char txt[VAN_MAX_DATA_BYTES - 1 + 1];  // Max 28 data bytes, minus header (1), plus terminating '\0'

        bool first = true;
        int at2 = 1;
        while (at2 < dataLen)
        {
            strncpy(txt, (const char*) data + at2, dataLen - at2);
            txt[dataLen - at2] = 0;
            at += at >= n ? 0 :
                snprintf_P(buf + at, n - at, PSTR("%s\n\"%s\""), first ? emptyStr : commaStr, txt);
            at2 += strlen(txt) + 1;
            first = false;
        } // while

        at += at >= n ? 0 : snprintf_P(buf + at, n - at, PSTR("\n]\n}\n}\n"));
    } // if

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseSatNavStatus3Pkt

VanPacketParseResult_t ParseSatNavGuidanceDataPkt(TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#9CE
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#9CE

    const uint8_t* data = pkt.Data();

    uint16_t currHeading = (uint16_t)data[1] << 8 | data[2];  // in compass degrees (0...359)
    uint16_t headingToDestination = (uint16_t)data[3] << 8 | data[4];  // in compass degrees (0...359)
    uint16_t roadDistanceToDestination = (uint16_t)(data[5] & 0x7F) << 8 | data[6];
    bool roadDistanceToDestinationInKmsMiles = data[5] & 0x80;
    uint16_t gpsDistanceToDestination = (uint16_t)(data[7] & 0x7F) << 8 | data[8];
    bool gpsDistanceToDestinationInKmsMiles = data[7] & 0x80;
    uint16_t distanceToNextTurn = (uint16_t)(data[9] & 0x7F) << 8 | data[10];
    bool distanceToNextTurnInKmsMiles = data[9] & 0x80;
    uint16_t headingOnRoundabout = (uint16_t)data[11] << 8 | data[12];

    // TODO - Not sure, just guessing. Could also be number of instructions still to be done.
    uint16_t minutesToTravel = (uint16_t)data[13] << 8 | data[14];

    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"satnav_curr_heading_compass_needle_rotation\": \"%u\",\n"
            "\"satnav_curr_heading_compass_needle\":\n"
            "{\n"
                "\"style\":\n"
                "{\n"
                    "\"transform\": \"rotate(%udeg)\"\n"
                "}\n"
            "},\n"
            "\"satnav_curr_heading\": \"%u\",\n"  // degrees
            "\"satnav_heading_to_dest_pointer_rotation\": \"%u\",\n"
            "\"satnav_heading_to_dest_pointer\":\n"
            "{\n"
                "\"style\":\n"
                "{\n"
                    "\"transform\": \"rotate(%udeg)\"\n"
                "}\n"
            "},\n"
            "\"satnav_heading_to_dest\": \"%u\",\n"  // degrees
            "\"satnav_distance_to_dest_via_road\": \"%s %s\",\n"
            "\"satnav_distance_to_dest_via_straight_line\": \"%s %s\",\n"
            "\"satnav_turn_at\": \"%s %s\",\n"
            "\"satnav_heading_on_roundabout_as_text\": \"%s\",\n"  // degrees
            "\"satnav_minutes_to_travel\": \"%u\"\n"
        "}\n"
    "}\n";

    char floatBuf[4][MAX_FLOAT_SIZE];
    int at = snprintf_P(buf, n, jsonFormatter,

        360 - currHeading, // Compass needle direction is current heading, mirrored over a vertical line
        360 - currHeading,
        currHeading,
        (360 - currHeading + headingToDestination) % 360, // To show pointer indicating direction relative to current heading
        (360 - currHeading + headingToDestination) % 360,
        headingToDestination,

        ToFloatStr(floatBuf[0], roadDistanceToDestination, 0),

        mfdDistanceUnit == MFD_DISTANCE_UNIT_METRIC ?
            roadDistanceToDestinationInKmsMiles ? PSTR("km") : PSTR("m") :
            roadDistanceToDestinationInKmsMiles ? PSTR("mi") : PSTR("yd"),

        ToFloatStr(floatBuf[1], gpsDistanceToDestination, 0),

        mfdDistanceUnit == MFD_DISTANCE_UNIT_METRIC ?
            gpsDistanceToDestinationInKmsMiles ? PSTR("km") : PSTR("m") :
            gpsDistanceToDestinationInKmsMiles ? PSTR("mi") : PSTR("yd"),

        ToFloatStr(floatBuf[2], distanceToNextTurn, 0),

        mfdDistanceUnit == MFD_DISTANCE_UNIT_METRIC ?
            distanceToNextTurnInKmsMiles ? PSTR("km") : PSTR("m") :
            distanceToNextTurnInKmsMiles ? PSTR("mi") : PSTR("yd"),

        headingOnRoundabout == 0x7FFF ? notApplicable3Str : ToFloatStr(floatBuf[3], headingOnRoundabout, 0, false),
        minutesToTravel
    );

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseSatNavGuidanceDataPkt

VanPacketParseResult_t ParseSatNavGuidancePkt(TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#64E
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#64E

    int dataLen = pkt.DataLen();
    if (dataLen != 3 && dataLen != 4 && dataLen != 6 && dataLen != 9 && dataLen != 13 && dataLen != 16 && dataLen != 23)
    {
        return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
    } // if

    const uint8_t* data = pkt.Data();

    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"satnav_curr_turn_icon\": \"%s\",\n"

            "\"satnav_fork_icon_take_right_exit\": \"%s\",\n"
            "\"satnav_fork_icon_keep_right\": \"%s\",\n"
            "\"satnav_fork_icon_take_left_exit\": \"%s\",\n"
            "\"satnav_fork_icon_keep_left\": \"%s\",\n"

            "\"satnav_next_turn_icon\": \"%s\",\n"

            "\"satnav_next_fork_icon_take_right_exit\": \"%s\",\n"
            "\"satnav_next_fork_icon_keep_right\": \"%s\",\n"
            "\"satnav_next_fork_icon_take_left_exit\": \"%s\",\n"
            "\"satnav_next_fork_icon_keep_left\": \"%s\",\n"

            "\"satnav_turn_around_if_possible_icon\": \"%s\",\n"
            "\"satnav_follow_road_icon\": \"%s\",\n"
            "\"satnav_not_on_map_icon\": \"%s\"";

    // Determines which guidance icon(s) will be visible
    int at = snprintf_P(buf, n, jsonFormatter,

        // TODO - data[2] can be only 0x00, 0x01 or 0x02?
        (data[1] == 0x01 && (data[2] == 0x00 || data[2] == 0x01)) || (data[1] == 0x03 && data[2] != 0x02) ? onStr : offStr,

        (data[1] == 0x01 && data[2] == 0x02 && data[4] == 0x12) ||
        (data[1] == 0x03 && data[2] == 0x02 && data[6] == 0x12)
            ? onStr : offStr,

        (data[1] == 0x01 && data[2] == 0x02 && data[4] == 0x14) ||
        (data[1] == 0x03 && data[2] == 0x02 && data[6] == 0x14)
            ? onStr : offStr,

        // Never seen; just guessing
        (data[1] == 0x01 && data[2] == 0x02 && data[4] == 0x21) ||
        (data[1] == 0x03 && data[2] == 0x02 && data[6] == 0x21)
            ? onStr : offStr,

        (data[1] == 0x01 && data[2] == 0x02 && data[4] == 0x41) ||
        (data[1] == 0x03 && data[2] == 0x02 && data[6] == 0x41)
            ? onStr : offStr,

        data[1] == 0x03 && dataLen != 9 ? onStr : offStr,

        data[1] == 0x03 && dataLen == 9 && data[7] == 0x12 ? onStr : offStr,
        data[1] == 0x03 && dataLen == 9 && data[7] == 0x14 ? onStr : offStr,
        data[1] == 0x03 && dataLen == 9 && data[7] == 0x21 ? onStr : offStr,  // Never seen; just guessing
        data[1] == 0x03 && dataLen == 9 && data[7] == 0x41 ? onStr : offStr,

        data[1] == 0x04 ? onStr : offStr,
        data[1] == 0x05 ? onStr : offStr,
        data[1] == 0x06 ? onStr : offStr
    );

    if (data[1] == 0x01)  // Single turn
    {
        if (data[2] == 0x00 || data[2] == 0x01)
        {
            if (dataLen != 13) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

            // One instruction icon: current in data[4...11]

            GuidanceInstructionIconJson(PSTR("satnav_curr_turn_icon"), data + 4, buf, at, n);
        }
        else if (data[2] == 0x02)
        {
            if (dataLen != 6) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

            // "Fork or exit" instruction
        } // if
    }
    else if (data[1] == 0x03)  // Double turn
    {
        if (dataLen == 9)
        {
            // Two instruction icons: current (fork) in data[6], next (fork) in data[7]
            // Example: 64E E (RA0) 82-03-02-20-02-54-14-14-82
        }
        else if (dataLen == 16)
        {
            // Two instruction icons: current (fork) in data[6], next in data[7...14]

            GuidanceInstructionIconJson(PSTR("satnav_next_turn_icon"), data + 7, buf, at, n);
        }
        else if (dataLen == 23)
        {
            // Two instruction icons: current in data[6...13], next in data[14...21]

            GuidanceInstructionIconJson(PSTR("satnav_curr_turn_icon"), data + 6, buf, at, n);
            GuidanceInstructionIconJson(PSTR("satnav_next_turn_icon"), data + 14, buf, at, n);
        }
        else
        {
            return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
        } // if
    }
    else if (data[1] == 0x04)  // Turn around if possible
    {
        if (dataLen != 3) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

        // No further data in this packet
    } // if
    else if (data[1] == 0x05)  // Follow road
    {
        if (dataLen != 4) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

        // Show one of the five available icons

        const static char jsonFormatter[] PROGMEM =
            ",\n"
            "\"satnav_follow_road_then_turn_right\": \"%s\",\n"
            "\"satnav_follow_road_then_turn_left\": \"%s\",\n"
            "\"satnav_follow_road_until_roundabout\": \"%s\",\n"
            "\"satnav_follow_road_straight_ahead\": \"%s\",\n"
            "\"satnav_follow_road_retrieving_next_instruction\": \"%s\"";

        at += at >= n ? 0 :
            snprintf_P(buf + at, n - at, jsonFormatter,

                data[2] == 0x01 ? onStr : offStr,
                data[2] == 0x02 ? onStr : offStr,
                data[2] == 0x04 ? onStr : offStr,
                data[2] == 0x08 ? onStr : offStr,
                data[2] == 0x10 ? onStr : offStr
            );
    }
    else if (data[1] == 0x06)  // Not on map
    {
        if (dataLen != 4) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

        uint16_t direction = (data[2] * 225 + 1800) % 3600;

        const static char jsonFormatter[] PROGMEM =
            ",\n"
            "\"satnav_not_on_map_follow_heading_as_text\": \"%u.%u\",\n"  // degrees
            "\"satnav_not_on_map_follow_heading\":\n"
            "{\n"
                "\"style\":\n"
                "{\n"
                    "\"transform\": \"rotate(%u.%udeg)\"\n"
                "}\n"
            "}";

        at += at >= n ? 0 :
            snprintf_P(buf + at, n - at, jsonFormatter,

                direction / 10,
                direction % 10,
                direction / 10,
                direction % 10
            );
    } // if

    at += at >= n ? 0 : snprintf_P(buf + at, n - at, PSTR("\n}\n}\n"));

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseSatNavGuidancePkt

// Compose a string for a street name
String ComposeStreetString(const String& records5, const String& records6)
{
    String result = "";

    // First letter 'G' indicates prefix, e.g. "Rue de la" as in "Rue de la PAIX"
    // First letter 'I' indicates building name??
    if (records5[0] == 'G' || records5[0] == 'I')
    {
        result += records5.substring(1);  // Skip the 'G' or 'I'
        if (records5.length() > 1) result += String(" ");
    } // if

    result += records6;

    // First letter 'D' indicates postfix, e.g. "Strasse" as in "ORANIENBURGER Strasse"
    if (records5[0] == 'D')
    {
        if (records5.length() > 1) result += String(" ");
        result += records5.substring(1);  // Skip the 'D'
    } // if

    return result;
} // ComposeStreetString

// Compose a string for a city name plus optional district
String ComposeCityString(const String& records3, const String& records4)
{
    String result = records3;
    if (records4.length() > 0) result += " - " + records4;
    return result;
} // ComposeCityString

VanPacketParseResult_t ParseSatNavReportPkt(TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#6CE
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#6CE

    int dataLen = pkt.DataLen();
    if (dataLen < 3) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

    #define MAX_SATNAV_STRINGS_PER_RECORD 15
    #define MAX_SATNAV_RECORDS 80
    static String records[MAX_SATNAV_RECORDS];

    static int currentRecord = 0;
    static int currentString = 0;

    // String currently being read
    #define MAX_SATNAV_STRING_SIZE 128
    static char buffer[MAX_SATNAV_STRING_SIZE];
    static int offsetInBuffer = 0;

    const uint8_t* data = pkt.Data();

    uint8_t packetFragmentNo = data[0] >> 3 & 0x0F;

    #define INVALID_SATNAV_REPORT (0xFF)
    static uint8_t report = INVALID_SATNAV_REPORT;
    static uint8_t lastFragmentNo = 0;

    int offsetInPacket = 1;
    if (packetFragmentNo == 0)
    {
        // First fragment

        report = data[1];
        lastFragmentNo = 0;

        // Reset
        offsetInPacket = 2;
        offsetInBuffer = 0;

        // Clear all records and reset array indexes. Otherwise, if the last fragment of the previous report
        // was missed, the data will continue to pile up after the previous records.
        // TODO - check if the following can be commented out:
        //for (int i = 0; i < MAX_SATNAV_RECORDS; i++) records[i] = "";

        currentRecord = 0;
        currentString = 0;
    }
    else
    {
        // Missed a previous fragment?
        if (report == INVALID_SATNAV_REPORT) return VAN_PACKET_PARSE_FRAGMENT_MISSED;

        uint8_t expectedFragmentNo = lastFragmentNo + 1;
        if (expectedFragmentNo > 0x0F) expectedFragmentNo = 1;  // Rolling over to 1 instead of 0??

        // Missed the expected fragment?
        if (packetFragmentNo != expectedFragmentNo)
        {
            Serial.printf_P(
                PSTR("%s==> ERROR: satnav report (IDEN 0x6CE) packetFragmentNo = %u ; expectedFragmentNo = %u\n"),
                TimeStamp(),
                packetFragmentNo,
                expectedFragmentNo
            );
            report = INVALID_SATNAV_REPORT;
            return VAN_PACKET_PARSE_FRAGMENT_MISSED;
        } // if

        lastFragmentNo = packetFragmentNo;
    } // if

    while (offsetInPacket < dataLen - 1)
    {
        // New record?
        if (data[offsetInPacket] == 0x01)
        {
            offsetInPacket++;

            if (++currentRecord >= MAX_SATNAV_RECORDS)
            {
                // Warning on Serial output
                Serial.printf_P(PSTR("%s==> WARNING: too many records in satnav report!\n"), TimeStamp());
            } // if

            currentString = 0;
            offsetInBuffer = 0;
        } // if

        int maxLen = dataLen - 1 - offsetInPacket;
        if (offsetInBuffer + maxLen >= MAX_SATNAV_STRING_SIZE)
        {
            maxLen = MAX_SATNAV_STRING_SIZE - offsetInBuffer - 1;
        } // if

        strncpy(buffer + offsetInBuffer, (const char*) data + offsetInPacket, maxLen);
        buffer[offsetInBuffer + maxLen] = 0;

        size_t bufLen = strlen(buffer);
        offsetInPacket += bufLen + 1 - offsetInBuffer;
        if (offsetInPacket >= dataLen)
        {
            offsetInBuffer = bufLen;
            continue;
        } // if

        offsetInBuffer = 0;

        // Calculate which entry of 'records' array to fill
        int i = currentRecord;

        switch(report)
        {
            case SR_CURRENT_STREET:
            case SR_NEXT_STREET:
            case SR_DESTINATION:
            case SR_LAST_DESTINATION:
            case SR_PERSONAL_ADDRESS:
            case SR_PROFESSIONAL_ADDRESS:
            case SR_SERVICE_ADDRESS:
            case SR_ENTER_HOUSE_NUMBER:
            {
                i = currentString;
            } // case
            break;

            case SR_ENTER_CITY:
            case SR_ENTER_STREET:
            case SR_PERSONAL_ADDRESS_LIST:
            case SR_PROFESSIONAL_ADDRESS_LIST:
            case SR_SERVICE_LIST:
            {
                i = currentRecord;
            } // case
            break;

            case SR_SOFTWARE_MODULE_VERSIONS:
            {
                i = currentRecord * 3 + currentString;
            } // case
            break;
        } // switch

        // Better safe than sorry
        if (i >= MAX_SATNAV_RECORDS) continue;

        // Copy the current string buffer into the array of Strings
        records[i] = buffer;

        // Fix a bug in the original MFD: '#' is used to indicate a "soft hyphen"
        records[i].replace("#", "&shy;");

        if (report == SR_PERSONAL_ADDRESS_LIST || report == SR_PROFESSIONAL_ADDRESS_LIST)
        {
            records[i].replace(" ", "&nbsp;");
        } // if

        // Special (last) characters:
        // - 0x80: indicates that the entry cannot be selected because the current navigation disc cannot be
        //   read. This is shown as a "?".
        // - 0x81: indicates that the entry cannot be selected because the current navigation disc is for a
        //   different country/region. This is shown on the original MFD as an circle with a bar "(-)"; here we
        //   use a circle with a cross "(X)".
        records[i].replace("\x80", "?");
        records[i].replace("\x81", "&#x24E7;");

        // Replace special characters by HTML-safe ones, e.g. "\xEB" (ë) by "&euml;"
        AsciiToHtml(records[i]);

        if (++currentString >= MAX_SATNAV_STRINGS_PER_RECORD)
        {
            // Warning on serial output
            Serial.printf_P(PSTR("%s==> WARNING: too many strings in record in satnav report!\n"), TimeStamp());
        } // if
    } // while

    // Not last fragment?
    if ((data[0] & 0x80) == 0x00) return VAN_PACKET_NO_CONTENT;

    // Fragment was missed?
    if (report == INVALID_SATNAV_REPORT) return VAN_PACKET_PARSE_FRAGMENT_MISSED;

    // Last packet in sequence

    // Create an 'easily digestable' report in JSON format

    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"satnav_report\": \"%s\"";

    int at = snprintf_P(buf, n, jsonFormatter, SatNavRequestStr(report));

    bool wasCurrentStreetKnown = isCurrentStreetKnown;

    switch(report)
    {
        case SR_CURRENT_STREET:
        case SR_NEXT_STREET:
        {
            String city = ComposeCityString(records[3], records[4]);  // City (if any) + optional district

            if (records[6].length() == 0)
            {
                // In this case, the original MFD says: "Street not listed". We just show the city.

                if (report == SR_CURRENT_STREET && city.length() > 0)
                {
                    // Bug in original MFD: it seems to not "know" the current street as long as we're
                    // "IN_GUIDANCE_MODE"? TODO - not sure
                    if (satnavStatus2 != SATNAV_STATUS_2_IN_GUIDANCE_MODE) isCurrentStreetKnown = true;

                    satnavCurrentStreet = city;
                } // if

                const static char jsonFormatter[] PROGMEM =
                    ",\n"
                    "\"%s\": \"%s\"";

                at += at >= n ? 0 :
                    snprintf_P(buf + at, n - at, jsonFormatter,
                        report == SR_CURRENT_STREET ? PSTR("satnav_curr_street") : PSTR("satnav_next_street"),
                        city.c_str()
                    );

                break;
            } // if

            String street = ComposeStreetString(records[5], records[6]).c_str();

            if (report == SR_CURRENT_STREET)
            {
                // Bug in original MFD: it seems to not "know" the current street as long as we're
                // "IN_GUIDANCE_MODE"? TODO - not sure
                if (satnavStatus2 != SATNAV_STATUS_2_IN_GUIDANCE_MODE) isCurrentStreetKnown = true;

                satnavCurrentStreet = street + " (" + city + ")";
            } // if

            const static char jsonFormatter[] PROGMEM =
                ",\n"
                "\"%s\": \"%s (%s)\"";

            // Current/next street is in first (and only) record. Copy only city [3], district [4] (if any) and
            // street [5, 6]; skip the other strings.
            at += at >= n ? 0 :
                snprintf_P(buf + at, n - at, jsonFormatter,
                    report == SR_CURRENT_STREET ? PSTR("satnav_curr_street") : PSTR("satnav_next_street"),
                    street.c_str(),
                    city.c_str()
                );
        } // case
        break;

        case SR_DESTINATION:
        case SR_LAST_DESTINATION:
        {
            const static char jsonFormatter[] PROGMEM =
                ",\n"
                "\"%s\": \"%s\",\n"
                "\"%s\": \"%s\",\n"
                "\"%s\": \"%s\",\n"
                "\"%s\": \"%s\",\n"
                "\"%s\": \"%s\"";

            // Address is in first (and only) record. Copy at least only city [3], district [4] (if any), street [5, 6]
            // and house number [7]; skip the other strings.
            at += at >= n ? 0 :
                snprintf_P(buf + at, n - at, jsonFormatter,

                    report == SR_DESTINATION ?
                        PSTR("satnav_current_destination_country") :
                        PSTR("satnav_last_destination_country"),

                    // Country
                    records[1].c_str(),

                    report == SR_DESTINATION ?
                        PSTR("satnav_current_destination_province") :
                        PSTR("satnav_last_destination_province"),

                    // Province
                    records[2].c_str(),

                    report == SR_DESTINATION ?
                        PSTR("satnav_current_destination_city") :
                        PSTR("satnav_last_destination_city"),

                    // City + optional district
                    ComposeCityString(records[3], records[4]).c_str(),

                    report == SR_DESTINATION ?
                        PSTR("satnav_current_destination_street") :
                        PSTR("satnav_last_destination_street"),

                    // Street
                    // Note: if the street is empty: it means "City centre"
                    ComposeStreetString(records[5], records[6]).c_str(),

                    report == SR_DESTINATION ?
                        PSTR("satnav_current_destination_house_number") :
                        PSTR("satnav_last_destination_house_number"),

                    // First string is either "C" or "V"; "C" has GPS coordinates in [7] and [8]; "V" has house number
                    // in [7]. If we see "V", show house number
                    records[0] == "V" && records[7] != "0" ?
                        records[7].c_str() :
                        emptyStr
                );
        } // case
        break;

        case SR_PERSONAL_ADDRESS:
        case SR_PROFESSIONAL_ADDRESS:
        {
            const static char jsonFormatter[] PROGMEM =
                ",\n"
                "\"%s\": \"%s\",\n"
                "\"%s\": \"%s\",\n"
                "\"%s\": \"%s\",\n"
                "\"%s\": \"%s\",\n"
                "\"%s\": \"%s\",\n"
                "\"%s\": \"%s\"";

            // Chosen address is in first (and only) record. Copy at least city [3], district [4] (if any),
            // street [5, 6], house number [7] and entry name [8]; skip the other strings.
            at += at >= n ? 0 :
                snprintf_P(buf + at, n - at, jsonFormatter,

                    report == SR_PERSONAL_ADDRESS ?
                        PSTR("satnav_personal_address_entry") :
                        PSTR("satnav_professional_address_entry"),

                    // Name of the entry
                    records[0] == "C" ? records[9].c_str() : records[8].c_str(),

                    // Address

                    report == SR_PERSONAL_ADDRESS ?
                        PSTR("satnav_personal_address_country") :
                        PSTR("satnav_professional_address_country"),

                    // Country
                    records[1].c_str(),

                    report == SR_PERSONAL_ADDRESS ?
                        PSTR("satnav_personal_address_province") :
                        PSTR("satnav_professional_address_province"),

                    // Province
                    records[2].c_str(),

                    report == SR_PERSONAL_ADDRESS ?
                        PSTR("satnav_personal_address_city") :
                        PSTR("satnav_professional_address_city"),

                    // City + optional district
                    ComposeCityString(records[3], records[4]).c_str(),

                    report == SR_PERSONAL_ADDRESS ?
                        PSTR("satnav_personal_address_street") :
                        PSTR("satnav_professional_address_street"),

                    // Street
                    // Note: if the street is empty: it means "City centre"
                    ComposeStreetString(records[5], records[6]).c_str(),

                    report == SR_PERSONAL_ADDRESS ?
                        PSTR("satnav_personal_address_house_number") :
                        PSTR("satnav_professional_address_house_number"),

                    // First string is either "C" or "V"; "C" has GPS coordinates in [7] and [8]; "V" has house number
                    // in [7]. If we see "V", show house number
                    records[0] == "V" && records[7] != "0" ?
                        records[7].c_str() :
                        emptyStr
                );
        } // case
        break;

        case SR_SERVICE_ADDRESS:
        {
            const static char jsonFormatter[] PROGMEM =
                ",\n"
                "\"satnav_service_address_entry\": \"%s\",\n"
                "\"satnav_service_address_country\": \"%s\",\n"
                "\"satnav_service_address_province\": \"%s\",\n"
                "\"satnav_service_address_city\": \"%s\",\n"
                "\"satnav_service_address_street\": \"%s\",\n"
                "\"satnav_service_address_distance\": \"%s %s\"";

            // Chosen service address is in first (and only) record. Copy at least city [3], district [4]
            // (if any), street [5, 6], entry name [9] and distance [11]; skip the other strings.
            at += at >= n ? 0 :
                snprintf_P(buf + at, n - at, jsonFormatter,

                    // Name of the service address
                    records[9].c_str(),

                    // Service address

                    // Country
                    records[1].c_str(),

                    // Province
                    records[2].c_str(),

                    // City + optional district
                    ComposeCityString(records[3], records[4]).c_str(),

                    // Street
                    ComposeStreetString(records[5], records[6]).c_str(),

                    // Distance to the service address (sat nav reports in metres or in yards)
                    records[11].c_str(),
                    mfdDistanceUnit == MFD_DISTANCE_UNIT_METRIC ? PSTR("m") : PSTR("yd")
                );
        } // case
        break;

        case SR_ENTER_CITY:
        case SR_ENTER_STREET:
        case SR_PERSONAL_ADDRESS_LIST:
        case SR_PROFESSIONAL_ADDRESS_LIST:
        {
            const static char jsonFormatter[] PROGMEM =
                ",\n"
                "\"satnav_list\":\n"
                "[";

            at += at >= n ? 0 :
                snprintf_P(buf + at, n - at, jsonFormatter);

            // Each item in the list is a single string in a separate record
            for (int i = 0; i < currentRecord; i++)
            {
                at += at >= n ? 0 :
                    snprintf_P(buf + at, n - at,
                        PSTR("%s\n\"%s\""),
                        i == 0 ? emptyStr : commaStr,
                        records[i].c_str()
                    );
            } // for

            at += at >= n ? 0 :
                snprintf_P(buf + at, n - at, PSTR("\n]"));
        } // case
        break;

        case SR_ENTER_HOUSE_NUMBER:
        {
            const static char jsonFormatter[] PROGMEM =
                ",\n"
                "\"satnav_house_number_range\": \"From %s to %s\"";

            // Range of "house numbers" is in first (and only) record, the lowest number is in the first string, and
            // highest number is in the second string.
            // Note: "0...0" means: not applicable. MFD will follow through directly to showing the address (without a
            //   house number).
            at += at >= n ? 0 :
                snprintf_P(buf + at, n - at, jsonFormatter,

                    records[0].c_str(),
                    records[1].c_str()
                );
        } // case
        break;

        case SR_SERVICE_LIST:
        {
            const static char jsonFormatter[] PROGMEM =
                ",\n"
                "\"satnav_list\":\n"
                "[";

            at += at >= n ? 0 :
                snprintf_P(buf + at, n - at, jsonFormatter);

            // Each "service" in the list is a single string in a separate record
            for (int i = 0; i < currentRecord; i++)
            {
                at += at >= n ? 0 :
                    snprintf_P(buf + at, n - at,
                        PSTR("%s\n\"%s\""),
                        i == 0 ? emptyStr : commaStr,
                        records[i].c_str()
                    );
            } // for

            at += at >= n ? 0 :
                snprintf_P(buf + at, n - at, PSTR("\n]"));
        } // case
        break;

        case SR_SOFTWARE_MODULE_VERSIONS:
        {
            // To see the module versions on the original MFD:
            // - Press Menu on the infrared remote control
            // - Hold Esc until the menu goes away
            // - Press Left twice
            // - Hold Esc until the debug menu appears ("Supplier DEBUG Menu")

            const static char jsonFormatter[] PROGMEM =
                ",\n"
                "\"satnav_software_modules_list\":\n"
                "[";

            at += at >= n ? 0 :
                snprintf_P(buf + at, n - at, jsonFormatter);

            // Each "module" in the list is a triplet of strings ('module_name', then 'version' and 'date' in a rather
            // free format) in a separate record
            for (int i = 0; i < currentRecord; i++)
            {
                at += at >= n ? 0 :
                    snprintf_P(buf + at, n - at,
                        PSTR("%s\n\"%s - %s - %s\""),
                        i == 0 ? emptyStr : commaStr,
                        records[i * 3].c_str(),
                        records[i * 3 + 1].c_str(),
                        records[i * 3 + 2].c_str()
                    );
            } // for

            at += at >= n ? 0 :
                snprintf_P(buf + at, n - at, PSTR("\n]"));
        } // case
        break;

    } // switch

    if (! wasCurrentStreetKnown && isCurrentStreetKnown)
    {
        UpdateLargeScreenForCurrentStreetKnown();

        at += at >= n ? 0 :
            snprintf_P(buf + at, n - at, PSTR(",\n\"large_screen\": \"%s\""),
                LargeScreenStr()
            );
    } // if

    at += at >= n ? 0 : snprintf_P(buf + at, n - at, PSTR("\n}\n}\n"));

    // Reset
    report = INVALID_SATNAV_REPORT;
    offsetInBuffer = 0;

    // Clear all records and reset array indexes. Otherwise, if the first fragment of the next report
    // is missed, the data will continue to pile up after the current records.
    // TODO - check if the following can be commented out:
    //for (int i = 0; i < MAX_SATNAV_RECORDS; i++) records[i] = "";

    currentRecord = 0;
    currentString = 0;

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseSatNavReportPkt

VanPacketParseResult_t ParseMfdToSatNavPkt(TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#94E
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#94E

    int dataLen = pkt.DataLen();
    if (dataLen != 4 && dataLen != 9 && dataLen != 11) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

    const uint8_t* data = pkt.Data();
    uint8_t request = data[0];
    uint8_t param = data[1];
    uint8_t type = data[2];

    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"mfd_to_satnav_request\": \"%s\",\n"
            "\"mfd_to_satnav_request_type\": \"%s\"";

    int at = snprintf_P(buf, n, jsonFormatter, SatNavRequestStr(request), SatNavRequestTypeStr(type));

    const char* goToScreen =

        // Determine which sat nav screen must be/become visible.
        // An exmpty string ("") indicates: stay in current screen for now, trigger screen change on remote control
        // button press.

        // Combinations:

        // * request == 0x02 (SR_ENTER_CITY),
        //   param == 0x1D || param == 0x0D:
        //   - type = 0 (SRT_REQ_N_ITEMS) (dataLen = 4): request (remaining) list length
        //     -- data[3]: (next) character to narrow down selection with. 0x00 if none.
        //   - type = 1 (SRT_REQ_ITEMS) (dataLen = 9): request list
        //     -- data[5] << 8 | data[6]: offset in list (0-based)
        //     -- data[7] << 8 | data[8]: number of items to retrieve
        //   - type = 2 (SRT_SELECT) (dataLen = 11): select entry
        //     -- data[5] << 8 | data[6]: selected entry (0-based)

        request == SR_ENTER_CITY && (param == 0x1D || param == 0x0D) && type == SRT_REQ_N_ITEMS ? PSTR("satnav_enter_city_characters") :
        request == SR_ENTER_CITY && (param == 0x1D || param == 0x0D) && type == SRT_REQ_ITEMS ? PSTR("satnav_choose_from_list") :
        request == SR_ENTER_CITY && (param == 0x1D || param == 0x0D) && type == SRT_SELECT ? emptyStr :

        // * request == 0x05 (SR_ENTER_STREET),
        //   param == 0x1D || param == 0x0D:
        //   - type = 0 (SRT_REQ_N_ITEMS) (dataLen = 4): request (remaining) list length
        //     -- data[3]: (next) character to narrow down selection with. 0x00 if none.
        //   - type = 1 (SRT_REQ_ITEMS) (dataLen = 9): request list
        //     -- data[5] << 8 | data[6]: offset in list (0-based)
        //     -- data[7] << 8 | data[8]: number of items to retrieve
        //   - type = 2 (SRT_SELECT) (dataLen = 11): select entry
        //     -- data[5] << 8 | data[6]: selected entry (0-based)
        //   param == 0x1D: (TODO also || param == 0x0D ?)
        //   - type = 3 (SRT_SELECT_CITY_CENTER) (dataLen = 9): select city center
        //     -- data[5] << 8 | data[6]: offset in list (always 0)
        //     -- data[7] << 8 | data[8]: number of items to retrieve (always 1)

        request == SR_ENTER_STREET && (param == 0x1D || param == 0x0D) && type == SRT_REQ_N_ITEMS ? PSTR("satnav_enter_street_characters") :
        request == SR_ENTER_STREET && (param == 0x1D || param == 0x0D) && type == SRT_REQ_ITEMS ? PSTR("satnav_choose_from_list") :
        request == SR_ENTER_STREET && (param == 0x1D || param == 0x0D) && type == SRT_SELECT ? emptyStr :
        request == SR_ENTER_STREET && param == 0x1D && type == SRT_SELECT_CITY_CENTER ? PSTR("satnav_show_current_destination") :

        // * request == 0x06 (SR_ENTER_HOUSE_NUMBER),
        //   param == 0x1D:
        //   - type = 1 (SRT_REQ_ITEMS) (dataLen = 9): request range of house numbers
        //     -- data[7] << 8 | data[8]: always 1
        //   - type = 2 (SRT_SELECT) (dataLen = 11): enter house number
        //     -- data[5] << 8 | data[6]: entered house number. 0 if not applicable.

        request == SR_ENTER_HOUSE_NUMBER && param == 0x1D && type == SRT_REQ_ITEMS ? PSTR("satnav_current_destination_house_number") :
        request == SR_ENTER_HOUSE_NUMBER && param == 0x1D && type == SRT_SELECT ? PSTR("satnav_show_current_destination") :

        // * request == 0x08 (SR_SERVICE_LIST),
        //   param == 0x0D:
        //   - type = 0 (SRT_REQ_N_ITEMS) (dataLen = 4): request list length. Satnav will respond with 38 and no
        //              letters or number to choose from.
        //   - type = 2 (SRT_SELECT) (dataLen = 11): select entry from list
        //     -- data[5] << 8 | data[6]: selected entry (0-based)

        request == SR_SERVICE_LIST && param == 0x0D && type == SRT_REQ_N_ITEMS ? emptyStr :
        request == SR_SERVICE_LIST && param == 0x0D && type == SRT_SELECT ? emptyStr :

        // * request == 0x08 (SR_SERVICE_LIST),
        //   param == 0xFF:
        //   - type = 0 (SRT_REQ_N_ITEMS) (dataLen = 4): present nag screen. Satnav response is SR_SERVICE_LIST
        //              with list_size=38, but the MFD ignores that.
        //   - type = 1 (SRT_REQ_ITEMS) (dataLen = 9): request list
        //     -- data[5] << 8 | data[6]: offset in list (always 0)
        //     -- data[7] << 8 | data[8]: number of items to retrieve (always 38)

        request == SR_SERVICE_LIST && param == 0xFF && type == SRT_REQ_N_ITEMS ? emptyStr :
        request == SR_SERVICE_LIST && param == 0xFF && type == SRT_REQ_ITEMS ? emptyStr :

        // * request == 0x09 (SR_SERVICE_ADDRESS),
        //   param == 0x0D:
        //   - type = 0 (SRT_REQ_N_ITEMS) (dataLen = 4): request list length
        //   - type = 1 (SRT_REQ_ITEMS) (dataLen = 9): request list
        //     -- data[5] << 8 | data[6]: offset in list (always 0)
        //     -- data[7] << 8 | data[8]: number of items to retrieve (always 1: MFD browses address by address)

        request == SR_SERVICE_ADDRESS && param == 0x0D && type == SRT_REQ_N_ITEMS ? PSTR("satnav_choose_from_list") :
        request == SR_SERVICE_ADDRESS && param == 0x0D && type == SRT_REQ_ITEMS ? PSTR("satnav_show_service_address") :

        // * request == 0x09 (SR_SERVICE_ADDRESS),
        //   param == 0x0E:
        //   - type = 2 (SRT_SELECT) (dataLen = 11): select entry from list
        //     -- data[5] << 8 | data[6]: selected entry (0-based)

        request == SR_SERVICE_ADDRESS && param == 0x0E && type == SRT_SELECT ? emptyStr : // PSTR("satnav_guidance") :

        // * request == 0x0B (SR_ARCHIVE_IN_DIRECTORY),
        //   param == 0xFF:
        //   - type = 0 (SRT_REQ_N_ITEMS) (dataLen = 4): request list of available characters
        //     -- data[3]: character entered by user. 0x00 if none.

        request == SR_ARCHIVE_IN_DIRECTORY && param == 0xFF && type == SRT_REQ_N_ITEMS ? emptyStr :

        // * request == 0x0B (SR_ARCHIVE_IN_DIRECTORY),
        //   param == 0x0D:
        //   - type = 2 (SRT_SELECT) (dataLen = 11): confirm entered name
        //     -- no further data

        request == SR_ARCHIVE_IN_DIRECTORY && param == 0x0D && type == SRT_SELECT ? emptyStr :

        // * request == 0x0E (SR_LAST_DESTINATION),
        //   param == 0x0D:
        //   - type = 2 (SRT_SELECT) (dataLen = 11):
        //     -- no further data

        request == SR_LAST_DESTINATION && param == 0x0D && type == SRT_SELECT ? emptyStr :

        // * request == 0x0E (SR_LAST_DESTINATION),
        //   param == 0xFF:
        //   - type = 1 (SRT_REQ_ITEMS) (dataLen = 9):
        //     -- data[5] << 8 | data[6]: offset in list (always 0)
        //     -- data[7] << 8 | data[8]: number of items to retrieve (always 1)

        request == SR_LAST_DESTINATION && param == 0xFF && type == SRT_REQ_ITEMS ? emptyStr : // PSTR("satnav_show_last_destination") :

        // * request == 0x0F (SR_NEXT_STREET),
        //   param == 0xFF:
        //   - type = 1 (SRT_REQ_ITEMS) (dataLen = 9):
        //     -- data[5] << 8 | data[6]: offset in list (always 0)
        //     -- data[7] << 8 | data[8]: number of items to retrieve (always 1)

        request == SR_NEXT_STREET && param == 0xFF && type == SRT_REQ_ITEMS ? emptyStr : // PSTR("satnav_guidance") :

        // * request == 0x10 (SR_CURRENT_STREET),
        //   param == 0x0D:
        //   - type = 2 (SRT_SELECT) (dataLen = 11): select current location for e.g. service addresses
        //     -- no further data

        request == SR_CURRENT_STREET && param == 0x0D && type == SRT_SELECT ? emptyStr :

        // * request == 0x10 (SR_CURRENT_STREET),
        //   param == 0xFF:
        //   - type = 1 (SRT_REQ_ITEMS) (dataLen = 9): get current location during sat nav guidance
        //     -- data[5] << 8 | data[6]: offset in list (always 0)
        //     -- data[7] << 8 | data[8]: number of items to retrieve (always 1)

        // Don't switch screen; the screen must either stay "satnav_guidance" in guidance mode, or
        // "satnav_current_location" if the current screen is "clock". Complicated, so let the JavaScript logic
        // handle this.
        request == SR_CURRENT_STREET && param == 0xFF && type == SRT_REQ_ITEMS ? emptyStr :

        // * request == 0x11 (SR_PERSONAL_ADDRESS),
        //   param == 0x0E:
        //   - type = 2 (SRT_SELECT) (dataLen = 11): select entry from list
        //     -- data[5] << 8 | data[6]: selected entry (0-based)

        request == SR_PERSONAL_ADDRESS && param == 0x0E && type == SRT_SELECT ? emptyStr : // PSTR("satnav_guidance") :

        // * request == 0x11 (SR_PERSONAL_ADDRESS),
        //   param == 0xFF:
        //   - type = 1 (SRT_REQ_ITEMS) (dataLen = 9): get entry
        //     -- data[5] << 8 | data[6]: selected entry (0-based)

        request == SR_PERSONAL_ADDRESS && param == 0xFF && type == SRT_REQ_ITEMS ? PSTR("satnav_show_personal_address") :

        // * request == 0x12 (SR_PROFESSIONAL_ADDRESS),
        //   param == 0x0E:
        //   - type = 2 (SRT_SELECT) (dataLen = 11): select entry from list
        //     -- data[5] << 8 | data[6]: selected entry (0-based)

        request == SR_PROFESSIONAL_ADDRESS && param == 0x0E && type == SRT_SELECT ? emptyStr : // PSTR("satnav_guidance") :

        // * request == 0x12 (SR_PROFESSIONAL_ADDRESS),
        //   param == 0xFF:
        //   - type = 1 (SRT_REQ_ITEMS) (dataLen = 9): get entry
        //     -- data[5] << 8 | data[6]: selected entry (0-based)

        request == SR_PROFESSIONAL_ADDRESS && param == 0xFF && type == SRT_REQ_ITEMS ? PSTR("satnav_show_professional_address") :

        // * request == 0x1B (SR_PERSONAL_ADDRESS_LIST),
        //   param == 0xFF:
        //   - type = 0 (SRT_REQ_N_ITEMS) (dataLen = 4): request list length
        //   - type = 1 (SRT_REQ_ITEMS) (dataLen = 9): request list
        //     -- data[5] << 8 | data[6]: offset in list
        //     -- data[7] << 8 | data[8]: number of items to retrieve

        request == SR_PERSONAL_ADDRESS_LIST && param == 0xFF && type == SRT_REQ_N_ITEMS ? emptyStr : // PSTR("satnav_choose_from_list") :
        request == SR_PERSONAL_ADDRESS_LIST && param == 0xFF && type == SRT_REQ_ITEMS ? PSTR("satnav_choose_from_list") :

        // * request == 0x1C (SR_PROFESSIONAL_ADDRESS_LIST),
        //   param == 0xFF:
        //   - type = 0 (SRT_REQ_N_ITEMS) (dataLen = 4): request list length
        //   - type = 1 (SRT_REQ_ITEMS) (dataLen = 9): request list
        //     -- data[5] << 8 | data[6]: offset in list
        //     -- data[7] << 8 | data[8]: number of items to retrieve

        request == SR_PROFESSIONAL_ADDRESS_LIST && param == 0xFF && type == SRT_REQ_N_ITEMS ? emptyStr : // PSTR("satnav_choose_from_list") :
        request == SR_PROFESSIONAL_ADDRESS_LIST && param == 0xFF && type == SRT_REQ_ITEMS ? PSTR("satnav_choose_from_list") :

        // * request == 0x1D (SR_DESTINATION),
        //   param == 0x0E:
        //   - type = 2 (SRT_SELECT) (dataLen = 11): select fastest route ?
        //     -- no further data

        request == SR_DESTINATION && param == 0x0E && type == SRT_SELECT ? emptyStr : // PSTR("satnav_guidance") :

        // * request == 0x1D (SR_DESTINATION),
        //   param == 0xFF:
        //   - type = 1 (SRT_REQ_ITEMS) (dataLen = 9): get current destination. Satnav replies (IDEN 0x6CE) with the last
        //     destination, a city center with GPS coordinate (if no street has been entered yet), or a
        //     full address
        //     -- data[5] << 8 | data[6]: offset in list (always 0)
        //     -- data[7] << 8 | data[8]: number of items to retrieve (always 1)

        request == SR_DESTINATION && param == 0xFF && type == SRT_REQ_ITEMS ? emptyStr :

        emptyStr;

    if (strlen_P(goToScreen) > 0)
    {
        at += at >= n ? 0 :
            snprintf_P(buf + at, n - at, PSTR
                (
                    ",\n"
                    "\"go_to_screen\": \"%s\""
                ),

                goToScreen
            );
    } // if

    if (data[3] != 0x00)
    {
        char buffer[2];
        sprintf_P(buffer, PSTR("%c"), data[3]);

        at += at >= n ? 0 :
            snprintf_P(buf + at, n - at, PSTR
                (
                    ",\n"
                    "\"mfd_to_satnav_enter_character\": \"%s\""
                ),

                (data[3] >= 'A' && data[3] <= 'Z') ||
                (data[3] >= '0' && data[3] <= '9') ||
                data[3] == '.' || data[3] == '\'' ?
                    buffer :
                data[3] == ' ' ? "_" : // Space
                data[3] == 0x01 ? "Esc" :
                "?"
            );
    } // if

    if (dataLen >= 9)
    {
        uint16_t selectionOrOffset = (uint16_t)data[5] << 8 | data[6];
        uint16_t length = (uint16_t)data[7] << 8 | data[8];

        //if (selectionOrOffset > 0 && length > 0)
        if (length > 0)
        {
            at += at >= n ? 0 :
                snprintf_P(buf + at, n - at, PSTR
                    (
                        ",\n"
                        "\"mfd_to_satnav_offset\": \"%u\",\n"
                        "\"mfd_to_satnav_length\": \"%u\""
                    ),
                    selectionOrOffset,
                    length
                );
        }
        //else if (selectionOrOffset > 0)
        else
        {
            at += at >= n ? 0 :
                snprintf_P(buf + at, n - at, PSTR
                    (
                        ",\n"
                        "\"mfd_to_satnav_selection\": \"%u\""
                    ),
                    selectionOrOffset
                );
        } // if
    } // if

    at += at >= n ? 0 : snprintf_P(buf + at, n - at, PSTR("\n}\n}\n"));

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseMfdToSatNavPkt

VanPacketParseResult_t ParseSatNavToMfdPkt(TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#74E
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#74E

    const uint8_t* data = pkt.Data();
    uint8_t request = data[1];
    uint16_t listSize = data[4] << 8 | data[5];

    // Sometimes there is a second list size. The first list size (bytes 4 and 5) is the number of items *containing*
    // the selected characters, the second list size (bytes 11 and 12) is the number of items *starting* with the
    // selected characters.
    uint16_t list2Size = data[11] << 8 | data[12];

    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"satnav_to_mfd_response\": \"%s\"";

    int at = snprintf_P(buf, n, jsonFormatter, SatNavRequestStr(request));

    if (listSize != 0xFFFF)
    {
        at += at >= n ? 0 : snprintf_P(buf + at, n - at, PSTR(",\n\"satnav_to_mfd_list_size\": \"%u\""), listSize);
    } // if

    // data[10] is some "flags" byte. Values seen:
    // - 0x40 : No second list
    // - 0x41 : Second list
    // - 0x48 : No second list
    // - 0x50 : No second list
    // - 0x61 : Second list
    // - 0xF1 : Second list with same length as first list
    // if ((data[10] == 0x41) || (data[10] == 0x61) && list2Size >= 0)

    at += at >= n ? 0 : snprintf_P(buf + at, n - at, PSTR(",\n\"satnav_to_mfd_list_2_size\": \"%u\""), list2Size);

    at += at >= n ? 0 : snprintf_P(buf + at, n - at, PSTR(",\n\"satnav_to_mfd_show_characters\": \""));

    // Store number of services; it must be reported to the WebSocket client upon connection, so that it can
    // enable the "Select a service" menu item
    if (request == SR_SERVICE_LIST && listSize != 0 && listSize != 0xFFFF) satnavServiceListSize = listSize;

    // TODO - handle SR_ARCHIVE_IN_DIRECTORY

    // Available letters are bit-coded in bytes 17...20. Print the letter if it is available.
    for (int byte = 0; byte <= 3; byte++)
    {
        for (int bit = 0; bit < (byte == 3 ? 2 : 8); bit++)
        {
            if (data[byte + 17] >> bit & 0x01)
            {
                at += at >= n ? 0 :
                    snprintf_P(buf + at, n - at, PSTR("%c"), 65 + 8 * byte + bit);
            } // if
        } // for
    } // for

    if (data[21] >> 6 & 0x01)
    {
        // Special character: single quote (')
        at += at >= n ? 0 : snprintf_P(buf + at, n - at, PSTR("\'"));
    } // if

    // Available numbers are bit-coded in bytes 20...21, starting with '0' at bit 2 of byte 20, ending
    // with '9' at bit 3 of byte 21. Print the number if it is available.
    for (int byte = 0; byte <= 1; byte++)
    {
        for (int bit = (byte == 0 ? 2 : 0); bit < (byte == 1 ? 4 : 8); bit++)
        {
            if (data[byte + 20] >> bit & 0x01)
            {
                at += at >= n ? 0 :
                    snprintf_P(buf + at, n - at, PSTR("%c"), 48 + 8 * byte + bit - 2);
            } // if
        } // for
    } // for

    if (data[22] >> 1 & 0x01)
    {
        // <Space>, will be shown as '_'
        at += at >= n ? 0 : snprintf_P(buf + at, n - at, PSTR("_"));
    } // if

    if (data[22] >> 3 & 0x01)
    {
        // Special character: dot (.)
        at += at >= n ? 0 : snprintf_P(buf + at, n - at, PSTR("."));
    } // if

    at += at >= n ? 0 : snprintf_P(buf + at, n - at, PSTR("\"\n}\n}\n"));

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseSatNavToMfdPkt

VanPacketParseResult_t ParseSatNavDownloading(TVanPacketRxDesc&, char* buf, const int n)
{
    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"satnav_downloading\": \"%" PRIu32 "\"\n"
        "}\n"
    "}\n";

    int at = snprintf_P(buf, n, jsonFormatter, ++satnavDownloadProgress);

    satnavServiceListSize = -1;
    satnavDisclaimerAccepted = false;  // User will need to accept again

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseSatNavDownloading

VanPacketParseResult_t ParseWheelSpeedPkt(TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#744

    const uint8_t* data = pkt.Data();

    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"wheel_speed_rear_right\": \"%s\",\n"
            "\"wheel_speed_rear_left\": \"%s\",\n"
            "\"wheel_pulses_rear_right\": \"%u\",\n"
            "\"wheel_pulses_rear_left\": \"%u\"\n"
        "}\n"
    "}\n";

    char floatBuf[2][MAX_FLOAT_SIZE];

    int at = snprintf_P(buf, n, jsonFormatter,
        ToFloatStr(floatBuf[0], ((uint16_t)data[0] << 8 | data[1]) / 100.0, 2),
        ToFloatStr(floatBuf[1], ((uint16_t)data[2] << 8 | data[3]) / 100.0, 2),
        (uint16_t)data[4] << 8 | data[5],
        (uint16_t)data[6] << 8 | data[7]
    );

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseWheelSpeedPkt

VanPacketParseResult_t ParseOdometerPkt(TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#8FC
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#8FC

    const uint8_t* data = pkt.Data();

    float odometer = ((uint32_t)data[1] << 16 | (uint32_t)data[2] << 8 | data[3]) / 10.0;

    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"odometer_2\": \"%s\"\n"
        "}\n"
    "}\n";

    char floatBuf[MAX_FLOAT_SIZE];

    int at = snprintf_P(buf, n, jsonFormatter,
        mfdDistanceUnit == MFD_DISTANCE_UNIT_METRIC ?
            ToFloatStr(floatBuf, odometer, 1) :
            ToFloatStr(floatBuf, ToMiles(odometer), 1)
    );

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseOdometerPkt

VanPacketParseResult_t ParseCom2000Pkt(TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#450

    const uint8_t* data = pkt.Data();

    // TODO - replace event "display" by "button_press"; JavaScript on served website could react by changing to
    // different screen or displaying popup
    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"com2000_light_switch_auto\": \"%s\",\n"
            "\"com2000_light_switch_fog_light_forward\": \"%s\",\n"
            "\"com2000_light_switch_fog_light_backward\": \"%s\",\n"
            "\"com2000_light_switch_signal_beam\": \"%s\",\n"
            "\"com2000_light_switch_full_beam\": \"%s\",\n"
            "\"com2000_light_switch_all_off\": \"%s\",\n"
            "\"com2000_light_switch_side_lights\": \"%s\",\n"
            "\"com2000_light_switch_low_beam\": \"%s\",\n"
            "\"com2000_right_stalk_button_trip_computer\": \"%s\",\n"
            "\"com2000_right_stalk_rear_window_wash\": \"%s\",\n"
            "\"com2000_right_stalk_rear_window_wiper\": \"%s\",\n"
            "\"com2000_right_stalk_windscreen_wash\": \"%s\",\n"
            "\"com2000_right_stalk_windscreen_wipe_once\": \"%s\",\n"
            "\"com2000_right_stalk_windscreen_wipe_auto\": \"%s\",\n"
            "\"com2000_right_stalk_windscreen_wipe_normal\": \"%s\",\n"
            "\"com2000_right_stalk_windscreen_wipe_fast\": \"%s\",\n"
            "\"com2000_turn_signal_left\": \"%s\",\n"
            "\"com2000_turn_signal_right\": \"%s\",\n"
            "\"com2000_head_unit_stalk_button_src\": \"%s\",\n"
            "\"com2000_head_unit_stalk_button_volume_up\": \"%s\",\n"
            "\"com2000_head_unit_stalk_button_volume_down\": \"%s\",\n"
            "\"com2000_head_unit_stalk_button_seek_backward\": \"%s\",\n"
            "\"com2000_head_unit_stalk_button_seek_forward\": \"%s\",\n"
            "\"com2000_head_unit_stalk_wheel_pos\": \"%d\"\n"
        "}\n"
    "}\n";

    int at = snprintf_P(buf, n, jsonFormatter,

        data[1] & 0x01 ? onStr : offStr,
        data[1] & 0x02 ? onStr : offStr,
        data[1] & 0x04 ? onStr : offStr,
        data[1] & 0x08 ? onStr : offStr,
        data[1] & 0x10 ? onStr : offStr,
        data[1] & 0x20 ? onStr : offStr,
        data[1] & 0x40 ? onStr : offStr,
        data[1] & 0x80 ? onStr : offStr,

        data[2] & 0x01 ? onStr : offStr,
        data[2] & 0x02 ? onStr : offStr,
        data[2] & 0x04 ? onStr : offStr,
        data[2] & 0x08 ? onStr : offStr,
        data[2] & 0x10 ? onStr : offStr,
        data[2] & 0x20 ? onStr : offStr,
        data[2] & 0x40 ? onStr : offStr,
        data[2] & 0x80 ? onStr : offStr,

        data[3] & 0x40 ? onStr : offStr,
        data[3] & 0x80 ? onStr : offStr,

        data[5] & 0x02 ? onStr : offStr,
        data[5] & 0x04 ? onStr : offStr,
        data[5] & 0x08 ? onStr : offStr,
        data[5] & 0x40 ? onStr : offStr,
        data[5] & 0x80 ? onStr : offStr,

        (int8_t)data[6]
    );

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseCom2000Pkt

VanPacketParseResult_t ParseCdChangerCmdPkt(TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#8EC

    const uint8_t* data = pkt.Data();
    uint16_t cdcCommand = (uint16_t)data[0] << 8 | data[1];

    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"cd_changer_command\": \"%s\"\n"
        "}\n"
    "}\n";

    int at = snprintf_P(buf, n, jsonFormatter,

        cdcCommand == 0x1101 ? PSTR("POWER_OFF") :
        cdcCommand == 0x2101 ? PSTR("POWER_OFF") :
        cdcCommand == 0x1181 ? PSTR("PAUSE") :
        cdcCommand == 0x1183 ? PSTR("PLAY") :
        cdcCommand == 0x31FE ? PSTR("PREVIOUS_TRACK") :
        cdcCommand == 0x31FF ? PSTR("NEXT_TRACK") :
        cdcCommand == 0x4101 ? PSTR("CD_1") :
        cdcCommand == 0x4102 ? PSTR("CD_2") :
        cdcCommand == 0x4103 ? PSTR("CD_3") :
        cdcCommand == 0x4104 ? PSTR("CD_4") :
        cdcCommand == 0x4105 ? PSTR("CD_5") :
        cdcCommand == 0x4106 ? PSTR("CD_6") :
        cdcCommand == 0x41FE ? PSTR("PREVIOUS_CD") :
        cdcCommand == 0x41FF ? PSTR("NEXT_CD") :
        ToHexStr(cdcCommand)
    );

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseCdChangerCmdPkt

VanPacketParseResult_t ParseMfdToHeadUnitPkt(TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#8D4
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#8D4

    int dataLen = pkt.DataLen();
    const uint8_t* data = pkt.Data();
    int at = 0;

    // Maybe this is in fact "Head unit to MFD"??

    if (data[0] == 0x11)
    {
        if (dataLen != 2) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

        const static char jsonFormatter[] PROGMEM =
        "{\n"
            "\"event\": \"display\",\n"
            "\"data\":\n"
            "{\n"
                "\"head_unit_update_audio_bits_mute\": \"%s\",\n"
                "\"head_unit_update_audio_bits_auto_volume\": \"%s\",\n"
                "\"head_unit_update_audio_bits_loudness\": \"%s\",\n"
                "\"head_unit_update_audio_bits_audio_menu\": \"%s\",\n"
                "\"head_unit_update_audio_bits_power\": \"%s\",\n"
                "\"head_unit_update_audio_bits_contact_key\": \"%s\"\n"
            "}\n"
        "}\n";

        at = snprintf_P(buf, n, jsonFormatter,
            data[1] & 0x01 ? onStr : offStr,
            data[1] & 0x02 ? onStr : offStr,
            data[1] & 0x10 ? onStr : offStr,

            // Bug: if CD changer is playing, this one is always "OPEN"...
            data[1] & 0x20 ? openStr : closedStr,

            data[1] & 0x40 ? onStr : offStr,
            data[1] & 0x80 ? onStr : offStr
        );
    }
    else if (data[0] == 0x12)
    {
        if (dataLen != 2 && dataLen != 11) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

        const static char jsonFormatter[] PROGMEM =
        "{\n"
            "\"event\": \"display\",\n"
            "\"data\":\n"
            "{\n"
                "\"head_unit_update_switch_to\": \"%s\"";

        at = snprintf_P(buf, n, jsonFormatter,
            data[1] == 0x01 ? PSTR("TUNER") :

            data[1] == 0x02 ?
                seenTapePresence ? PSTR("TAPE") :
                seenCdPresence ? PSTR("CD") :
                PSTR("INTERNAL_CD_OR_TAPE") :

            data[1] == 0x03 ? PSTR("CD_CHANGER") :

            // This is the "default" mode for the head unit, to sit there and listen to the navigation
            // audio. The navigation audio volume is also always set (usually a lot higher than the radio)
            // whenever this source is chosen.
            data[1] == 0x05 ? PSTR("NAVIGATION") :

            ToHexStr(data[1])
        );

        if (dataLen == 11)
        {
            const static char jsonFormatter2[] PROGMEM = ",\n"
                "\"head_unit_update_power\": \"%s\",\n"
                "\"head_unit_update_source\": \"%s\",\n"
                "\"head_unit_update_volume_1\": \"%u%s\",\n"
                "\"head_unit_update_balance\": \"%d%s\",\n"
                "\"head_unit_update_fader\": \"%d%s\",\n"
                "\"head_unit_update_bass\": \"%d%s\",\n"
                "\"head_unit_update_treble\": \"%d%s\"";

            at += at >= n ? 0 : snprintf_P(buf + at, n - at, jsonFormatter2,

                data[2] & 0x01 ? onStr : offStr,

                (data[4] & 0x0F) == 0x00 ? noneStr :  // source
                (data[4] & 0x0F) == 0x01 ? PSTR("TUNER") :

                (data[4] & 0x0F) == 0x02 ?
                    seenTapePresence ? PSTR("TAPE") :
                    seenCdPresence ? PSTR("CD") :
                    PSTR("INTERNAL_CD_OR_TAPE") :

                (data[4] & 0x0F) == 0x03 ? PSTR("CD_CHANGER") :

                // This is the "default" mode for the head unit, to sit there and listen to the navigation
                // audio. The navigation audio volume is also always set (usually a lot higher than the radio)
                // whenever this source is chosen.
                (data[4] & 0x0F) == 0x05 ? PSTR("NAVIGATION") :

                ToHexStr((uint8_t)(data[4] & 0x0F)),

                data[5] & 0x7F,
                data[5] & 0x80 ? updatedStr : emptyStr,
                (int8_t)(0x3F) - (data[6] & 0x7F),
                data[6] & 0x80 ? updatedStr : emptyStr,
                (int8_t)(0x3F) - (data[7] & 0x7F),
                data[7] & 0x80 ? updatedStr : emptyStr,
                (int8_t)(data[8] & 0x7F) - 0x3F,
                data[8] & 0x80 ? updatedStr : emptyStr,
                (int8_t)(data[9] & 0x7F) - 0x3F,
                data[9] & 0x80 ? updatedStr : emptyStr
            );
        } // if

        at += at >= n ? 0 : snprintf_P(buf + at, n - at, PSTR("\n}\n}\n"));
    }
    else if (data[0] == 0x13)
    {
        if (dataLen != 2) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

        const static char jsonFormatter[] PROGMEM =
        "{\n"
            "\"event\": \"display\",\n"
            "\"data\":\n"
            "{\n"
                "\"head_unit_update_volume_2\": \"%u(%s%s)\"\n"
            "}\n"
        "}\n";

        at = snprintf_P(buf, n, jsonFormatter,
            data[1] & 0x1F,
            data[1] & 0x40 ? PSTR("relative: ") : PSTR("absolute"),
            data[1] & 0x40 ?
                data[1] & 0x20 ? PSTR("decrease") : PSTR("increase") :
                emptyStr
        );
    }
    else if (data[0] == 0x14)
    {
        // Seen when the audio popup is on the MFD and a level is changed, and when the audio popup on
        // the MFD disappears.

        // Examples:
        // Raw: #5848 ( 3/15) 10 0E 8D4 WA0 14-BF-3F-43-43-51-D6 ACK OK 51D6 CRC_OK
        // Raw: #6031 (11/15) 10 0E 8D4 WA0 14-BF-3F-45-43-60-84 ACK OK 6084 CRC_OK
        // Raw: #8926 (11/15) 10 0E 8D4 WA0 14-BF-3F-46-43-F7-B0 ACK OK F7B0 CRC_OK

        if (dataLen != 5) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

        // TODO - bit 7 of data[1] is always 1 ?

        const static char jsonFormatter[] PROGMEM =
        "{\n"
            "\"event\": \"display\",\n"
            "\"data\":\n"
            "{\n"
                "\"head_unit_update_audio_levels_balance\": \"%d\",\n"
                "\"head_unit_update_audio_levels_fader\": \"%d\",\n"
                "\"head_unit_update_audio_levels_bass\": \"%d\",\n"
                "\"head_unit_update_audio_levels_treble\": \"%d\"\n"
            "}\n"
        "}\n";

        at = snprintf_P(buf, n, jsonFormatter,
            (int8_t)(0x3F) - (data[1] & 0x7F),
            (int8_t)(0x3F) - data[2],
            (int8_t)data[3] - 0x3F,
            (int8_t)data[4] - 0x3F
        );
    }
    else if (data[0] == 0x27)
    {
        if (dataLen != 2) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

        const static char jsonFormatter[] PROGMEM =
        "{\n"
            "\"event\": \"display\",\n"
            "\"data\":\n"
            "{\n"
                "\"head_unit_preset_request_band\": \"%s\",\n"
                "\"head_unit_preset_request_memory\": \"%u\"\n"
            "}\n"
        "}\n";

        at = snprintf_P(buf, n, jsonFormatter,
            TunerBandStr(data[1] >> 4 & 0x07),
            data[1] & 0x0F
        );
    }
    else if (data[0] == 0x61)
    {
        if (dataLen != 4) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

        const static char jsonFormatter[] PROGMEM =
        "{\n"
            "\"event\": \"display\",\n"
            "\"data\":\n"
            "{\n"
                "\"head_unit_cd_request\": \"%s\"\n"
            "}\n"
        "}\n";

        at = snprintf_P(buf, n, jsonFormatter,
            data[1] == 0x02 ? PSTR("PAUSE") :
            data[1] == 0x03 ? PSTR("PLAY") :
            data[3] == 0xFF ? PSTR("NEXT") :
            data[3] == 0xFE ? PSTR("PREVIOUS") :
            ToHexStr(data[3])
        );
    }
    else if (data[0] == 0xD1)
    {
        if (dataLen != 1) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

        const static char jsonFormatter[] PROGMEM =
        "{\n"
            "\"event\": \"display\",\n"
            "\"data\":\n"
            "{\n"
                "\"head_unit_tuner_info_request\": \"REQUEST\"\n"
            "}\n"
        "}\n";

        at = snprintf_P(buf, n, jsonFormatter);
    }
    else if (data[0] == 0xD2)
    {
        if (dataLen != 1) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

        const static char jsonFormatter[] PROGMEM =
        "{\n"
            "\"event\": \"display\",\n"
            "\"data\":\n"
            "{\n"
                "\"head_unit_tape_info_request\": \"REQUEST\"\n"
            "}\n"
        "}\n";

        at = snprintf_P(buf, n, jsonFormatter);
    }
    else if (data[0] == 0xD6)
    {
        if (dataLen != 1) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

        const static char jsonFormatter[] PROGMEM =
        "{\n"
            "\"event\": \"display\",\n"
            "\"data\":\n"
            "{\n"
                "\"head_unit_cd_track_info_request\": \"REQUEST\"\n"
            "}\n"
        "}\n";

        at = snprintf_P(buf, n, jsonFormatter);
    }
    else
    {
        return VAN_PACKET_PARSE_TO_BE_DECODED;
    } // if

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseMfdToHeadUnitPkt

// Equipment status data, e.g. presence of sat nav and other peripherals.
// Some data is not regularly sent over the VAN bus. If a WebSocket client connects, we want to give a direct
// update of this data as received thus far, instead of having to wait for this data to appear on the bus.
const char* EquipmentStatusDataToJson(char* buf, const int n)
{
    InitSmallScreen();
    InitSatnavGuidancePreference();

    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"contact_key_position\": \"%s\",\n"
            "\"door_open\": \"%s\",\n"
            "\"lights\": \"%s\",\n"
            "\"small_screen\": \"%s\",\n"
            "\"large_screen\": \"%s\",\n"
            "\"trip_computer_screen_tab\": \"%s\",\n"
            "\"cd_changer_cartridge_present\": \"%s\",\n"
            "\"satnav_equipment_present\": \"%s\",\n"
            "\"satnav_disc_recognized\": \"%s\",\n"
            "\"satnav_initialized\": \"%s\",\n"
            "\"satnav_guidance_preference\": \"%s\"";

    int at = snprintf_P(buf, n, jsonFormatter,
        ContactKeyPositionStr(contactKeyPosition),
        doorOpen ? yesStr : noStr,
        lightsStr,
        SmallScreenStr(),  // Small screen (left hand side of the display) to start with
        LargeScreenStr(),  // Large screen (right hand side of the display) to start with
        TripComputerStr(),
        cdChangerCartridgePresent ? yesStr : noStr,
        satnavEquipmentDetected ? yesStr : noStr,
        satnavDiscRecognized ? yesStr : noStr,
        satnavInitialized ? yesStr : noStr,
        SatNavGuidancePreferenceStr(satnavGuidancePreference)
    );

    if (satnavDisclaimerAccepted)
    {
        at += at >= n ? 0 : snprintf(buf + at, n - at, PSTR(",\n\"satnav_disclaimer_accepted\": \"YES\""));
    } // if

    // The fuel level as percentage (byte 7 of IDEN 4FC) does not seem to be regularly sent over the VAN bus,
    // so if we have it, report it here.
    if (fuelLevelPercentage != FUEL_LEVEL_PERCENTAGE_INVALID)
    {
        char floatBuf[MAX_FLOAT_SIZE];
        at += at >= n ? 0 :
            snprintf_P(buf + at, n - at, fuelLevelPercentageFormatter,
                fuelLevelPercentage,
                ToFloatStr(floatBuf, fuelLevelPercentage / 100.0, 2, false)
            );
    } // if

    if (strlen(vinNumber) != 0)
    {
        at += at >= n ? 0 : snprintf(buf + at, n - at, PSTR(",\n\"vin\": \"%-17.17s\""), vinNumber);
    } // if

    if (condenserPressure != CONDENSER_PRESSURE_INVALID)
    {
        char floatBuf[2][MAX_FLOAT_SIZE];

        at += at >= n ? 0 :
            snprintf_P(buf + at, n - at,
                PSTR(
                    ",\n\"condenser_pressure_bar\": \"%s\",\n"
                    "\"condenser_pressure_psi\": \"%s\""
                ),
                ToFloatStr(floatBuf[0], ToBar(condenserPressure), 1),
                ToFloatStr(floatBuf[1], ToPsi(condenserPressure), 0)
            );
    } // if

    if (evaporatorTemp != EVAPORATOR_TEMP_INVALID)
    {
        float evaporatorTempCelsius = evaporatorTemp / 10.0 - 40.0;
        char floatBuf[MAX_FLOAT_SIZE];
        at += at >= n ? 0 :
            snprintf_P(buf + at, n - at, PSTR(",\n\"evaporator_temp\": \"%s\""),
                mfdTemperatureUnit == MFD_TEMPERATURE_UNIT_CELSIUS ?
                    ToFloatStr(floatBuf, evaporatorTempCelsius, 1) :
                    ToFloatStr(floatBuf, ToFahrenheit(evaporatorTempCelsius), 0)
            );
    } // if

    if (setFanSpeed != SET_FAN_SPEED_INVALID)
    {
        at += at >= n ? 0 : snprintf_P(buf + at, n - at, PSTR(",\n\"set_fan_speed\": \"%u\""), setFanSpeed);
    } // if

    if (strlen_P(satnavStatus2Str) != 0)
    {
        at += at >= n ? 0 : snprintf_P(buf + at, n - at, PSTR(",\n\"satnav_status_2\": \"%s\""), satnavStatus2Str);
    } // if

    if (! satnavCurrentStreet.isEmpty())
    {
        at += at >= n ? 0 :
            snprintf_P(buf + at, n - at, PSTR(",\n\"satnav_curr_street\": \"%s\""), satnavCurrentStreet.c_str());
    } // if

    if (satnavServiceListSize > 0)
    {
        // Report also the number of services, so that the WebSocket client can enable the "Select a service" menu item
        at += at >= n ? 0 :
            snprintf_P(buf + at, n - at, PSTR
                (
                    ",\n"
                    "\"satnav_to_mfd_response\": \"%s\",\n"
                    "\"satnav_to_mfd_list_size\": \"%d\""
                ),
                SatNavRequestStr(SR_SERVICE_LIST),
                satnavServiceListSize
            );
    } // if

    at += at >= n ? 0 :
        snprintf_P
        (
            buf + at, n - at,
            PSTR(",\n\"mfd_disable_navigation_menu_while_driving\": \"%s\""),
          #ifdef MFD_DISABLE_NAVIGATION_MENU_WHILE_DRIVING
            yesStr
          #else
            noStr
          #endif
        );

    at += at >= n ? 0 : snprintf_P(buf + at, n - at, PSTR("\n}\n}\n"));

    // JSON buffer overflow?
    if (at >= n) return "";

  #ifdef PRINT_JSON_BUFFERS_ON_SERIAL
    Serial.printf_P(PSTR("%sEquipment status data as JSON object:\n"), TimeStamp());
    PrintJsonText(buf);
  #endif // PRINT_JSON_BUFFERS_ON_SERIAL

    return buf;
} // EquipmentStatusDataToJson

// Check if the new packet data differs from the previous.
// Optionally, print the new packet on serial port, highlighting the bytes that differ.
bool IsPacketDataDuplicate(TVanPacketRxDesc& pkt, IdenHandler_t* handler)
{
  #ifdef PRINT_RAW_PACKET_DATA
    uint16_t iden = pkt.Iden();
  #endif // PRINT_RAW_PACKET_DATA

    int dataLen = pkt.DataLen();
    const uint8_t* data = pkt.Data();

    // Relying on short-circuit boolean evaluation
    bool isDuplicate =
        handler->prevDataLen == dataLen  // handler->prevDataLen is initialized to -1
        && handler->prevData != nullptr
        && memcmp(data, handler->prevData, dataLen) == 0;

    if (handler->ignoreDups && isDuplicate) return true;  // Duplicate packet, to be ignored

    // Don't repeatedly print the same packet
    if (isDuplicate) return false;  // Duplicate packet, not to be ignored, but don't print over and over

  #ifdef PRINT_RAW_PACKET_DATA
    // Not a duplicate packet: print the diff, and save the packet to compare with the next
    if (IsPacketSelected(iden, SELECTED_PACKETS))
    {
        Serial.printf_P(PSTR("%sReceived: %s packet (IDEN %03X)\n"), TimeStamp(), handler->idenStr, iden);

        // The first time, or after an call to ResetPacketPrevData, handler->prevDataLen will be -1, so only
        // the "FULL: " line will be printed
        if (handler->prevData != nullptr && handler->prevDataLen >= 0)
        {
            // First line: print the new packet's data where it differs from the previous packet
            Serial.printf_P(PSTR("DIFF: %03X %1X (%s) "), iden, pkt.CommandFlags(), pkt.CommandFlagsStr());
            if (dataLen > 0)
            {
                int n = handler->prevDataLen;
                for (int i = 0; i < n; i++)
                {
                    char diffByte[] = "\u00b7\u00b7";  // \u00b7 is center dot character

                    // Relying on short-circuit boolean evaluation
                    if (i >= dataLen || data[i] != handler->prevData[i])
                    {
                        snprintf_P(diffByte, sizeof(diffByte), PSTR("%02X"), handler->prevData[i]);
                    } // if
                    Serial.printf_P(PSTR("%s%s"), diffByte, i < n - 1 ? dashStr : emptyStr);
                } // for
                Serial.print("\n");
            }
            else
            {
                Serial.print("<no_data>\n");
            } // if
        } // if
    } // if
  #endif // PRINT_RAW_PACKET_DATA

    if (handler->prevData == nullptr) handler->prevData = (uint8_t*) malloc(VAN_MAX_DATA_BYTES);

    if (handler->prevData != nullptr)
    {
        // Save packet data to compare against at next packet reception
        memset(handler->prevData, 0, VAN_MAX_DATA_BYTES);
        memcpy(handler->prevData, data, dataLen);
        handler->prevDataLen = dataLen;
    } // if

  #ifdef PRINT_RAW_PACKET_DATA
    if (IsPacketSelected(iden, SELECTED_PACKETS))
    {
        // Now print the new packet's data in full
        Serial.printf_P(PSTR("FULL: %03X %1X (%s) "), iden, pkt.CommandFlags(), pkt.CommandFlagsStr());
        if (dataLen > 0)
        {
            for (int i = 0; i < dataLen; i++)
            {
                Serial.printf_P(PSTR("%02X%s"), data[i], i < dataLen - 1 ? dashStr : emptyStr);
            } // for
            Serial.print("\n");
        }
        else
        {
            Serial.print("<no_data>\n");
        } // if
    } // if
  #endif // PRINT_RAW_PACKET_DATA

    return false;
} // IsPacketDataDuplicate

static IdenHandler_t handlers[] =
{
    // Columns:
    // 1. IDEN value,
    // 2. IDEN string,
    // 3. Number of expected bytes (or -1 if varying/unknown),
    // 4. Ignore duplicates (boolean)
    // 5. handler function
    // 6. prevDataLen: must be initialized to -1 to indicate "unknown"
    { VIN_IDEN, "vin", 17, true, &ParseVinPkt, -1, nullptr },
    { ENGINE_IDEN, "engine", 7, true, &ParseEnginePkt, -1, nullptr },
    { HEAD_UNIT_STALK_IDEN, "head_unit_stalk", 2, true, &ParseHeadUnitStalkPkt, -1, nullptr },
    { LIGHTS_STATUS_IDEN, "lights_status", -1, true, &ParseLightsStatusPkt, -1, nullptr },
    { DEVICE_REPORT, "device_report", -1, true, &ParseDeviceReportPkt, -1, nullptr },
    { CAR_STATUS1_IDEN, "car_status_1", 27, true, &ParseCarStatus1Pkt, -1, nullptr },
    { CAR_STATUS2_IDEN, "car_status_2", -1, true, &ParseCarStatus2Pkt, -1, nullptr },
    { DASHBOARD_IDEN, "dashboard", 7, true, &ParseDashboardPkt, -1, nullptr },
    { DASHBOARD_BUTTONS_IDEN, "dashboard_buttons", -1, true, &ParseDashboardButtonsPkt, -1, nullptr },
    { HEAD_UNIT_IDEN, "head_unit", -1, true, &ParseHeadUnitPkt, -1, nullptr },
    { MFD_LANGUAGE_UNITS_IDEN, "time", 5, true, &ParseMfdLanguageUnitsPkt, -1, nullptr },
    { AUDIO_SETTINGS_IDEN, "audio_settings", 11, true, &ParseAudioSettingsPkt, -1, nullptr },
    { MFD_STATUS_IDEN, "mfd_status", 2, true, &ParseMfdStatusPkt, -1, nullptr },
    { AIRCON1_IDEN, "aircon_1", 5, true, &ParseAirCon1Pkt, -1, nullptr },
    { AIRCON2_IDEN, "aircon_2", 7, true, &ParseAirCon2Pkt, -1, nullptr },
    { CDCHANGER_IDEN, "cd_changer", -1, true, &ParseCdChangerPkt, -1, nullptr },
    { SATNAV_STATUS_1_IDEN, "satnav_status_1", 6, true, &ParseSatNavStatus1Pkt, -1, nullptr },
    { SATNAV_STATUS_2_IDEN, "satnav_status_2", -1, false, &ParseSatNavStatus2Pkt, -1, nullptr },
    { SATNAV_STATUS_3_IDEN, "satnav_status_3", -1, true, &ParseSatNavStatus3Pkt, -1, nullptr },
    { SATNAV_GUIDANCE_DATA_IDEN, "satnav_guidance_data", 16, true, &ParseSatNavGuidanceDataPkt, -1, nullptr },
    { SATNAV_GUIDANCE_IDEN, "satnav_guidance", -1, true, &ParseSatNavGuidancePkt, -1, nullptr },
    { SATNAV_REPORT_IDEN, "satnav_report", -1, true, &ParseSatNavReportPkt, -1, nullptr },
    { MFD_TO_SATNAV_IDEN, "mfd_to_satnav", -1, true, &ParseMfdToSatNavPkt, -1, nullptr },
    { SATNAV_TO_MFD_IDEN, "satnav_to_mfd", 27, true, &ParseSatNavToMfdPkt, -1, nullptr },
    { SATNAV_DOWNLOADING_IDEN, "satnav_downloading", 0, false, &ParseSatNavDownloading, -1, nullptr },
    { WHEEL_SPEED_IDEN, "wheel_speed", 5, true, &ParseWheelSpeedPkt, -1, nullptr },
    { ODOMETER_IDEN, "odometer", 5, true, &ParseOdometerPkt, -1, nullptr },
    { COM2000_IDEN, "com2000", 10, true, &ParseCom2000Pkt, -1, nullptr },
    { CDCHANGER_COMMAND_IDEN, "cd_changer_command", 2, true, &ParseCdChangerCmdPkt, -1, nullptr },
    { MFD_TO_HEAD_UNIT_IDEN, "display_to_head_unit", -1, true, &ParseMfdToHeadUnitPkt, -1, nullptr },
}; // handlers

const IdenHandler_t* const handlers_end = handlers + sizeof(handlers) / sizeof(handlers[0]);

const char* ParseVanPacketToJson(TVanPacketRxDesc& pkt)
{
    int dataLen = pkt.DataLen();
    if (dataLen < 0 || dataLen > VAN_MAX_DATA_BYTES) return ""; // Unexpected packet length

  #ifdef ON_DESK_MFD_ESP_MAC
    if (WiFi.macAddress() == ON_DESK_MFD_ESP_MAC)
    {
      // On the desk test setup, we want to see a lot of detailed output
        if (! pkt.CheckCrc())
        {
            Serial.printf_P(PSTR("%sVAN PACKET CRC ERROR!\n"), TimeStamp());

          #ifdef VAN_RX_ISR_DEBUGGING
            // Show byte content of packet, plus full dump of bit timings for packets that have CRC ERROR,
            // for further analysis
            pkt.DumpRaw(Serial);
            pkt.getIsrDebugPacket().Dump(Serial);
          #endif // VAN_RX_ISR_DEBUGGING

            // Show byte content of packet for easy comparing with the repaired version
            pkt.DumpRaw(Serial);

            if (! pkt.CheckCrcAndRepair()) return ""; // CRC error

            // Print again, after fix
            pkt.DumpRaw(Serial);
        } // if
    }
    else
    {
  #endif // ON_DESK_MFD_ESP_MAC

        if (! pkt.CheckCrcAndRepair())
        {
          #ifdef PRINT_VAN_CRC_ERROR_PACKETS_ON_SERIAL
            // Show byte content of packet
            Serial.printf_P(PSTR("%sVAN PACKET CRC ERROR!\n"), TimeStamp());
            pkt.DumpRaw(Serial);

          #ifdef VAN_RX_ISR_DEBUGGING
            // Fully dump bit timings for packets that have CRC ERROR, for further analysis
            pkt.getIsrDebugPacket().Dump(Serial);
          #endif // VAN_RX_ISR_DEBUGGING
          #endif // PRINT_VAN_CRC_ERROR_PACKETS_ON_SERIAL

            return ""; // CRC error
        } // if

  #ifdef ON_DESK_MFD_ESP_MAC
    } // if
  #endif // ON_DESK_MFD_ESP_MAC

    uint16_t iden = pkt.Iden();
    IdenHandler_t* handler = handlers;

    // Search the correct handler. Relying on short-circuit boolean evaluation.
    while (handler != handlers_end && handler->iden != iden) handler++;

    // Hander found?
    if (handler == handlers_end) return ""; // Unrecognized IDEN value

    if (handler->dataLen >= 0 && dataLen != handler->dataLen) return ""; // Unexpected packet length

    // Check if packet content is the same as in previous packet and must therefore be ignored
    if (IsPacketDataDuplicate(pkt, handler)) return "";

    int result = handler->parser(pkt, jsonBuffer, JSON_BUFFER_SIZE);

    // Errors we would like to see printed on the serial port
    if (result == VAN_PACKET_PARSE_UNEXPECTED_LENGTH
        || result == VAN_PACKET_PARSE_UNRECOGNIZED_IDEN
        || result == VAN_PACKET_PARSE_JSON_TOO_LONG
       )
    {
        Serial.printf_P(
            PSTR("%s==> WARNING: VAN packet parsing result = '%s'!\n"),
            TimeStamp(),
            VanPacketParseResultStr(result)
        );

        pkt.DumpRaw(Serial);

        // No use to return the JSON buffer; it is invalid
        return "";
    } // if

    // Any other errors: silently ignore
    if (result != VAN_PACKET_PARSE_OK) return "";

  #ifdef PRINT_JSON_BUFFERS_ON_SERIAL
    if (IsPacketSelected(iden, SELECTED_PACKETS))
    {
        Serial.printf_P(PSTR("%sParsed to JSON object:\n"), TimeStamp());
        PrintJsonText(jsonBuffer);
    } // if
  #endif // PRINT_JSON_BUFFERS_ON_SERIAL

    return jsonBuffer;
} // ParseVanPacketToJson

// Called when the user chooses a different distance unit or temperature unit, so that reporting is done immediately,
// instead of having to wait for a differing packet.
void ResetPacketPrevData()
{
    IdenHandler_t* handler = handlers;

    while (handler != handlers_end)
    {
        if (handler->prevData != nullptr)
        {
            memset(handler->prevData, 0, VAN_MAX_DATA_BYTES);
            handler->prevDataLen = -1;  // Re-initialize
        } // if

        handler++;
    } // while

    // These packet handlers have internal logic to skip duplicates:
    SkipCarStatus1PktDupDetect = true;
    SkipAirCon2PktDupDetect = true;
    SkipEnginePktDupDetect = true;
} // ResetPacketPrevData
