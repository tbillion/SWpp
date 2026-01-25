# ESP32 Post-Processing Utility

Complete port of the Serial Wombat HEX post-processing utility to ESP32 with LVGL GUI and SD card storage.

## Hardware Requirements

- **Board**: ESP32-32E (WROOM-32E) with 3.5" Display
- **Display**: 3.5 inch TFT, 320x480 resolution, ILI9488 driver
- **Storage**: MicroSD card (FAT32 formatted)
- **Reference**: [LCDWiki ESP32-32E 3.5" Display](https://www.lcdwiki.com/3.5inch_ESP32-32E_Display)

### Hardware Pinout

#### RGB LED (Status Indicator)
- Red LED: GPIO 4
- Green LED: GPIO 16
- Blue LED: GPIO 17

#### SD Card Interface (SPI)
- MISO: GPIO 2
- MOSI: GPIO 15
- CLK: GPIO 14
- CS: GPIO 13

#### TFT Display (SPI)
- MOSI: GPIO 23
- MISO: GPIO 19
- CLK: GPIO 18
- CS: GPIO 5
- RST: GPIO 33
- DC: GPIO 27

## Features

This application provides complete Intel HEX file post-processing with:

1. **Intel HEX File Parsing**: Full support for Intel HEX format with extended addressing
2. **CRC-16-CCITT Calculation**: Industry-standard CRC computation over memory ranges
3. **Memory Operations**: Fill, crop, and manipulate firmware memory images
4. **Multiple Output Formats**:
   - CRCed_Full.hex: Complete processed firmware
   - CRCed_App.hex: Cropped application area
   - CRCed_App_rle.c: RLE-compressed C array for bootloader
   - rleCheck.hex: Verification file from RLE array
5. **LVGL Graphical Interface**: User-friendly touch interface for file selection and processing
6. **RGB LED Status Indication**:
   - Yellow (static): Loading file
   - Red (breathing): Processing/busy
   - Green (static): Waiting for user input
7. **Real-time Progress**: Visual progress bar and status updates
8. **Error Handling**: Comprehensive error checking with user feedback

## Software Requirements

### PlatformIO (Recommended)
- PlatformIO Core 6.1.0 or later
- Platform: espressif32
- Framework: Arduino

### Arduino IDE (Alternative)
- Arduino IDE 1.8.19 or later
- ESP32 Board Support Package 2.0.0 or later

### Required Libraries
- TFT_eSPI 2.5.43 or later
- LVGL 8.3.11
- SdFat 2.2.2 or later

## Installation

### Using PlatformIO (Recommended)

1. Clone or download this repository
2. Open the ESP32_PostProcessing folder in PlatformIO
3. Connect your ESP32-32E board via USB
4. Build and upload:
   ```bash
   pio run -t upload
   ```
5. Monitor serial output:
   ```bash
   pio device monitor
   ```

### Using Arduino IDE

1. Install the ESP32 board support:
   - Go to File → Preferences
   - Add to Additional Board Manager URLs: `https://dl.espressif.com/dl/package_esp32_index.json`
   - Go to Tools → Board → Boards Manager
   - Search for "ESP32" and install

2. Install required libraries via Library Manager:
   - TFT_eSPI by Bodmer
   - lvgl by LVGL
   - SdFat by Bill Greiman

3. Copy the contents of `include/config.h` to define the hardware pins

4. Open ESP32_PostProcessing.ino in Arduino IDE

5. Select board: Tools → Board → ESP32 Arduino → ESP32 Dev Module

6. Configure settings:
   - Upload Speed: 921600
   - CPU Frequency: 240MHz
   - Flash Frequency: 80MHz
   - Flash Mode: QIO
   - Flash Size: 4MB
   - Partition Scheme: Default 4MB with spiffs

7. Upload the sketch

## Usage

### Preparing SD Card

1. Format a microSD card as FAT32
2. Copy your Intel HEX files (.hex) to the root directory of the SD card
3. Insert the SD card into the ESP32-32E module

### Operating the Application

1. Power on the ESP32-32E module
2. The RGB LED will light up yellow during initialization, then green when ready
3. The display shows a list of available HEX files on the SD card
4. Tap on a HEX file to select it
5. The "Process File" button will become enabled
6. Tap "Process File" to begin processing
7. The RGB LED will breathe red during processing
8. Progress is shown on the progress bar
9. When complete:
   - The RGB LED turns green
   - Processing results and CRC value are displayed
   - Output files are created on the SD card with the same base name

### LED Status Indicators

- **Yellow (Static)**: Loading or initializing
- **Red (Breathing)**: Actively processing file
- **Green (Static)**: Ready/waiting for user input or processing complete

### Output Files

For an input file named `firmware.hex`, the following files are created:

- `firmware_CRCed_Full.hex`: Complete firmware with CRC and magic numbers
- `firmware_CRCed_App.hex`: Cropped application area only
- `firmware_CRCed_App_rle.c`: C source file with RLE-compressed bootloader array
- `firmware_rleCheck.hex`: Verification file reconstructed from RLE array

## Processing Details

The post-processing utility performs the following operations:

1. **Load**: Parses Intel HEX file into memory map
2. **Fill**: Fills memory region (0x8000 to 0x3FFFF) with 0x00FFFFFF pattern
3. **Magic Numbers**: Sets programming indicator at 0x1F800:
   - Byte 0: 0x23
   - Byte 1: 0xCD
   - Byte 2: 0x00
4. **CRC Calculation**: Computes CRC-16-CCITT over range 0x8000 to 0x3F000
5. **CRC Storage**: Stores CRC at address 0x1F804 (little-endian)
6. **Export**: Generates multiple output formats for bootloader use
7. **Verification**: Re-reads RLE array and generates verification HEX file

## Code Structure

```
ESP32_PostProcessing/
├── ESP32_PostProcessing.ino  Main application with LVGL UI
├── platformio.ini             PlatformIO configuration
├── include/
│   ├── config.h              Hardware pin definitions
│   ├── lv_conf.h             LVGL configuration
│   ├── RGBLed.h              RGB LED controller header
│   └── IntelHex.h            Intel HEX parser header
├── src/
│   ├── RGBLed.cpp            RGB LED breathing effect implementation
│   └── IntelHex.cpp          Complete HEX processing implementation
└── README.md                 This file
```

## Memory Specifications

- **Hex Memory Map**: Up to 256KB addressable space
- **Display Buffer**: 25.6KB (320 x 40 pixels x 2 bytes)
- **LVGL Memory**: 48KB heap allocation
- **Free Heap**: Approximately 150KB+ available on ESP32

## Troubleshooting

### SD Card Not Detected
- Ensure SD card is FAT32 formatted
- Check SD card is properly inserted
- Verify wiring connections match pinout
- Try different SD card

### Display Issues
- Verify TFT pins match configuration
- Check power supply is adequate (5V/1A minimum)
- Ensure SPI bus is not conflicting

### Processing Errors
- Verify HEX file is valid Intel HEX format
- Check sufficient free space on SD card
- Review serial monitor for detailed error messages

### LED Not Working
- Verify RGB LED pins are correct
- Check LED is common cathode type
- Ensure PWM channels are not in use by other peripherals

## Serial Monitor Output

Connect to serial port at 115200 baud to see detailed processing information:
- File selection confirmations
- Processing progress
- CRC calculation results
- Warning messages
- Completion status

## Compatibility

This port maintains full compatibility with the original C# post-processing utility:
- Identical CRC calculation algorithm (CRC-16-CCITT)
- Same memory addressing and layout
- Equivalent output file formats
- Compatible with Serial Wombat bootloader

## License

This code is provided as a complete, production-ready port of the Serial Wombat post-processing utility. It follows the same MIT License as the parent Serial Wombat project.

## Credits

- **Original C# Utility**: Serial Wombat project by Broadwell Consulting Inc.
- **ESP32 Port**: Complete rewrite in C++ for embedded ESP32 platform
- **Hardware**: ESP32-32E module from LCDWiki
- **Libraries**: TFT_eSPI (Bodmer), LVGL (LVGL team), SdFat (Bill Greiman)

## Support

For issues specific to the ESP32 port, check:
1. Hardware connections match documented pinout
2. SD card is properly formatted and contains valid HEX files
3. All required libraries are installed at specified versions
4. Serial monitor output for detailed error information

For Serial Wombat firmware and original utility questions, refer to the main Serial Wombat repository.
