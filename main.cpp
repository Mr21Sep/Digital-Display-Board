#include <WiFi.h>
#include <WebServer.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

// Access Point credentials
const char* ap_ssid = "DigitalBoard_AP";     // WiFi name that ESP32 will create
const char* ap_password = "12345678";        // Password (minimum 8 characters)

// I2C LCD setup (address 0x27 is most common, 16 columns, 2 rows)
LiquidCrystal_I2C lcd(0x27, 16, 2);

WebServer server(80);

String displayMessage = "Digital Board Ready!";
unsigned long lastUpdate = 0;
int scrollPosition = 0;
bool isScrolling = false;

// Function declarations
void handleRoot();
void handleUpdate();
void handleClear();
void handleStatus();
void handleTest();
void updateDisplay();
void scrollDisplay();

void setup() {
  Serial.begin(115200);
  
  // Initialize I2C communication
  Wire.begin(21, 22); // SDA=21, SCL=22 for ESP32
  
  // Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Starting AP...");
  
  Serial.println("Initializing LCD...");
  
  // Create WiFi Access Point
  WiFi.softAP(ap_ssid, ap_password);
  
  // Get the IP address of the access point
  IPAddress apIP = WiFi.softAPIP();
  
  Serial.println("");
  Serial.println("Access Point Started!");
  Serial.print("Network Name (SSID): ");
  Serial.println(ap_ssid);
  Serial.print("Password: ");
  Serial.println(ap_password);
  Serial.print("IP address: ");
  Serial.println(apIP);
  
  // Display AP info on LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi: ");
  lcd.print(String(ap_ssid).substring(0, 10)); // Shortened for display
  lcd.setCursor(0, 1);
  lcd.print("IP: ");
  lcd.print(apIP);
  delay(5000);
  
  // Setup web server routes
  server.on("/", handleRoot);
  server.on("/update", HTTP_POST, handleUpdate);
  server.on("/clear", HTTP_GET, handleClear);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/test", HTTP_GET, handleTest);
  
  server.begin();
  Serial.println("HTTP server started");
  Serial.println("Connect to WiFi '" + String(ap_ssid) + "' and go to http://" + apIP.toString());
  
  // Initial display
  updateDisplay();
}

void loop() {
  server.handleClient();
  
  // Handle scrolling for long messages
  if (isScrolling && millis() - lastUpdate > 300) {
    scrollDisplay();
    lastUpdate = millis();
  }
}

void handleRoot() {
  String html = "<!DOCTYPE html>";
  html += "<html><head>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>Digital Display Board</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 20px; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); min-height: 100vh; }";
  html += ".container { max-width: 500px; margin: 0 auto; background: white; padding: 30px; border-radius: 15px; box-shadow: 0 10px 30px rgba(0,0,0,0.3); }";
  html += "h1 { color: #333; text-align: center; margin-bottom: 30px; font-size: 28px; }";
  html += ".status-card { background: #f8f9fa; padding: 15px; border-radius: 10px; margin: 20px 0; border-left: 4px solid #007bff; }";
  html += "input[type='text'] { width: 100%; padding: 15px; margin: 10px 0; box-sizing: border-box; border: 2px solid #ddd; border-radius: 8px; font-size: 16px; transition: border-color 0.3s; }";
  html += "input[type='text']:focus { border-color: #007bff; outline: none; }";
  html += ".btn { background: linear-gradient(45deg, #28a745, #20c997); color: white; padding: 15px 25px; border: none; border-radius: 8px; cursor: pointer; font-size: 16px; margin: 8px 0; width: 100%; transition: transform 0.2s; }";
  html += ".btn:hover { transform: translateY(-2px); box-shadow: 0 5px 15px rgba(0,0,0,0.2); }";
  html += ".btn-clear { background: linear-gradient(45deg, #dc3545, #e74c3c); }";
  html += ".btn-status { background: linear-gradient(45deg, #17a2b8, #138496); }";
  html += ".btn-test { background: linear-gradient(45deg, #ffc107, #fd7e14); }";
  html += ".network-info { background: #e8f4f8; padding: 15px; border-radius: 8px; margin: 15px 0; }";
  html += ".message-display { background: #000; color: #00ff00; font-family: 'Courier New', monospace; padding: 15px; border-radius: 8px; margin: 15px 0; font-size: 18px; text-align: center; min-height: 60px; display: flex; align-items: center; justify-content: center; }";
  html += ".lcd-frame { background: #2c5f41; padding: 10px; border-radius: 10px; margin: 15px 0; }";
  html += ".lcd-screen { background: #1a4d2e; padding: 5px; border-radius: 5px; }";
  html += "</style>";
  html += "</head><body>";
  html += "<div class='container'>";
  html += "<h1>📟 Digital Display Board (I2C)</h1>";
  
  // Network Information
  html += "<div class='network-info'>";
  html += "<strong>📶 Network Info:</strong><br>";
  html += "WiFi Name: <strong>" + String(ap_ssid) + "</strong><br>";
  html += "IP Address: <strong>" + WiFi.softAPIP().toString() + "</strong><br>";
  html += "Connected Devices: <strong>" + String(WiFi.softAPgetStationNum()) + "</strong>";
  html += "</div>";
  
  // LCD Simulator
  html += "<div class='lcd-frame'>";
  html += "<div class='lcd-screen'>";
  html += "<div class='message-display'>";
  if (displayMessage.length() > 0) {
    // Show first 32 characters (2 lines of 16 characters each)
    String line1 = displayMessage.substring(0, min(16, (int)displayMessage.length()));
    String line2 = "";
    if (displayMessage.length() > 16) {
      line2 = displayMessage.substring(16, min(32, (int)displayMessage.length()));
    }
    html += line1;
    if (line2.length() > 0) {
      html += "<br>" + line2;
    }
  } else {
    html += "[Display is empty]";
  }
  html += "</div>";
  html += "</div>";
  html += "</div>";
  
  // Message Input Form
  html += "<form action='/update' method='post'>";
  html += "<input type='text' name='message' placeholder='Enter your message here...' maxlength='200' required>";
  html += "<input type='submit' value='📤 Update Display' class='btn'>";
  html += "</form>";
  
  // Control Buttons
  html += "<button onclick=\"location.href='/clear'\" class='btn btn-clear'>🗑️ Clear Display</button>";
  html += "<button onclick=\"location.href='/test'\" class='btn btn-test'>🧪 Test Display</button>";
  html += "<button onclick=\"location.href='/status'\" class='btn btn-status'>📊 System Status</button>";
  
  // Instructions
  html += "<div class='status-card'>";
  html += "<strong>📱 How to Use:</strong><br>";
  html += "1. Make sure you're connected to <strong>" + String(ap_ssid) + "</strong><br>";
  html += "2. Type your message above<br>";
  html += "3. Click 'Update Display' to show it on the LCD<br>";
  html += "4. Use 'Test Display' to check if LCD is working properly<br>";
  html += "5. Messages longer than 32 characters will scroll automatically";
  html += "</div>";
  
  html += "</div>";
  html += "<script>";
  html += "setTimeout(function(){ location.reload(); }, 30000);"; // Auto refresh every 30 seconds
  html += "</script>";
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

void handleUpdate() {
  if (server.hasArg("message")) {
    displayMessage = server.arg("message");
    if (displayMessage.length() == 0) {
      displayMessage = "Empty Message";
    }
    updateDisplay();
    
    // Send response back to browser
    String response = "<!DOCTYPE html>";
    response += "<html><head>";
    response += "<meta http-equiv='refresh' content='2;url=/'>";
    response += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    response += "<style>body{font-family:Arial;text-align:center;margin-top:50px;background:linear-gradient(135deg, #667eea 0%, #764ba2 100%);color:white;}</style>";
    response += "</head><body>";
    response += "<div style='background:white;color:black;padding:30px;border-radius:15px;display:inline-block;margin-top:100px;'>";
    response += "<h2>✅ Message Updated Successfully!</h2>";
    response += "<p><strong>Now Displaying:</strong></p>";
    response += "<div style='background:#000;color:#00ff00;padding:15px;font-family:monospace;border-radius:5px;margin:10px 0;'>";
    response += displayMessage;
    response += "</div>";
    response += "<p>Redirecting back to control panel...</p>";
    response += "</div>";
    response += "</body></html>";
    
    server.send(200, "text/html", response);
  } else {
    server.send(400, "text/plain", "No message received");
  }
}

void handleClear() {
  displayMessage = "";
  lcd.clear();
  scrollPosition = 0;
  isScrolling = false;
  
  String response = "<!DOCTYPE html>";
  response += "<html><head>";
  response += "<meta http-equiv='refresh' content='2;url=/'>";
  response += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  response += "<style>body{font-family:Arial;text-align:center;margin-top:50px;background:linear-gradient(135deg, #667eea 0%, #764ba2 100%);color:white;}</style>";
  response += "</head><body>";
  response += "<div style='background:white;color:black;padding:30px;border-radius:15px;display:inline-block;margin-top:100px;'>";
  response += "<h2>🧹 Display Cleared!</h2>";
  response += "<p>The LCD screen has been cleared.</p>";
  response += "<p>Redirecting back to control panel...</p>";
  response += "</div>";
  response += "</body></html>";
  
  server.send(200, "text/html", response);
}

void handleTest() {
  // Test sequence
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Testing LCD...");
  lcd.setCursor(0, 1);
  lcd.print("Line 1 & Line 2");
  
  String response = "<!DOCTYPE html>";
  response += "<html><head>";
  response += "<meta http-equiv='refresh' content='3;url=/'>";
  response += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  response += "<style>body{font-family:Arial;text-align:center;margin-top:50px;background:linear-gradient(135deg, #667eea 0%, #764ba2 100%);color:white;}</style>";
  response += "</head><body>";
  response += "<div style='background:white;color:black;padding:30px;border-radius:15px;display:inline-block;margin-top:100px;'>";
  response += "<h2>🧪 Testing LCD Display</h2>";
  response += "<p>Test message sent to LCD:</p>";
  response += "<div style='background:#000;color:#00ff00;padding:15px;font-family:monospace;border-radius:5px;margin:10px 0;'>";
  response += "Testing LCD...<br>Line 1 & Line 2";
  response += "</div>";
  response += "<p>Check your LCD screen. Redirecting back...</p>";
  response += "</div>";
  response += "</body></html>";
  
  server.send(200, "text/html", response);
  
  // Restore original message after 3 seconds
  delay(3000);
  updateDisplay();
}

void handleStatus() {
  String response = "<!DOCTYPE html>";
  response += "<html><head>";
  response += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  response += "<title>System Status</title>";
  response += "<style>";
  response += "body { font-family: Arial, sans-serif; margin: 20px; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); min-height: 100vh; }";
  response += ".container { max-width: 500px; margin: 0 auto; background: white; padding: 30px; border-radius: 15px; box-shadow: 0 10px 30px rgba(0,0,0,0.3); }";
  response += ".status-item { background: #f8f9fa; padding: 15px; margin: 10px 0; border-radius: 8px; border-left: 4px solid #28a745; }";
  response += ".btn { background: linear-gradient(45deg, #6c757d, #5a6268); color: white; padding: 12px 20px; border: none; border-radius: 8px; cursor: pointer; text-decoration: none; display: inline-block; margin-top: 20px; }";
  response += "</style>";
  response += "</head><body>";
  response += "<div class='container'>";
  response += "<h1>📊 System Status</h1>";
  
  response += "<div class='status-item'>";
  response += "<strong>🌐 Network Status:</strong><br>";
  response += "Access Point: " + String(ap_ssid) + "<br>";
  response += "IP Address: " + WiFi.softAPIP().toString() + "<br>";
  response += "Connected Devices: " + String(WiFi.softAPgetStationNum());
  response += "</div>";
  
  response += "<div class='status-item'>";
  response += "<strong>💾 Memory Status:</strong><br>";
  response += "Free Heap: " + String(ESP.getFreeHeap()) + " bytes<br>";
  response += "Chip Model: " + String(ESP.getChipModel()) + "<br>";
  response += "CPU Frequency: " + String(ESP.getCpuFreqMHz()) + " MHz";
  response += "</div>";
  
  response += "<div class='status-item'>";
  response += "<strong>📺 LCD Status:</strong><br>";
  response += "Type: I2C LCD (16x2)<br>";
  response += "I2C Address: 0x27<br>";
  response += "SDA Pin: GPIO 21<br>";
  response += "SCL Pin: GPIO 22";
  response += "</div>";
  
  response += "<div class='status-item'>";
  response += "<strong>📝 Display Status:</strong><br>";
  response += "Current Message Length: " + String(displayMessage.length()) + " characters<br>";
  response += "Scrolling: " + String(isScrolling ? "Yes" : "No") + "<br>";
  response += "Message: " + (displayMessage.length() > 0 ? displayMessage : "[Empty]");
  response += "</div>";
  
  response += "<div class='status-item'>";
  response += "<strong>⏱️ Uptime:</strong><br>";
  response += "Running for: " + String(millis() / 1000) + " seconds";
  response += "</div>";
  
  response += "<a href='/' class='btn'>← Back to Control Panel</a>";
  response += "</div>";
  response += "</body></html>";
  
  server.send(200, "text/html", response);
}

void updateDisplay() {
  lcd.clear();
  scrollPosition = 0;
  
  if (displayMessage.length() == 0) {
    return;
  }
  
  if (displayMessage.length() <= 16) {
    // Short message - display normally
    lcd.setCursor(0, 0);
    lcd.print(displayMessage);
    isScrolling = false;
  } else if (displayMessage.length() <= 32) {
    // Medium message - use both lines
    lcd.setCursor(0, 0);
    lcd.print(displayMessage.substring(0, 16));
    if (displayMessage.length() > 16) {
      lcd.setCursor(0, 1);
      lcd.print(displayMessage.substring(16));
    }
    isScrolling = false;
  } else {
    // Long message - enable scrolling
    lcd.setCursor(0, 0);
    lcd.print(displayMessage.substring(0, 16));
    if (displayMessage.length() > 16) {
      lcd.setCursor(0, 1);
      lcd.print(displayMessage.substring(16, 32));
    }
    isScrolling = true;
    lastUpdate = millis();
  }
}

void scrollDisplay() {
  if (displayMessage.length() > 32) {
    scrollPosition++;
    if (scrollPosition > displayMessage.length() - 16) {
      scrollPosition = 0;
      delay(1000); // Pause at beginning
    }
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(displayMessage.substring(scrollPosition, scrollPosition + 16));
    
    if (displayMessage.length() > scrollPosition + 16) {
      lcd.setCursor(0, 1);
      int secondLineStart = scrollPosition + 16;
      if (secondLineStart < displayMessage.length()) {
        lcd.print(displayMessage.substring(secondLineStart, min((int)displayMessage.length(), secondLineStart + 16)));
      }
    }
  }
}