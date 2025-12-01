#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <SD.h>

// --- WIFI CONFIGURATION ---
const char* ssid     = "Tenda_7B7E00";
const char* password = "ENTER_WIFI_PASSWORD";

// --- PIN DEFINITIONS ---
#define SD_CS_PIN   4  // D2
#define TFT_CS      15 // D8 
#define TFT_RST     16 // D0 
#define TFT_DC      5  // D1

// --- COLOR DEFINITIONS ---
#define ST77XX_GRAY 0x8410 

// Initialize Screen
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// --- GLOBAL VARIABLES ---
String currentEng = "";
String currentGer = "";
long totalWords = 0;      
long currentIndex = 0;    
bool sdAvailable = false;

// --- AUTOMATION GLOBALS ---
String currentFileName = "/words.txt"; 
String masterFileName = "/words.txt";
bool autoMode = false;
unsigned long autoInterval = 3000; 
unsigned long lastAutoUpdate = 0;

ESP8266WebServer server(80);

// ==========================================
//   HELPER FUNCTIONS 
// ==========================================

uint16_t read16(File &f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); 
  ((uint8_t *)&result)[1] = f.read(); 
  return result;
}

uint32_t read32(File &f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); 
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); 
  return result;
}

// ==========================================
//   CORE LOGIC (MEMORY FIXED)
// ==========================================

void appendToFile(String filename, String e, String g) {
  if (!sdAvailable) return;
  if (!filename.startsWith("/")) filename = "/" + filename;
  
  File file = SD.open(filename, FILE_WRITE);
  if (file) {
    file.print(e); file.print(","); file.println(g); 
    file.close();
    Serial.println("Saved to " + filename);
  }
}

void countTotalWords(String filename) {
  if (!sdAvailable) return;
  if (!filename.startsWith("/")) filename = "/" + filename;

  File file = SD.open(filename, FILE_READ);
  if (!file) {
    totalWords = 0;
    return;
  }

  long count = 0;
  while (file.available()) {
    // Memory Optimization: Don't save string, just find newline
    char c = file.read();
    if (c == '\n') count++;
  }
  file.close();
  totalWords = count;
}

// *** THIS IS THE FIX ***
void loadWordAtIndex(String filename, long index) {
  if (!sdAvailable || totalWords == 0) return;
  if (!filename.startsWith("/")) filename = "/" + filename;

  File file = SD.open(filename, FILE_READ);
  if (!file) return;

  // MEMORY FIX: Skip lines by reading bytes, NOT creating Strings
  for (long i = 0; i < index; i++) {
    while (file.available()) {
      char c = file.read();
      if (c == '\n') break; // Found end of line, stop skipping
    }
  }

  // Now we are at the right line, read the data safely
  if (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    int separatorIndex = line.indexOf(',');
    
    // Safety check: ensure line is not empty
    if (line.length() > 0) {
      if (separatorIndex > 0) {
        currentEng = line.substring(0, separatorIndex);
        currentGer = line.substring(separatorIndex + 1);
      } else {
        currentEng = line; 
        currentGer = ""; 
      }
    } else {
      // If reading failed (EOF), keep previous or show error
      // currentEng = "Read Error"; 
    }
  }
  file.close();
}

void initFile(String filename) {
  if (!sdAvailable) return;
  
  if (filename == masterFileName && !SD.exists(filename)) {
    appendToFile(masterFileName, "Hello", "Hallo");
    appendToFile(masterFileName, "Water", "Wasser");
  }

  currentFileName = filename;
  countTotalWords(filename); 
  currentIndex = 0;          
  loadWordAtIndex(currentFileName, currentIndex); 
}

// ==========================================
//   UI & WEB
// ==========================================

void drawSmartText(String text, int y, uint16_t color) {
  int len = text.length();
  int size = 3;
  if (len > 8) size = 2;   
  if (len > 13) size = 1;  
  
  tft.setTextSize(size);
  tft.setTextColor(color);
  int textWidth = len * 6 * size;
  int x = (tft.width() - textWidth) / 2;
  if (x < 0) x = 0;
  tft.setCursor(x, y);
  tft.println(text);
}

void centerTextFixed(String text, int y, int size, uint16_t color) {
  tft.setTextSize(size);
  tft.setTextColor(color);
  int textWidth = text.length() * 6 * size;
  int x = (tft.width() - textWidth) / 2;
  if (x < 0) x = 0;
  tft.setCursor(x, y);
  tft.println(text);
}

void updateScreen() {
  tft.fillScreen(ST77XX_BLACK);
  
  // Status Dots
  if(sdAvailable) tft.fillCircle(155, 5, 2, ST77XX_GREEN); 
  else tft.fillCircle(155, 5, 2, ST77XX_RED);

  if(autoMode) {
    tft.setTextColor(ST77XX_MAGENTA);
    tft.setTextSize(1);
    tft.setCursor(2, 2);
    tft.print("A");
  }

  if (totalWords == 0) {
    centerTextFixed("Empty File", 50, 2, ST77XX_RED);
    return;
  }
  
  // Counter
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_GRAY); 
  tft.setCursor(5, 5);
  tft.print(String(currentIndex + 1) + "/" + String(totalWords));

  // EN
  centerTextFixed("EN", 15, 1, ST77XX_CYAN);
  drawSmartText(currentEng, 30, ST77XX_WHITE);

  // Divider
  tft.drawFastHLine(10, 64, 140, ST77XX_WHITE);

  // DE
  centerTextFixed("DE", 75, 1, ST77XX_GREEN);
  drawSmartText(currentGer, 90, ST77XX_YELLOW);
}

// BMP Helper (Standard)
#define BUFFPIXEL 20
void bmpDraw(const char *filename, uint8_t x, uint16_t y) {
  File     bmpFile;
  int      bmpWidth, bmpHeight;   
  uint8_t  bmpDepth;              
  uint32_t bmpImageoffset;        
  uint32_t rowSize;               
  uint8_t  sdbuffer[3*BUFFPIXEL]; 
  uint8_t  buffidx = sizeof(sdbuffer); 
  boolean  goodBmp = false;       
  boolean  flip    = true;        
  int      w, h, row, col;
  uint8_t  r, g, b;
  uint32_t pos = 0;

  if((x >= tft.width()) || (y >= tft.height())) return;

  if (!(bmpFile = SD.open(filename))) return;

  if(read16(bmpFile) == 0x4D42) { 
    (void)read32(bmpFile); (void)read32(bmpFile); 
    bmpImageoffset = read32(bmpFile); 
    (void)read32(bmpFile);
    bmpWidth  = read32(bmpFile);
    bmpHeight = read32(bmpFile);
    if(read16(bmpFile) == 1) { 
      bmpDepth = read16(bmpFile); 
      if((bmpDepth == 24) && (read32(bmpFile) == 0)) { 
        goodBmp = true; 
        rowSize = (bmpWidth * 3 + 3) & ~3;
        if(bmpHeight < 0) { bmpHeight = -bmpHeight; flip = false; }
        w = bmpWidth; h = bmpHeight;
        if((x+w-1) >= tft.width())  w = tft.width()  - x;
        if((y+h-1) >= tft.height()) h = tft.height() - y;
        for (row=0; row<h; row++) { 
          if(flip) pos = bmpImageoffset + (bmpHeight - 1 - row) * rowSize;
          else     pos = bmpImageoffset + row * rowSize;
          if(bmpFile.position() != pos) bmpFile.seek(pos);
          for (col=0; col<w; col++) { 
            if (buffidx >= sizeof(sdbuffer)) { 
              bmpFile.read(sdbuffer, sizeof(sdbuffer));
              buffidx = 0; 
            }
            b = sdbuffer[buffidx++]; g = sdbuffer[buffidx++]; r = sdbuffer[buffidx++];
            tft.drawPixel(x+col, y+row, tft.color565(r,g,b));
          } 
        } 
      } 
    }
  }
  bmpFile.close();
}

String getFileListHTML() {
  String html = "<h3>Select List:</h3><div class='file-list'>";
  File root = SD.open("/");
  while (true) {
    File entry = root.openNextFile();
    if (!entry) break; 
    if (!entry.isDirectory()) {
      String fname = String(entry.name());
      if (fname.endsWith(".txt") || fname.endsWith(".TXT")) {
        String style = (String("/") + fname == currentFileName) ? "background:#4CAF50;" : "";
        html += "<a href='/selectFile?name=" + fname + "'><button class='file-btn' style='" + style + "'>" + fname + "</button></a>";
      }
    }
    entry.close();
  }
  root.close();
  html += "</div>";
  return html;
}

String getWebPage() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<style>";
  html += "body { font-family: sans-serif; text-align: center; background-color: #222; color: white; margin: 0; padding: 20px; }";
  html += ".btn { display: inline-block; width: 40%; padding: 15px; margin: 5px; border-radius: 10px; border: none; cursor: pointer; color: white; font-size: 16px; }";
  html += ".prev { background-color: #f44336; } .next { background-color: #4CAF50; }"; 
  html += ".input-group { margin: 20px auto; width: 95%; padding: 15px; background: #333; border-radius: 10px; box-sizing: border-box; }";
  html += ".file-btn { display: block; width: 90%; margin: 5px auto; padding: 10px; background: #555; color: white; border: none; border-radius: 5px; cursor: pointer; text-align: left; }";
  html += "</style></head><body>";
  
  html += "<h1>Word Learner</h1>";
  html += "<div style='color:#888'>File: " + currentFileName + "<br>Word: " + String(currentIndex + 1) + " / " + String(totalWords) + "</div>";
  
  if (totalWords > 0) {
    html += "<h2 style='color:#ffeb3b;'>" + currentEng + "</h2>";
    html += "<h3 style='color:#4CAF50;'>" + currentGer + "</h3>";
  } else {
    html += "<h2>(Empty List)</h2>";
  }
  
  if (!autoMode) {
    html += "<a href=\"/prev\"><button class=\"btn prev\">< PREV</button></a>";
    html += "<a href=\"/next\"><button class=\"btn next\">NEXT ></button></a>";
  } else {
    html += "<div style='padding:15px; background:#2e4430; border-radius:10px; color:#4CAF50;'>AUTO MODE ACTIVE</div>";
  }
  
  html += "<div class='input-group'><h3>Automation</h3>";
  html += "<form action='/setMode' method='GET'>";
  if(autoMode) {
      html += "<input type='hidden' name='auto' value='0'>";
      html += "<input type='submit' value='STOP AUTO' class='btn prev' style='width:90%'>";
  } else {
      html += "<input type='hidden' name='auto' value='1'>";
      html += "Interval (sec): <input type='number' name='sec' value='" + String(autoInterval/1000) + "' style='width:50px'>";
      html += "<input type='submit' value='START AUTO' class='btn next'>";
  }
  html += "</form></div>";
  
  html += "<div class='input-group'><h3>Add New Word</h3>";
  html += "<form action='/add' method='GET'>";
  html += "<input type='text' name='eng' placeholder='English' required><br>";
  html += "<input type='text' name='ger' placeholder='German' required><br>";
  html += "<input type='submit' value='SAVE' class='btn' style='background-color:#2196F3; width:90%; color:white;'></form>";
  html += "</div>";

  html += "<div class='input-group'><h3>File Manager</h3>";
  html += "<form action='/createFile' method='GET'>";
  html += "<input type='text' name='name' placeholder='New list name' required>";
  html += "<input type='submit' value='CREATE' class='btn' style='background-color:#FF9800; color:white;'></form>";
  html += getFileListHTML();
  html += "</div>";
  
  html += "</body></html>";
  return html;
}

void setup() {
  Serial.begin(115200);
  delay(100);
  
  pinMode(SD_CS_PIN, OUTPUT); digitalWrite(SD_CS_PIN, HIGH); 
  pinMode(TFT_CS, OUTPUT);    digitalWrite(TFT_CS, HIGH);

  tft.initR(INITR_BLACKTAB); 
  tft.setRotation(1); 
  tft.fillScreen(ST77XX_BLUE); 
  centerTextFixed("BOOTING...", 60, 2, ST77XX_WHITE);
  delay(500);

  if (!SD.begin(SD_CS_PIN)) {
    tft.fillScreen(ST77XX_RED); 
    centerTextFixed("SD FAIL", 60, 2, ST77XX_WHITE);
    sdAvailable = false;
    delay(2000); 
  } else {
    sdAvailable = true;
    if(SD.exists("logo.bmp")) {
      tft.fillScreen(ST77XX_BLACK);
      bmpDraw("logo.bmp", 0, 0);
      delay(3000);
    }
    initFile(masterFileName);
  }

  tft.fillScreen(ST77XX_BLACK); 
  centerTextFixed("Connecting...", 60, 1, ST77XX_WHITE);
  
  WiFi.begin(ssid, password);
  int retryCount = 0;
  while (WiFi.status() != WL_CONNECTED && retryCount < 20) {
    delay(500);
    retryCount++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    tft.fillScreen(ST77XX_BLACK);
    centerTextFixed("IP Address:", 40, 1, ST77XX_GREEN);
    centerTextFixed(WiFi.localIP().toString(), 60, 2, ST77XX_MAGENTA);
    delay(3000); 
  } else {
    centerTextFixed("WiFi Fail!", 60, 2, ST77XX_RED);
    delay(2000);
  }

  updateScreen();

  server.on("/", []() { server.send(200, "text/html", getWebPage()); });
  
  server.on("/next", []() {
    if(!autoMode && totalWords > 0) { 
      currentIndex++; 
      if (currentIndex >= totalWords) currentIndex = 0; 
      loadWordAtIndex(currentFileName, currentIndex);
      updateScreen(); 
    }
    server.sendHeader("Location", "/"); server.send(303);
  });
  
  server.on("/prev", []() {
    if(!autoMode && totalWords > 0) { 
      currentIndex--; 
      if (currentIndex < 0) currentIndex = totalWords - 1; 
      loadWordAtIndex(currentFileName, currentIndex);
      updateScreen(); 
    }
    server.sendHeader("Location", "/"); server.send(303);
  });

  server.on("/add", []() {
    String e = server.arg("eng"); String g = server.arg("ger");
    if (e.length() > 0 && g.length() > 0) {
      appendToFile(masterFileName, e, g);
      if (currentFileName != masterFileName) appendToFile(currentFileName, e, g);
      initFile(currentFileName);
      currentIndex = totalWords - 1; 
      loadWordAtIndex(currentFileName, currentIndex);
      updateScreen();
    }
    server.sendHeader("Location", "/"); server.send(303);
  });

  server.on("/setMode", []() {
    String autoStr = server.arg("auto");
    if (autoStr == "1") {
      autoMode = true;
      String secStr = server.arg("sec");
      if (secStr.toInt() > 0) autoInterval = secStr.toInt() * 1000;
    } else {
      autoMode = false;
    }
    updateScreen(); 
    server.sendHeader("Location", "/"); server.send(303);
  });

  server.on("/selectFile", []() {
    String fname = server.arg("name");
    if (fname.length() > 0) {
      initFile(fname); 
      updateScreen();
    }
    server.sendHeader("Location", "/"); server.send(303);
  });

  server.on("/createFile", []() {
    String fname = server.arg("name");
    if (fname.length() > 0) {
      if (!fname.endsWith(".txt")) fname += ".txt";
      if (!fname.startsWith("/")) fname = "/" + fname;
      if (!SD.exists(fname)) {
        File f = SD.open(fname, FILE_WRITE);
        if (f) f.close(); 
        initFile(fname);
        updateScreen();
      }
    }
    server.sendHeader("Location", "/"); server.send(303);
  });

  server.begin();
}

void loop() {
  server.handleClient();
  
  if (autoMode && totalWords > 0) {
    if (millis() - lastAutoUpdate > autoInterval) {
      lastAutoUpdate = millis();
      currentIndex++;
      if (currentIndex >= totalWords) currentIndex = 0;
      loadWordAtIndex(currentFileName, currentIndex);
      updateScreen();
    }
  }
}