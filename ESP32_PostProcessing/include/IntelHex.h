#ifndef INTELHEX_H
#define INTELHEX_H

#include <Arduino.h>
#include <map>
#include <vector>
#include <SdFat.h>

class HexData {
public:
    HexData();
    ~HexData();
    
    bool load(FsFile& file, bool enforceChecksum);
    bool loadSW18BootloaderArray(FsFile& file);
    
    void fill(uint32_t inclusiveStartAddress, uint32_t exclusiveEndAddress, uint8_t value);
    void fill16(uint32_t inclusiveStartAddress, uint32_t exclusiveEndAddress, uint16_t value);
    void fill32(uint32_t inclusiveStartAddress, uint32_t exclusiveEndAddress, uint32_t value);
    void crop(uint32_t inclusiveStartAddress, uint32_t exclusiveEndAddress);
    
    uint16_t crc16ccitt(uint32_t start, uint32_t exclusiveEnd);
    
    bool writeByte(uint32_t address, uint8_t value);
    bool readByte(uint32_t address, uint8_t& value);
    
    bool toHexFileString(FsFile& output, uint32_t inclusiveStartAddress, uint32_t exclusiveEndAddress);
    bool toSW18BootloaderArray(FsFile& output, uint32_t inclusiveStartAddress, uint32_t exclusiveEndAddress, bool commentAddresses = false);
    bool twoColumn(FsFile& output);
    
    uint32_t getHighestAddress();
    uint32_t getLowestAddress();
    size_t getMemorySize();
    
    String getWarnings();
    void clearWarnings();
    
private:
    std::map<uint32_t, uint8_t> memory;
    String warnings;
    
    uint32_t hexToVal(const String& s);
    String hexLine(uint32_t address, uint32_t& addressHighWord, std::vector<uint8_t>& dataline);
};

#endif // INTELHEX_H
