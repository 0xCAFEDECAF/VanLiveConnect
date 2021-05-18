
#include <ArduinoJson.h>
#include "FS.h"

#define MILLIS_PER_SEC (1000UL)

StoredData _initialStore =
{
    .satnavGuidanceActive = false,
    .satnavDiscPresent = false,
    .satnavGuidancePreference = 0x00,
};

StoredData* _store = NULL;

// Make sure the number is greater than or equal to the number of entries in StoredData
#define JSON_BUFFER_SIZE (JSON_OBJECT_SIZE(10 + 2 * MAX_DIRECTORY_ENTRIES))
//#define JSON_BUFFER_SIZE (JSON_OBJECT_SIZE(10))
const char STORE_FILE_NAME[] = "/store.json";

unsigned long _lastSaved = 0;

void saveStore()
{
    File storeFile = SPIFFS.open(STORE_FILE_NAME, "w");
    if (! storeFile) return;

    StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();

    root["satnavGuidanceActive"] = _store->satnavGuidanceActive;
    root["satnavDiscPresent"] = _store->satnavDiscPresent; 
    root["satnavGuidancePreference"] = _store->satnavGuidancePreference; 

    int i = 0;
    JsonArray& personalDirectoryEntries = root.createNestedArray("personalDirectoryEntries");
    while (true)
    {
        if (i >= MAX_DIRECTORY_ENTRIES || _store->personalDirectoryEntries[i].length() == 0) break;
        personalDirectoryEntries.add(_store->personalDirectoryEntries[i++]);
    } // while

    i = 0;
    JsonArray& professionalDirectoryEntries = root.createNestedArray("professionalDirectoryEntries");
    while (true)
    {
        if (i >= MAX_DIRECTORY_ENTRIES || _store->professionalDirectoryEntries[i].length() == 0) break;
        professionalDirectoryEntries.add(_store->professionalDirectoryEntries[i++]);
    } // while

    Serial.printf_P(PSTR("Writing to '%s' ..."), STORE_FILE_NAME);

    size_t nWritten = root.printTo(storeFile);
    size_t size = storeFile.size();
    if (nWritten == size) Serial.printf_P(PSTR(" %lu bytes written OK\n"), nWritten);
    else Serial.println(F(" FAILED!"));

    storeFile.close();

    _lastSaved = millis();
} // saveStore

void _readStore()
{
    File storeFile = SPIFFS.open(STORE_FILE_NAME, "r");

    // If the file does not exist or is invalid, try to create one with initial values
    if (! storeFile) return saveStore();
    size_t size = storeFile.size();
    if (size > 4096) return saveStore();

    // Allocate a buffer to store contents of the file
    std::unique_ptr<char[]> buf(new char[size + 1]);

    // We don't use String here because ArduinoJson library requires the input
    // buffer to be mutable. If you don't use ArduinoJson, you may as well
    // use storeFile.readString(...) instead.
    storeFile.readBytes(buf.get(), size);
    *(buf.get() + size) = 0;

    Serial.printf_P(PSTR("Contents of '%s':\n"), STORE_FILE_NAME);
    Serial.println(buf.get());

    StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(buf.get());
    if (!root.success()) return saveStore();

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
            Serial.println(it->as<char*>());
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
            Serial.println(it->as<char*>());
            _store->professionalDirectoryEntries[i++] = it->as<String>();
        } // for
    } // if

    storeFile.close();
} // _readStore

String _formatBytes(size_t bytes)
{
    if (bytes < 1024) return String(bytes)+" Bytes";
    else if (bytes < (1024 * 1024)) return String(bytes/1024.0)+" KBytes";
    else if(bytes < (1024 * 1024 * 1024)) return String(bytes/1024.0/1024.0)+" MBytes";
    else return String(bytes/1024.0/1024.0/1024.0)+" GBytes";
} // _formatBytes

void _setupStore()
{
    Serial.print(F("Mounting SPI Flash File System (SPIFFS) ..."));

    // Make sure the file system is formatted and mounted
    if (! SPIFFS.begin())
    {
        Serial.print(F("\nFailed to mount file system, trying to format ..."));
        if (! SPIFFS.format())
        {
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
        Serial.printf_P(PSTR("FS File: '%s', size: %s\n"), fileName.c_str(), _formatBytes(fileSize).c_str());

        // Remove all files except one "instance" of '/store.json'
        if (fileName != STORE_FILE_NAME || foundOne) SPIFFS.remove(fileName);

        if (fileName == STORE_FILE_NAME) foundOne = true;
    } // while

    _readStore();
} // setupStore

StoredData* getStore()
{
    if (_store) return _store;

    // Allocate and fill with initial data
    _store = new StoredData;
    *_store = _initialStore;

    _setupStore();

    return _store;
} // getStore
