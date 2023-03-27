
int _timeZoneOffset = 0;  // in minutes

void SetTimeZoneOffset(int newTimeZoneOffset)
{
    if (newTimeZoneOffset >= -12 * 60 && newTimeZoneOffset <= 12 * 60) _timeZoneOffset = newTimeZoneOffset;
} // SetTimeZoneOffset

#ifdef PREPEND_TIME_STAMP_TO_DEBUG_OUTPUT

#include <TimeLib.h>

// Check if an epoch value is valid
inline boolean IsTimeValid(time_t t)
{
    return (t >= 1451606400UL);  // 2016-01-01 0:00:00
} // IsTimeValid 

char _dt[20];
const char* DateTime(time_t t, boolean full)
{
    if (! IsTimeValid(t))
    {
        // Print elapsed time since last (re-)boot

        unsigned long t = millis() / 1000;  // Total number of seconds since (re-)boot
        unsigned long hours = t / 3600;
        t = t % 3600;
        unsigned long minutes = t / 60;
        unsigned long seconds = t % 60;
        sprintf_P(_dt, PSTR("%02d:%02d:%02d"), hours, minutes, seconds);

        // Replace leading '00' by '--' to indicate this is not real time of day
        if (_dt[0] == '0' && _dt[1] == '0')
        {
            _dt[0] = _dt[1] = '-';
            if (_dt[3] == '0' && _dt[4] == '0') _dt[3] = _dt[4] = '-';
        } // if

        return _dt;
    } // if

    time_t prev, next;

    if (! full)
    {
        time_t t = now();
        prev = previousMidnight(t);
        next = nextMidnight(t);
    } // if

    // Rely on short-circuit boolean evaluation
    if (! full && t >= prev && t < next)
    {
        // Today
        sprintf_P(_dt, PSTR("%02d:%02d:%02d"), hour(t), minute(t), second(t));
    }
    else
    {
        // Other day, or 'full' format requested
        sprintf_P(_dt, PSTR("%02d-%02d-%04d %02d:%02d:%02d"), day(t), month(t), year(t), hour(t), minute(t), second(t));
    } // if

    return _dt;
} // DateTime

uint32_t _msec = 0;
unsigned long _millis_at_setTime = 0;

bool SetTime(uint32_t epoch, uint32_t msec)
{
    if (! IsTimeValid(epoch)) return false;

    msec += 30;  // Typical latency

    _msec = msec;
    _millis_at_setTime = millis();

    epoch += _timeZoneOffset * 60;
    setTime(epoch);  // function from TimeLib.h

    return true;
} // SetTime

char _ts[sizeof(_dt) + 7];
const char* TimeStamp()
{
    time_t t = now();
    unsigned long m = millis();
    unsigned long diff = (m - _millis_at_setTime) % 1000UL;
    unsigned long msec = _msec + diff;

    if (msec > 1000)
    {
        t++;
        msec -= 1000;
    } // if

    sprintf_P(_ts, "[%s.%03lu] ", DateTime(t), msec);
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
