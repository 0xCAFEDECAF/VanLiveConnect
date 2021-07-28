
#include <ArduinoJson.h>
#include "FS.h"

#define MILLIS_PER_SEC (1000UL)

StoredData_t _initialStore =
{
    .smallScreenIndex = SMALL_SCREEN_TRIP_INFO_1, // When the original MFD is plugged in, this is what it starts with
    .satnavGuidanceActive = false,
    .satnavDiscPresent = false,
    .satnavGuidancePreference = 0x00,
};

StoredData_t* _store = NULL;

// Table with the MD5 hash value of each file in the root directory
struct FileMd5Entry_t
{
    String path;
    String md5;
}; // FileMd5Entry_t

#define MAX_FILE_MD5 20
FileMd5Entry_t fileMd5[MAX_FILE_MD5];
int fileMd5Last = 0;

// Make sure the number is greater than or equal to the number of entries in StoredData_t
#define JSON_BUFFER_SIZE (JSON_OBJECT_SIZE(10 + 2 * MAX_DIRECTORY_ENTRIES))
//#define JSON_BUFFER_SIZE (JSON_OBJECT_SIZE(20))
const char STORE_FILE_NAME[] = "/store.json";

bool _storeDirty = false;
unsigned long _lastMarked = 0;

void _saveStore()
{
    StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();

    root["smallScreenIndex"] = _store->smallScreenIndex;
    root["satnavGuidanceActive"] = _store->satnavGuidanceActive;
    root["satnavDiscPresent"] = _store->satnavDiscPresent;
    root["satnavGuidancePreference"] = _store->satnavGuidancePreference;

    int i = 0;
    JsonArray& personalDirectoryEntries = root.createNestedArray("personalDirectoryEntries");
    while (i < MAX_DIRECTORY_ENTRIES && _store->personalDirectoryEntries[i].length() > 0)
    {
        personalDirectoryEntries.add(_store->personalDirectoryEntries[i++]);
    } // while

    i = 0;
    JsonArray& professionalDirectoryEntries = root.createNestedArray("professionalDirectoryEntries");
    while (i < MAX_DIRECTORY_ENTRIES && _store->professionalDirectoryEntries[i].length() > 0)
    {
        professionalDirectoryEntries.add(_store->professionalDirectoryEntries[i++]);
    } // while

    Serial.printf_P(PSTR("Writing to '%s' ..."), STORE_FILE_NAME);

    VanBusRx.Disable();
    File storeFile = SPIFFS.open(STORE_FILE_NAME, "w");
    if (! storeFile)
    {
        VanBusRx.Enable();
        Serial.println(F(" FAILED!"));
        return;
    } // if

    size_t nWritten = root.printTo(storeFile);
    size_t size = storeFile.size();
    storeFile.close();
    VanBusRx.Enable();

    if (nWritten != size)
    {
        Serial.println(F(" FAILED!"));
        return;
    } // if

    Serial.printf_P(PSTR(" %lu bytes written OK\n"), nWritten);
    _storeDirty = false;
} // _saveStore

void _readStore()
{
    VanBusRx.Disable();
    File storeFile = SPIFFS.open(STORE_FILE_NAME, "r");

    // If the file does not exist or is invalid, try to create one with initial values
    if (! storeFile) return _saveStore();  // Will also re-enable the VanBusRx
    size_t size = storeFile.size();
    if (size > 4096) return _saveStore();  // Will also re-enable the VanBusRx

    // Allocate a buffer to store contents of the file
    std::unique_ptr<char[]> buf(new char[size + 1]);

    // We don't use String here because ArduinoJson library requires the input
    // buffer to be mutable. If you don't use ArduinoJson, you may as well
    // use storeFile.readString(...) instead.
    storeFile.readBytes(buf.get(), size);

    storeFile.close();
    VanBusRx.Enable();

    *(buf.get() + size) = 0;

    Serial.printf_P(PSTR("Contents of '%s':\n"), STORE_FILE_NAME);
    Serial.println(buf.get());

    StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;

    JsonObject& root = jsonBuffer.parseObject(buf.get());
    if (!root.success()) return _saveStore();

    if (root.containsKey("smallScreenIndex")) _store->smallScreenIndex = root["smallScreenIndex"];
    if (root.containsKey("satnavGuidanceActive")) _store->satnavGuidanceActive = root["satnavGuidanceActive"];
    if (root.containsKey("satnavDiscPresent")) _store->satnavDiscPresent = root["satnavDiscPresent"];
    if (root.containsKey("satnavGuidancePreference")) _store->satnavGuidancePreference = root["satnavGuidancePreference"];

    if (root.containsKey("personalDirectoryEntries"))
    {
        JsonArray& arr = root["personalDirectoryEntries"];
        int i = 0;
        for (JsonArray::iterator it = arr.begin(); it != arr.end(); ++it)
        {
            if (i >= MAX_DIRECTORY_ENTRIES) break;
            //Serial.println(it->as<char*>());
            _store->personalDirectoryEntries[i++] = it->as<String>();
        } // for
    } // if
    if (root.containsKey("professionalDirectoryEntries"))
    {
        JsonArray& arr = root["professionalDirectoryEntries"];
        int i = 0;
        for (JsonArray::iterator it = arr.begin(); it != arr.end(); ++it)
        {
            if (i >= MAX_DIRECTORY_ENTRIES) break;
            //Serial.println(it->as<char*>());
            _store->professionalDirectoryEntries[i++] = it->as<String>();
        } // for
    } // if
} // _readStore

String _formatBytes(size_t bytes)
{
    if (bytes < 1024) return String(bytes)+" Bytes";
    else if (bytes < (1024 * 1024)) return String(bytes/1024.0)+" KBytes";
    else if(bytes < (1024 * 1024 * 1024)) return String(bytes/1024.0/1024.0)+" MBytes";
    else return String(bytes/1024.0/1024.0/1024.0)+" GBytes";
} // _formatBytes

void SetupStore()
{
    // Allocate and fill with initial data
    _store = new StoredData_t;
    *_store = _initialStore;

    Serial.print(F("Mounting SPI Flash File System (SPIFFS) ..."));

    VanBusRx.Disable();

    // Make sure the file system is formatted and mounted
    if (! SPIFFS.begin())
    {
        Serial.print(F("\nFailed to mount file system, trying to format ..."));
        if (! SPIFFS.format())
        {
            VanBusRx.Enable();
            Serial.println(F("\nFailed to format file system, no persistent storage available!"));
            return;
        } // if
    } // if

    // Print file system size
    FSInfo fs_info;
    SPIFFS.info(fs_info);
    char b[MAX_FLOAT_SIZE];
    Serial.printf_P(PSTR(" OK, total %s MByes\n"), FloatToStr(b, fs_info.totalBytes / 1024.0 / 1024.0, 2));

    // Print the contents of the root directory
    bool foundOne = false;
    Dir dir = SPIFFS.openDir("/");
    while (dir.next())
    {
        String fileName = dir.fileName();
        size_t fileSize = dir.fileSize();

        // Remove all "instances" of '/store.json' except one (yes, SPIFFS sometimes has the same name
        // for different files)
        if (fileName == STORE_FILE_NAME && foundOne)
        {
            SPIFFS.remove(fileName);
            continue;
        } // if

        // We want to find at least '/store.json'
        if (fileName == STORE_FILE_NAME) foundOne = true;

        // Create a table with the MD5 hash value of each file
        // Inspired by https://github.com/esp8266/Arduino/issues/3003
        File file = SPIFFS.open(fileName, "r");
        MD5Builder md5;
        md5.begin();
        md5.addStream(file, file.size());
        md5.calculate();
        md5.toString();
        file.close();

        Serial.printf_P(
            PSTR("FS File: '%s', size: %s, MD5: %s\n"),
            fileName.c_str(),
            _formatBytes(fileSize).c_str(),
            md5.toString().c_str()
        );

        if (fileMd5Last < MAX_FILE_MD5)
        {
            fileMd5[fileMd5Last].path = fileName;
            fileMd5[fileMd5Last].md5 = md5.toString();
            fileMd5Last++;
        }
        else
        {
            Serial.println("=====> Too many files found: please increase MAX_FILE_MD5");
        } // if
    } // while

    VanBusRx.Enable();

    _readStore();
} // SetupStore

String getMd5(const String& path)
{
    int i = 0;
    while (i < MAX_FILE_MD5)
    {
        if (fileMd5[i].path == path) return fileMd5[i].md5;
        i++;
    } // while

    // To indicate 'path' not found
    return "";
} // getMd5

// Must have called SetupStore() first.
// Note: 'lazy initialization' within this function (and anywhere else) just doesn't work. If this function is e.g.
// invoked from inside a callback function, the initialization just takes too long, or the stack overflows, or whatever,
// and the system becomes unstable.
StoredData_t* GetStore()
{
    return _store;
} // GetStore

void MarkStoreDirty()
{
    if (_storeDirty) return;
    _storeDirty = true;
    _lastMarked = millis();
} // MarkStoreDirty

void LoopStore()
{
    if (! _storeDirty) return;

    // Flush to file after so many seconds
    #define FLUSH_AFTER_SEC (5)

    // Arithmetic has safe roll-over
    if (millis() - _lastMarked < FLUSH_AFTER_SEC * MILLIS_PER_SEC) return;

    _saveStore();
} // LoopStore
