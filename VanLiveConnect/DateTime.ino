
int _timeZoneOffset = 0;  // in minutes

void SetTimeZoneOffset(int newTimeZoneOffset)
{
    if (newTimeZoneOffset >= -12 * 60 && newTimeZoneOffset <= 12 * 60) _timeZoneOffset = newTimeZoneOffset;
} // SetTimeZoneOffset

#ifdef PREPEND_TIME_STAMP_TO_DEBUG_OUTPUT

#include <TimeLib.h>  // https://github.com/PaulStoffregen/Time

// Check if an epoch value is valid
inline boolean IsTimeValid(time_t t)
{
    return (t >= 1451606400);  // 2016-01-01 0:00:00
} // IsTimeValid 

static char _dt[30] = {0};
const char* DateTime(time_t t, boolean full)
{
    if (! IsTimeValid(t))
    {
        // Print elapsed time since last (re-)boot

        unsigned long m = millis() / 1000;  // Total number of seconds since (re-)boot
        unsigned long hours = m / 3600;
        m = m % 3600;
        unsigned long minutes = m / 60;
        unsigned long seconds = m % 60;
        sprintf_P(_dt, PSTR("%02d:%02d:%02d\0"), hours, minutes, seconds);

        // Replace leading '00' by '--' to indicate this is not real time of day
        if (_dt[0] == '0' && _dt[1] == '0')
        {
            _dt[0] = _dt[1] = '-';
            if (_dt[3] == '0' && _dt[4] == '0') _dt[3] = _dt[4] = '-';
        } // if

        return _dt;
    } // if

    if (! full)
    {
        time_t n = now();
        if (t >= previousMidnight(n) && t < nextMidnight(n))
        {
            // Today
            sprintf_P(_dt, PSTR("%02d:%02d:%02d\0"), hour(t), minute(t), second(t));
            return _dt;
        }
    } // if

    // Other day, or 'full' format requested
    sprintf_P(_dt, PSTR("%04d-%02d-%02d %02d:%02d:%02d\0"), year(t), month(t), day(t), hour(t), minute(t), second(t));

    return _dt;
} // DateTime

uint32_t _msec = 0;
unsigned long _millis_at_setTime = 0;

bool SetTime(uint32_t epoch, uint32_t msec)
{
    if (! IsTimeValid(epoch)) return false;

    _millis_at_setTime = millis();

    msec += 30;  // Typical latency
    if (msec >= 1000)
    {
        epoch++;
        msec -= 1000;
    } // if

    _msec = msec;

    epoch += _timeZoneOffset * 60;
    setTime(epoch);  // function from TimeLib.h

    return true;
} // SetTime

static char _ts[sizeof(_dt) + 7] = {0};
const char* TimeStamp()
{
    time_t t = now();
    unsigned long m = millis();
    unsigned long diff = (m - _millis_at_setTime) % 1000UL;
    unsigned long msec = _msec + diff;

    if (msec >= 1000)
    {
        t++;
        msec -= 1000;
    } // if

    sprintf(_ts, "[%s.%03lu] \0", DateTime(t), msec);
    return _ts;
} // TimeStamp

void PrintTimeStamp()
{
    Serial.printf("%s", TimeStamp());
} // PrintTimeStamp

#else

const char* TimeStamp()
{
    return "";
} // TimeStamp

void PrintTimeStamp()
{
} // PrintTimeStamp

#endif  // PREPEND_TIME_STAMP_TO_DEBUG_OUTPUT
