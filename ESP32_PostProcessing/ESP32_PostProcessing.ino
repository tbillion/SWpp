#include <Arduino.h>
#include <SPI.h>
#include <SdFat.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include "config.h"
#include "RGBLed.h"
#include "IntelHex.h"

TFT_eSPI tft = TFT_eSPI();
SdFat sd;
RGBLed rgbLed;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[TFT_WIDTH * 40];
static lv_disp_drv_t disp_drv;

lv_obj_t* mainScreen;
lv_obj_t* fileList;
lv_obj_t* processBtn;
lv_obj_t* statusLabel;
lv_obj_t* progressBar;
lv_obj_t* crcLabel;
lv_obj_t* resultLabel;

String selectedFile = "";
bool processingActive = false;

void displayFlush(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t*)&color_p->full, w * h, true);
    tft.endWrite();
    
    lv_disp_flush_ready(disp);
}

void setupDisplay() {
    tft.begin();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);
    
    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, TFT_WIDTH * 40);
    
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = TFT_WIDTH;
    disp_drv.ver_res = TFT_HEIGHT;
    disp_drv.flush_cb = displayFlush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);
}

void setupSD() {
    SPI.begin(SD_CLK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
    
    if (!sd.begin(SD_CS_PIN, SD_SCK_MHZ(4))) {
        Serial.println("SD card initialization failed!");
        lv_obj_t* errorLabel = lv_label_create(lv_scr_act());
        lv_label_set_text(errorLabel, "SD Card Error!\nInsert SD card and reset.");
        lv_obj_align(errorLabel, LV_ALIGN_CENTER, 0, 0);
        while (true) {
            delay(1000);
        }
    }
}

void scanHexFiles() {
    FsFile root;
    if (!root.open("/")) {
        Serial.println("Failed to open root directory");
        return;
    }
    
    FsFile file;
    while (file.openNext(&root, O_RDONLY)) {
        char fileName[64];
        file.getName(fileName, sizeof(fileName));
        String name = String(fileName);
        if (!file.isDirectory() && (name.endsWith(".hex") || name.endsWith(".HEX"))) {
            lv_list_add_btn(fileList, LV_SYMBOL_FILE, fileName);
        }
        file.close();
    }
    root.close();
}

void fileListEventHandler(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* obj = lv_event_get_target(e);
    
    if (code == LV_EVENT_CLICKED) {
        selectedFile = String(lv_list_get_btn_text(fileList, obj));
        lv_label_set_text_fmt(statusLabel, "Selected: %s", selectedFile.c_str());
        lv_obj_clear_state(processBtn, LV_STATE_DISABLED);
        rgbLed.setStatus(LED_WAITING);
        Serial.print("File selected: ");
        Serial.println(selectedFile);
    }
}

void processHexFile() {
    if (selectedFile.length() == 0 || processingActive) return;
    
    processingActive = true;
    rgbLed.setStatus(LED_LOADING);
    lv_label_set_text(statusLabel, "Loading HEX file...");
    lv_bar_set_value(progressBar, 10, LV_ANIM_ON);
    lv_task_handler();
    
    FsFile hexFile;
    if (!hexFile.open(selectedFile.c_str(), O_RDONLY)) {
        lv_label_set_text_fmt(resultLabel, "#FF0000 Error:# Failed to open %s", selectedFile.c_str());
        processingActive = false;
        rgbLed.setStatus(LED_WAITING);
        return;
    }
    
    HexData hexData;
    bool loaded = hexData.load(hexFile, true);
    hexFile.close();
    
    if (!loaded) {
        lv_label_set_text_fmt(resultLabel, "#FF0000 Error:# Failed to parse HEX file\n%s", hexData.getWarnings().c_str());
        processingActive = false;
        rgbLed.setStatus(LED_WAITING);
        return;
    }
    
    lv_label_set_text(statusLabel, "Processing...");
    lv_bar_set_value(progressBar, 30, LV_ANIM_ON);
    rgbLed.setStatus(LED_BUSY);
    lv_task_handler();
    
    uint32_t baseAddr = 0x4000;
    uint32_t length = 0x20000 - 0x4000;
    baseAddr *= 2;
    length *= 2;
    
    hexData.fill32(baseAddr, baseAddr + length, 0x00FFFFFF);
    lv_bar_set_value(progressBar, 50, LV_ANIM_ON);
    lv_task_handler();
    
    hexData.writeByte(0x1F800 * 2, 0x23);
    hexData.writeByte(0x1F800 * 2 + 1, 0xCD);
    hexData.writeByte(0x1F800 * 2 + 2, 0x00);
    
    uint16_t crc = hexData.crc16ccitt(0x8000, 0x1F800 * 2);
    lv_bar_set_value(progressBar, 70, LV_ANIM_ON);
    lv_task_handler();
    
    hexData.writeByte(0x1F804 * 2 + 1, (uint8_t)(crc >> 8));
    hexData.writeByte(0x1F804 * 2, (uint8_t)(crc & 0xFF));
    
    lv_label_set_text_fmt(crcLabel, "CRC: 0x%04X (%u)", crc, crc);
    
    String basePath = selectedFile.substring(0, selectedFile.lastIndexOf('.'));
    
    lv_label_set_text(statusLabel, "Writing CRCed_Full.hex...");
    lv_task_handler();
    FsFile outFile;
    if (outFile.open((basePath + "_CRCed_Full.hex").c_str(), O_WRONLY | O_CREAT | O_TRUNC)) {
        uint32_t highest = hexData.getHighestAddress();
        hexData.toHexFileString(outFile, 0, highest + 1);
        outFile.close();
    }
    lv_bar_set_value(progressBar, 80, LV_ANIM_ON);
    lv_task_handler();
    
    hexData.crop(baseAddr, baseAddr + length);
    
    lv_label_set_text(statusLabel, "Writing CRCed_App.hex...");
    lv_task_handler();
    if (outFile.open((basePath + "_CRCed_App.hex").c_str(), O_WRONLY | O_CREAT | O_TRUNC)) {
        hexData.toHexFileString(outFile, baseAddr, baseAddr + length + 1);
        outFile.close();
    }
    
    lv_label_set_text(statusLabel, "Writing CRCed_App_rle.c...");
    lv_task_handler();
    if (outFile.open((basePath + "_CRCed_App_rle.c").c_str(), O_WRONLY | O_CREAT | O_TRUNC)) {
        hexData.toSW18BootloaderArray(outFile, baseAddr, baseAddr + length, false);
        outFile.close();
    }
    lv_bar_set_value(progressBar, 90, LV_ANIM_ON);
    lv_task_handler();
    
    lv_label_set_text(statusLabel, "Verifying RLE output...");
    lv_task_handler();
    HexData rleCheck;
    if (outFile.open((basePath + "_CRCed_App_rle.c").c_str(), O_RDONLY)) {
        rleCheck.loadSW18BootloaderArray(outFile);
        outFile.close();
        
        if (outFile.open((basePath + "_rleCheck.hex").c_str(), O_WRONLY | O_CREAT | O_TRUNC)) {
            rleCheck.toHexFileString(outFile, baseAddr, baseAddr + length + 1);
            outFile.close();
        }
    }
    
    lv_bar_set_value(progressBar, 100, LV_ANIM_ON);
    lv_label_set_text(statusLabel, "Processing complete!");
    lv_label_set_text_fmt(resultLabel, "#00FF00 Success!#\nFiles created:\n%s_CRCed_Full.hex\n%s_CRCed_App.hex\n%s_CRCed_App_rle.c\n%s_rleCheck.hex", 
                         basePath.c_str(), basePath.c_str(), basePath.c_str(), basePath.c_str());
    
    Serial.println("Processing complete!");
    Serial.print("CRC: 0x");
    Serial.print(crc, HEX);
    Serial.print(" (");
    Serial.print(crc);
    Serial.println(")");
    
    if (hexData.getWarnings().length() > 0) {
        Serial.println("Warnings:");
        Serial.println(hexData.getWarnings());
    }
    
    processingActive = false;
    rgbLed.setStatus(LED_WAITING);
}

void processBtnEventHandler(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        processHexFile();
    }
}

void createUI() {
    mainScreen = lv_obj_create(NULL);
    lv_scr_load(mainScreen);
    
    lv_obj_t* title = lv_label_create(mainScreen);
    lv_label_set_text(title, "HEX Post Processor");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    
    lv_obj_t* subtitle = lv_label_create(mainScreen);
    lv_label_set_text(subtitle, "Select a HEX file:");
    lv_obj_align(subtitle, LV_ALIGN_TOP_LEFT, 10, 40);
    
    fileList = lv_list_create(mainScreen);
    lv_obj_set_size(fileList, TFT_WIDTH - 20, 150);
    lv_obj_align(fileList, LV_ALIGN_TOP_MID, 0, 65);
    lv_obj_add_event_cb(fileList, fileListEventHandler, LV_EVENT_CLICKED, NULL);
    
    processBtn = lv_btn_create(mainScreen);
    lv_obj_set_size(processBtn, 200, 50);
    lv_obj_align(processBtn, LV_ALIGN_CENTER, 0, 10);
    lv_obj_add_state(processBtn, LV_STATE_DISABLED);
    lv_obj_add_event_cb(processBtn, processBtnEventHandler, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t* btnLabel = lv_label_create(processBtn);
    lv_label_set_text(btnLabel, "Process File");
    lv_obj_center(btnLabel);
    
    statusLabel = lv_label_create(mainScreen);
    lv_label_set_text(statusLabel, "Ready");
    lv_obj_align(statusLabel, LV_ALIGN_BOTTOM_MID, 0, -110);
    
    progressBar = lv_bar_create(mainScreen);
    lv_obj_set_size(progressBar, TFT_WIDTH - 20, 20);
    lv_obj_align(progressBar, LV_ALIGN_BOTTOM_MID, 0, -85);
    lv_bar_set_value(progressBar, 0, LV_ANIM_OFF);
    
    crcLabel = lv_label_create(mainScreen);
    lv_label_set_text(crcLabel, "CRC: ---");
    lv_obj_align(crcLabel, LV_ALIGN_BOTTOM_MID, 0, -60);
    
    resultLabel = lv_label_create(mainScreen);
    lv_label_set_recolor(resultLabel, true);
    lv_label_set_text(resultLabel, "");
    lv_obj_set_style_text_align(resultLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(resultLabel, LV_ALIGN_BOTTOM_MID, 0, -10);
    
    scanHexFiles();
}

void setup() {
    Serial.begin(115200);
    Serial.println("ESP32 HEX Post-Processing Utility");
    Serial.println("Hardware: ESP32-32E 3.5\" Display");
    
    rgbLed.begin();
    rgbLed.setStatus(LED_LOADING);
    
    setupDisplay();
    setupSD();
    createUI();
    
    rgbLed.setStatus(LED_WAITING);
    
    Serial.println("System ready. Select a HEX file to process.");
}

void loop() {
    lv_task_handler();
    rgbLed.update();
    delay(5);
}
