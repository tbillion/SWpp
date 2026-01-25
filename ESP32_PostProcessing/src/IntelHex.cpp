#include "IntelHex.h"

static const int HEX_LENGTH_OFFSET = 1;
static const int HEX_ADDRESS_OFFSET = 3;
static const int HEX_RECORD_TYPE_OFFSET = 7;
static const int HEX_DATA_OFFSET = 9;

static String formatHex(uint32_t value, int width) {
    String hex = String(value, HEX);
    hex.toUpperCase();
    while (hex.length() < (size_t)width) {
        hex = "0" + hex;
    }
    return hex;
}

HexData::HexData() {
    memory.clear();
    warnings = "";
}

HexData::~HexData() {
    memory.clear();
}

uint32_t HexData::hexToVal(const String& s) {
    uint32_t returnValue = 0;
    String upper = s;
    upper.toUpperCase();
    
    for (size_t i = 0; i < upper.length(); i++) {
        char c = upper[i];
        if (c >= '0' && c <= '9') {
            returnValue *= 16;
            returnValue += (uint32_t)(c - '0');
        } else if (c >= 'A' && c <= 'F') {
            returnValue *= 16;
            returnValue += (uint32_t)(c - 'A' + 10);
        } else {
            warnings += "Invalid character " + String(c) + " in Hex conversion\n";
            return 0;
        }
    }
    return returnValue;
}

bool HexData::load(FsFile& file, bool enforceChecksum) {
    if (!file) {
        warnings += "Invalid file handle\n";
        return false;
    }
    
    char line[256];
    uint32_t extendedAddress = 0;
    
    while (file.available()) {
        int len = file.fgets(line, sizeof(line));
        if (len <= 0) continue;
        
        String lineStr = String(line);
        lineStr.trim();
        
        if (lineStr.length() == 0) continue;
        
        lineStr.replace(" ", "");
        lineStr.replace("\t", "");
        lineStr.replace("\r", "");
        lineStr.replace("\n", "");
        
        if (lineStr.length() < 11) continue;
        if (lineStr[0] != ':') continue;
        
        uint32_t dataLength = hexToVal(lineStr.substring(HEX_LENGTH_OFFSET, HEX_LENGTH_OFFSET + 2));
        if ((dataLength * 2 + 11) != lineStr.length()) continue;
        
        uint32_t lineAddress = hexToVal(lineStr.substring(HEX_ADDRESS_OFFSET, HEX_ADDRESS_OFFSET + 4));
        uint32_t indicatedChecksum = hexToVal(lineStr.substring(lineStr.length() - 2, lineStr.length()));
        
        uint32_t calculatedChecksum = 0;
        for (size_t i = HEX_LENGTH_OFFSET; i < lineStr.length() - 2; i += 2) {
            calculatedChecksum += hexToVal(lineStr.substring(i, i + 2));
        }
        calculatedChecksum %= 256;
        calculatedChecksum = (256 - calculatedChecksum) % 256;
        
        if ((calculatedChecksum != indicatedChecksum) && enforceChecksum) {
            continue;
        }
        
        uint32_t recordType = hexToVal(lineStr.substring(HEX_RECORD_TYPE_OFFSET, HEX_RECORD_TYPE_OFFSET + 2));
        
        switch (recordType) {
            case 0: {
                for (uint32_t i = 0; i < dataLength; i++) {
                    uint8_t data = (uint8_t)hexToVal(lineStr.substring(HEX_DATA_OFFSET + 2 * i, HEX_DATA_OFFSET + 2 * i + 2));
                    uint32_t byteAddress = extendedAddress * 65536 + lineAddress + i;
                    if (memory.find(byteAddress) != memory.end()) {
                        warnings += "Address 0x" + String(byteAddress, HEX) + " is defined multiple times\n";
                    }
                    memory[byteAddress] = data;
                }
                break;
            }
            case 4: {
                if (dataLength != 2) continue;
                extendedAddress = hexToVal(lineStr.substring(HEX_DATA_OFFSET, HEX_DATA_OFFSET + 4));
                break;
            }
            case 1: {
                break;
            }
            default:
                break;
        }
    }
    
    return true;
}

void HexData::fill(uint32_t inclusiveStartAddress, uint32_t exclusiveEndAddress, uint8_t value) {
    for (uint32_t address = inclusiveStartAddress; address < exclusiveEndAddress; address++) {
        if (memory.find(address) == memory.end()) {
            memory[address] = value;
        }
    }
}

void HexData::fill16(uint32_t inclusiveStartAddress, uint32_t exclusiveEndAddress, uint16_t value) {
    for (uint32_t address = inclusiveStartAddress; address < exclusiveEndAddress; address += 2) {
        if (memory.find(address) == memory.end()) {
            memory[address] = (uint8_t)(value & 0xFF);
            memory[address + 1] = (uint8_t)(value >> 8);
        }
    }
}

void HexData::fill32(uint32_t inclusiveStartAddress, uint32_t exclusiveEndAddress, uint32_t value) {
    for (uint32_t address = inclusiveStartAddress; address < exclusiveEndAddress; address += 4) {
        if (memory.find(address) == memory.end()) {
            memory[address] = (uint8_t)(value & 0xFF);
            memory[address + 1] = (uint8_t)(value >> 8);
            memory[address + 2] = (uint8_t)(value >> 16);
            memory[address + 3] = (uint8_t)(value >> 24);
        }
    }
}

void HexData::crop(uint32_t inclusiveStartAddress, uint32_t exclusiveEndAddress) {
    auto it = memory.begin();
    while (it != memory.end()) {
        if (it->first < inclusiveStartAddress || it->first >= exclusiveEndAddress) {
            it = memory.erase(it);
        } else {
            ++it;
        }
    }
}

uint16_t HexData::crc16ccitt(uint32_t start, uint32_t exclusiveEnd) {
    uint16_t crc = 0xFFFF;
    uint32_t length = exclusiveEnd - start;
    
    for (uint32_t i = 0; i < length; i++) {
        uint32_t addr = i + start;
        auto it = memory.find(addr);
        if (it == memory.end()) {
            warnings += "CRC calculation error: Address 0x" + String(addr, HEX) + " not found\n";
            return 0;
        }
        uint16_t data = it->second;
        crc ^= (uint16_t)(data << 8);
        for (int j = 0; j < 8; j++) {
            if ((crc & 0x8000) > 0) {
                crc = (uint16_t)((crc << 1) ^ 0x1021);
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

bool HexData::writeByte(uint32_t address, uint8_t value) {
    memory[address] = value;
    return true;
}

bool HexData::readByte(uint32_t address, uint8_t& value) {
    auto it = memory.find(address);
    if (it != memory.end()) {
        value = it->second;
        return true;
    }
    return false;
}

String HexData::hexLine(uint32_t address, uint32_t& addressHighWord, std::vector<uint8_t>& dataline) {
    String s = "";
    
    for (size_t i = 0; i < dataline.size(); i++) {
        if (((address + i) >> 16) != addressHighWord) {
            if (i != 0) {
                uint32_t checksum = 0;
                s += ":";
                s += formatHex(i, 2);
                s += formatHex(address & 0xFFFF, 4);
                s += "00";
                checksum += i;
                checksum += address >> 8;
                checksum += address & 0xFF;
                
                for (size_t x = 0; x < i; x++) {
                    s += formatHex(dataline[x], 2);
                    checksum += dataline[x];
                }
                dataline.erase(dataline.begin(), dataline.begin() + i);
                checksum = ~checksum;
                checksum++;
                s += formatHex(checksum & 0xFF, 2);
                s += "\n";
            }
            {
                uint32_t checksum = 0;
                s += ":02000004";
                s += formatHex(address >> 16, 4);
                checksum = 2 + 4 + ((address >> 16) & 0xFF) + ((address >> 24) & 0xFF);
                checksum = ~checksum;
                checksum++;
                s += formatHex(checksum & 0xFF, 2);
                s += "\n";
                addressHighWord = address >> 16;
            }
        }
    }
    
    if (dataline.size() > 0) {
        uint32_t checksum = 0;
        s += ":";
        s += formatHex(dataline.size(), 2);
        s += formatHex(address & 0xFFFF, 4);
        s += "00";
        checksum += dataline.size();
        checksum += address >> 8;
        checksum += address & 0xFF;
        
        for (uint8_t b : dataline) {
            s += formatHex(b, 2);
            checksum += b;
        }
        checksum = ~checksum;
        checksum++;
        s += formatHex(checksum & 0xFF, 2);
        s += "\n";
    }
    return s;
}

bool HexData::toHexFileString(FsFile& output, uint32_t inclusiveStartAddress, uint32_t exclusiveEndAddress) {
    if (!output) return false;
    
    uint32_t address = inclusiveStartAddress;
    uint32_t addressHighWord = 0;
    std::vector<uint8_t> dataline;
    
    for (uint32_t i = inclusiveStartAddress; i < exclusiveEndAddress; i++) {
        auto it = memory.find(i);
        if (it != memory.end()) {
            if (dataline.size() == 0) {
                address = i;
            }
            dataline.push_back(it->second);
            if (dataline.size() >= 16 || ((i & 0xF) == 0xF)) {
                String line = hexLine(address, addressHighWord, dataline);
                output.print(line);
                dataline.clear();
            }
        } else {
            if (dataline.size() > 0) {
                String line = hexLine(address, addressHighWord, dataline);
                output.print(line);
                dataline.clear();
            }
        }
    }
    
    if (dataline.size() > 0) {
        String line = hexLine(address, addressHighWord, dataline);
        output.print(line);
    }
    output.println(":00000001FF");
    return true;
}

bool HexData::toSW18BootloaderArray(FsFile& output, uint32_t inclusiveStartAddress, uint32_t exclusiveEndAddress, bool commentAddresses) {
    if (!output) return false;
    
    if ((inclusiveStartAddress & 0x03) != 0) {
        warnings += "Inclusive start address 0x" + String(inclusiveStartAddress, HEX) + " is not on word boundary\n";
        return false;
    }
    if ((exclusiveEndAddress & 0x03) != 0) {
        warnings += "Exclusive end address 0x" + String(exclusiveEndAddress, HEX) + " is not on word boundary\n";
        return false;
    }
    
    output.println("#include <stdint.h>");
    output.print("uint32_t appStartAddress = 0x");
    output.print(formatHex(inclusiveStartAddress, 8));
    output.println(";");
    output.println("const uint32_t appImage[] = {");
    
    uint32_t rleStartAddress = inclusiveStartAddress;
    uint8_t b0, b1, b2;
    if (!readByte(inclusiveStartAddress, b0) || !readByte(inclusiveStartAddress + 1, b1) || !readByte(inclusiveStartAddress + 2, b2)) {
        warnings += "Failed to read start address data\n";
        return false;
    }
    uint32_t currentData = b0 + ((uint32_t)b1 << 8) + ((uint32_t)b2 << 16);
    
    for (uint32_t i = inclusiveStartAddress + 4; i < exclusiveEndAddress; i += 4) {
        if (!readByte(i, b0) || !readByte(i + 1, b1) || !readByte(i + 2, b2)) {
            warnings += "Failed to read address data at 0x" + formatHex(i, 8) + "\n";
            return false;
        }
        uint32_t newData = b0 + ((uint32_t)b1 << 8) + ((uint32_t)b2 << 16);
        
        if (newData == (currentData & 0xFFFFFF) && (currentData >> 24) != 0xFF) {
            currentData += (1 << 24);
        } else {
            output.print("0x");
            output.print(formatHex(currentData & 0xFFFFFFFF, 8));
            output.print(",");
            if (commentAddresses) {
                output.print(" // ");
                output.print(formatHex(rleStartAddress, 8));
            }
            output.println();
            rleStartAddress = i;
            currentData = newData;
        }
    }
    
    output.print("0x");
    output.print(formatHex(currentData & 0xFFFFFFFF, 8));
    output.print(",");
    if (commentAddresses) {
        output.print(" // ");
        output.print(formatHex(rleStartAddress, 8));
    }
    output.println();
    output.println("};");
    return true;
}

bool HexData::loadSW18BootloaderArray(FsFile& file) {
    if (!file) {
        warnings += "Invalid file handle\n";
        return false;
    }
    
    char line[256];
    uint32_t address = 0xFFFFFFFF;
    bool foundAddress = false;
    
    while (file.available()) {
        int len = file.fgets(line, sizeof(line));
        if (len <= 0) continue;
        
        String lineStr = String(line);
        lineStr.trim();
        
        if (!foundAddress) {
            int startIdx = lineStr.indexOf("appStartAddress");
            if (startIdx >= 0) {
                int hexIdx = lineStr.indexOf("0x");
                if (hexIdx >= 0) {
                    String addrStr = lineStr.substring(hexIdx + 2);
                    int endIdx = addrStr.indexOf(";");
                    if (endIdx >= 0) {
                        addrStr = addrStr.substring(0, endIdx);
                    }
                    addrStr.trim();
                    address = hexToVal(addrStr);
                    foundAddress = true;
                    continue;
                }
            }
        } else {
            int hexIdx = lineStr.indexOf("0x");
            if (hexIdx >= 0) {
                int commaIdx = lineStr.indexOf(",", hexIdx);
                if (commaIdx >= 0) {
                    String dataStr = lineStr.substring(hexIdx + 2, commaIdx);
                    dataStr.trim();
                    uint32_t data = hexToVal(dataStr);
                    int count = (int)(data >> 24) + 1;
                    data &= 0x00FFFFFF;
                    for (int i = 0; i < count; i++) {
                        memory[address++] = (uint8_t)(data & 0xFF);
                        memory[address++] = (uint8_t)((data >> 8) & 0xFF);
                        memory[address++] = (uint8_t)((data >> 16) & 0xFF);
                        memory[address++] = 0;
                    }
                }
            }
        }
    }
    
    return foundAddress;
}

bool HexData::twoColumn(FsFile& output) {
    if (!output) return false;
    
    for (auto const& pair : memory) {
        output.print(formatHex(pair.first, 0));
        output.print(" ");
        output.println(formatHex(pair.second, 2));
    }
    return true;
}

uint32_t HexData::getHighestAddress() {
    if (memory.empty()) return 0;
    return memory.rbegin()->first;
}

uint32_t HexData::getLowestAddress() {
    if (memory.empty()) return 0;
    return memory.begin()->first;
}

size_t HexData::getMemorySize() {
    return memory.size();
}

String HexData::getWarnings() {
    return warnings;
}

void HexData::clearWarnings() {
    warnings = "";
}
