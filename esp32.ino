#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <EasyButton.h>  
#include <WiFiManager.h>
#include <ArduinoWebsockets.h>   

using namespace websockets;

// -------------------------
// TFT Display Setup for a 170x320 ST7789
// When rotated (setRotation(1)), the effective resolution is 320x170
#define TFT_CS   5
#define TFT_DC   15   
#define TFT_RST  17
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// -------------------------
// Spotify Credentials (for refresh requests only; login is handled remotely)
const char* clientID = "d831b0f307a1448b8ba4aa26c71f1496";
const char* clientSecret = "e83057701a1c4fa9a50ce3849c5acdbd";

// -------------------------
// OAuth Tokens and Expiry (will be updated via WebSocket)
String accessToken = "";
String refreshToken = "";
unsigned long tokenExpiryTime = 0;

// -------------------------
// Cached Track Info and Timing
String lastSong = "";
String lastArtist = "";
unsigned long trackDuration = 0;     // in milliseconds
unsigned long trackStartTime = 0;    // when the track started (millis)
unsigned long lastAPICallTime = 0;

// -------------------------
// Global Flags
bool trackPaused = false;
bool inactiveError = false;  // true when a 204 (inactive) error occurs

// -------------------------
// Button Pin Definitions (using INPUT_PULLUP)
const int BUTTON_PLAY_PAUSE = 26;
const int BUTTON_NEXT       = 25;
const int BUTTON_PREVIOUS   = 27;

// -------------------------
// Create EasyButton objects
EasyButton btnPlayPause(BUTTON_PLAY_PAUSE, 50, true);
EasyButton btnNext(BUTTON_NEXT, 50, true);
EasyButton btnPrevious(BUTTON_PREVIOUS, 50, true);

// -------------------------
// Global scroll variables (in characters) for text scrolling
unsigned long lastScrollUpdateSong = 0;
int scrollOffsetSong = 0;
unsigned long lastScrollUpdateArtist = 0;
int scrollOffsetArtist = 0;
unsigned long songScrollDelayStart = 0;
unsigned long artistScrollDelayStart = 0;

// -------------------------
// Helper: Scroll a string by character
String scrollString(String str, int startIndex, int maxChars) {
  if (!str.endsWith(" ")) { str += " "; }
  int len = str.length();
  String result = "";
  for (int i = 0; i < maxChars; i++) {
    int index = (startIndex + i) % len;
    result += str.charAt(index);
  }
  return result;
}

// -------------------------
// TFT Helper Functions
void clearScreen() { tft.fillScreen(ST77XX_BLACK); }

void displayText(int x, int y, const char* text, uint16_t color = ST77XX_WHITE, int size = 2) {
  tft.setTextColor(color);
  tft.setTextSize(size);
  tft.setCursor(x, y);
  tft.print(text);
}

// -------------------------
// Display Functions
void displayDefault() {
  clearScreen();
  displayText(5, 70, "Waiting for Token...", ST77XX_WHITE, 2);
}

void displayPaused() {
  clearScreen();
  displayText(70, 80, "Song Paused", ST77XX_YELLOW, 3);
}

void displayTrackInfo(const char* song, const char* artist) {
  clearScreen();
  
  // Draw album art placeholder
  int imageX = 10, imageY = 20, imageW = 120, imageH = 120;
  tft.drawRect(imageX, imageY, imageW, imageH, ST77XX_WHITE);
  
  // Text window parameters
  int textX = 150, availableWidth = 160, nowPlayingY = 30, songY = 60, artistY = 90;
  displayText(textX, nowPlayingY, "Now Playing:", ST77XX_CYAN, 2);
  int textSize = 2, charWidth = 6 * textSize;
  int maxChars = availableWidth / charWidth;
  
  // Song text:
  {
    String songStr = String(song);
    tft.fillRect(textX, songY - 16, availableWidth, 20, ST77XX_BLACK);
    tft.setTextSize(textSize);
    tft.setTextWrap(false);
    if (songStr.length() <= maxChars) {
      tft.setCursor(textX, songY);
      tft.setTextColor(ST77XX_WHITE);
      tft.print(songStr);
    } else {
      unsigned long currentMillis = millis();
      if (currentMillis - songScrollDelayStart >= 5000 && currentMillis - lastScrollUpdateSong >= 1000) {
        scrollOffsetSong++;
        int16_t x1, y1;
        uint16_t textWidth, textHeight;
        tft.getTextBounds(song, textX, songY, &x1, &y1, &textWidth, &textHeight);
        if (scrollOffsetSong > (int)(textWidth - availableWidth)) scrollOffsetSong = 0;
        lastScrollUpdateSong = currentMillis;
      }
      String displaySongStr = scrollString(songStr, scrollOffsetSong, maxChars);
      tft.setCursor(textX, songY);
      tft.setTextColor(ST77XX_WHITE);
      tft.print(displaySongStr);
    }
  }
  
  // Artist text:
  {
    String artistStr = String(artist);
    tft.fillRect(textX, artistY - 16, availableWidth, 20, ST77XX_BLACK);
    tft.setTextSize(textSize);
    tft.setTextWrap(false);
    if (artistStr.length() <= maxChars) {
      tft.setCursor(textX, artistY);
      tft.setTextColor(ST77XX_GREEN);
      tft.print(artistStr);
    } else {
      unsigned long currentMillis = millis();
      if (currentMillis - artistScrollDelayStart >= 5000 && currentMillis - lastScrollUpdateArtist >= 1000) {
        scrollOffsetArtist++;
        int16_t ax1, ay1;
        uint16_t artistWidth, artistHeight;
        tft.getTextBounds(artist, textX, artistY, &ax1, &ay1, &artistWidth, &artistHeight);
        if (scrollOffsetArtist > (int)(artistWidth - availableWidth)) scrollOffsetArtist = 0;
        lastScrollUpdateArtist = currentMillis;
      }
      String displayArtistStr = scrollString(artistStr, scrollOffsetArtist, maxChars);
      tft.setCursor(textX, artistY);
      tft.setTextColor(ST77XX_GREEN);
      tft.print(displayArtistStr);
    }
  }
  
  // Progress bar
  unsigned long currentProgress = millis() - trackStartTime;
  if (currentProgress > trackDuration) currentProgress = trackDuration;
  int progressPercent = (trackDuration > 0) ? (100 * currentProgress / trackDuration) : 0;
  int barX = textX, barY = 120, barWidth = availableWidth, barHeight = 15;
  tft.drawRect(barX, barY, barWidth, barHeight, ST77XX_WHITE);
  int filledWidth = (barWidth * progressPercent) / 100;
  tft.fillRect(barX, barY, filledWidth, barHeight, ST77XX_GREEN);
}

// -------------------------
// WiFi & mDNS Setup using WiFiManager
void connectToWiFi() {
  WiFiManager wifiManager;
  wifiManager.setDebugOutput(true);
  if (!wifiManager.autoConnect("ESP32_AP", "esp32pass")) {
    Serial.println("Failed to connect and hit timeout");
    delay(3000);
    ESP.restart();
  }
  Serial.println("\nConnected to WiFi!");
  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());
  
  if (!MDNS.begin("esp32spotify")) {
    Serial.println("mDNS setup failed. Using IP-based redirect.");
  } else {
    Serial.println("mDNS started! Access at http://esp32spotify.local");
  }
}

// -------------------------
// WebSocket Client Integration
WebsocketsClient wsClient;

// Callback to process incoming WebSocket messages (token data)
void onTokenReceived(WebsocketsMessage message) {
  Serial.println("WebSocket Message received: " + message.data());
  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, message.data());
  if (!err) {
    if (doc.containsKey("access_token")) {
      accessToken = doc["access_token"].as<String>();
      Serial.println("Updated Access Token: " + accessToken);
    }
    if (doc.containsKey("refresh_token")) {
      refreshToken = doc["refresh_token"].as<String>();
      Serial.println("Updated Refresh Token: " + refreshToken);
    }
  } else {
    Serial.println("Failed to parse WebSocket message JSON:");
    Serial.println(err.c_str());
  }
}

void connectWebSocket() {
  // Set the callback to handle incoming token data
  wsClient.onMessage(onTokenReceived);
  
  // Attempt connection to your Render endpoint explicitly on port 443.
  if (wsClient.connect("wss://spotify-player-esp32.onrender.com:443")) {
    Serial.println("Connected to WebSocket server");
  } else {
    Serial.println("Failed to connect to WebSocket server");
  }
}

// -------------------------
// Spotify API Request Functions

// Refresh Spotify token using refresh token
void refreshSpotifyToken() {
  if (refreshToken == "") {
    Serial.println("No refresh token available!");
    return;
  }
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, "https://accounts.spotify.com/api/token");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  String requestBody = "grant_type=refresh_token&refresh_token=" + refreshToken +
                       "&client_id=" + String(clientID) +
                       "&client_secret=" + String(clientSecret);
  Serial.println("Refreshing Spotify Access Token at " + String(millis()));
  int httpResponseCode = http.POST(requestBody);
  String response = http.getString();
  Serial.println("HTTP Response: " + response);
  Serial.println("HTTP Code: " + String(httpResponseCode));
  if (httpResponseCode == 200) {
    StaticJsonDocument<1024> doc;
    deserializeJson(doc, response);
    accessToken = doc["access_token"].as<String>();
    tokenExpiryTime = millis() + (doc["expires_in"].as<int>() * 1000);
    Serial.println("New Access Token: " + accessToken);
  } else {
    Serial.println("Failed to refresh token! HTTP Code: " + String(httpResponseCode));
  }
  http.end();
}

void playSong() {
  if (millis() > tokenExpiryTime) {
    Serial.println("Token expired, refreshing...");
    refreshSpotifyToken();
  }
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, "https://api.spotify.com/v1/me/player/play");
  http.addHeader("Authorization", "Bearer " + accessToken);
  http.addHeader("Content-Type", "application/json");
  int httpResponseCode = http.PUT("{}");
  if (httpResponseCode == 200) {
    Serial.println("Playback started.");
    trackPaused = false;
  } else {
    Serial.println("Failed to start playback. Code: " + String(httpResponseCode));
  }
  http.end();
}

void pauseSong() {
  if (millis() > tokenExpiryTime) {
    Serial.println("Token expired, refreshing...");
    refreshSpotifyToken();
  }
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, "https://api.spotify.com/v1/me/player/pause");
  http.addHeader("Authorization", "Bearer " + accessToken);
  http.addHeader("Content-Type", "application/json");
  int httpResponseCode = http.PUT("{}");
  if (httpResponseCode == 200) {
    Serial.println("Playback paused.");
    trackPaused = true;
  } else {
    Serial.println("Failed to pause playback. Code: " + String(httpResponseCode));
  }
  http.end();
}

void nextSong() {
  if (millis() > tokenExpiryTime) {
    Serial.println("Token expired, refreshing...");
    refreshSpotifyToken();
  }
  scrollOffsetSong = 0;
  scrollOffsetArtist = 0;
  songScrollDelayStart = millis();
  artistScrollDelayStart = millis();
  
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, "https://api.spotify.com/v1/me/player/next");
  http.addHeader("Authorization", "Bearer " + accessToken);
  int httpResponseCode = http.POST("{}");
  if (httpResponseCode == 200) {
    Serial.println("Skipped to next track.");
  } else {
    Serial.println("Failed to skip track. Code: " + String(httpResponseCode));
  }
  http.end();
}

void previousSong() {
  if (millis() > tokenExpiryTime) {
    Serial.println("Token expired, refreshing...");
    refreshSpotifyToken();
  }
  scrollOffsetSong = 0;
  scrollOffsetArtist = 0;
  songScrollDelayStart = millis();
  artistScrollDelayStart = millis();
  
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, "https://api.spotify.com/v1/me/player/previous");
  http.addHeader("Authorization", "Bearer " + accessToken);
  int httpResponseCode = http.POST("{}");
  if (httpResponseCode == 200) {
    Serial.println("Returned to previous track.");
  } else {
    Serial.println("Failed to go to previous track. Code: " + String(httpResponseCode));
  }
  http.end();
}

void getSpotifyNowPlaying() {
  if (millis() - lastAPICallTime < 3000) return;
  lastAPICallTime = millis();
  
  if (millis() > tokenExpiryTime) {
    Serial.println("Access token expired! Refreshing...");
    refreshSpotifyToken();
  }
  
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, "https://api.spotify.com/v1/me/player/currently-playing");
  http.addHeader("Authorization", "Bearer " + accessToken);
  
  int httpCode = http.GET();
  String payload = http.getString();
  Serial.println("HTTP Code: " + String(httpCode));
  
  if (httpCode == 200 && payload.length() > 0) {
    Serial.println("Now Playing: " + payload);
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
      Serial.println("JSON Parsing Error!");
    } else {
      bool isPlaying = doc["is_playing"];
      if (!isPlaying) {
        trackPaused = true;
        clearScreen();
        displayPaused();
      } else {
        trackPaused = false;
        inactiveError = false;
        String songName = doc["item"]["name"].as<String>();
        String artistName = doc["item"]["artists"][0]["name"].as<String>();
        if (songName != lastSong) {
          scrollOffsetSong = 0;
          songScrollDelayStart = millis();
          scrollOffsetArtist = 0;
          artistScrollDelayStart = millis();
        }
        trackDuration = doc["item"]["duration_ms"].as<unsigned long>();
        unsigned long progress_ms = doc["progress_ms"].as<unsigned long>();
        trackStartTime = millis() - progress_ms;
        lastSong = songName;
        lastArtist = artistName;
        displayTrackInfo(songName.c_str(), artistName.c_str());
      }
    }
  }
  else if (httpCode == 204 || payload.length() == 0) {
    Serial.println("No track playing or inactive device.");
    trackPaused = true;
    inactiveError = true;
    clearScreen();
    displayText(10, 90, "Device inactive", ST77XX_WHITE, 2);
  }
  else if (httpCode == 400) {
    Serial.println("Error 400: Please log in");
    clearScreen();
    displayText(10, 70, "Please log in at", ST77XX_WHITE, 2);
    displayText(10, 100, "http://esp32spotify.local", ST77XX_GREEN, 2);
  }
  else {
    Serial.println("Failed to fetch song! Code: " + String(httpCode));
  }
  
  http.end();
}

// -------------------------
// Timing Variables
unsigned long lastAPIUpdate = 0;
unsigned long apiInterval = 500;  // update API every 500ms

// Global variable to track last display mode (0: default, 1: paused, 2: playing)
int lastDisplayMode = -1;

// -------------------------
// Button Checking with EasyButton
void setupButtons() {
  btnPlayPause.begin();
  btnNext.begin();
  btnPrevious.begin();
  
  btnPlayPause.onPressed([](){
    if (trackPaused) {
      playSong();
      trackPaused = false;
    } else {
      pauseSong();
      trackPaused = true;
    }
  });
  
  btnNext.onPressed([](){ nextSong(); });
  btnPrevious.onPressed([](){ previousSong(); });
}

void loopButtons() {
  btnPlayPause.read();
  btnNext.read();
  btnPrevious.read();
}

void setup() {
  Serial.begin(115200);
  tft.init(170, 320);
  tft.setRotation(1);
  clearScreen();
  displayText(10, 70, "Spotify Display Ready", ST77XX_WHITE, 2);
  
  setupButtons();
  
  // Connect to WiFi using WiFiManager
  connectToWiFi();
  
  // Connect to the secure WebSocket server to receive token updates
  connectWebSocket();
}

void loop() {
  wsClient.poll();  // Process incoming WebSocket messages
  loopButtons();
  
  // Update the display based on token status and Spotify playback
  if (accessToken == "") {
    displayDefault();
  } else {
    if (millis() - lastAPIUpdate >= apiInterval) {
      getSpotifyNowPlaying();
      lastAPIUpdate = millis();
    }
  }
  
  delay(10);  // Short delay to prevent CPU overload
}
