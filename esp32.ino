#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <EasyButton.h>  

// -------------------------
// TFT Display Setup for a 170x320 ST7789
// When rotated (setRotation(1)), the effective resolution is 320x170
#define TFT_CS   5
#define TFT_DC   15   // Changed from 16 since that pin is used for NeoPixel
#define TFT_RST  17
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// -------------------------
// WiFi Credentials
const char* ssid = "SINGTEL-MN7V";
const char* password = "Meng3953";

// -------------------------
// Web Server on port 80
WebServer server(80);

// -------------------------
// Spotify Credentials
const char* clientID = "d831b0f307a1448b8ba4aa26c71f1496";
const char* clientSecret = "e83057701a1c4fa9a50ce3849c5acdbd";
String redirectURI = "";

// -------------------------
// OAuth Tokens and Expiry
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
// Create EasyButton objects (debounce time: 50ms, active LOW)
EasyButton btnPlayPause(BUTTON_PLAY_PAUSE, 50, true);
EasyButton btnNext(BUTTON_NEXT, 50, true);
EasyButton btnPrevious(BUTTON_PREVIOUS, 50, true);

// -------------------------
// Global scroll variables (in characters) for text scrolling
unsigned long lastScrollUpdateSong = 0;
int scrollOffsetSong = 0;
unsigned long lastScrollUpdateArtist = 0;
int scrollOffsetArtist = 0;

// Global variables for initial scroll delay (5 seconds)
unsigned long songScrollDelayStart = 0;
unsigned long artistScrollDelayStart = 0;

// -------------------------
// Helper function: Scroll a string by character.
// Returns a substring of length maxChars starting from startIndex, wrapping around.
// Appends a space if not present.
String scrollString(String str, int startIndex, int maxChars) {
  if (!str.endsWith(" ")) {
    str += " ";
  }
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
void clearScreen() {
  tft.fillScreen(ST77XX_BLACK);
}

void displayText(int x, int y, const char* text, uint16_t color = ST77XX_WHITE, int size = 2) {
  tft.setTextColor(color);
  tft.setTextSize(size);
  tft.setCursor(x, y);
  tft.print(text);
}

// -------------------------
// Display Functions for Various Modes
void displayDefault() {
  clearScreen();
  displayText(5, 70, "Please log in at", ST77XX_WHITE, 2);
  displayText(5, 100, "http://esp32spotify.local", ST77XX_GREEN, 2);
}

void displayPaused() {
  clearScreen();
  displayText(70, 80, "Song Paused", ST77XX_YELLOW, 3);
}

// Updated displayTrackInfo():
// Draw album art placeholder on left, then display scrolling song and artist text plus a progress bar.
// The right-side text window is fixed to x = 150 and width = 160.
void displayTrackInfo(const char* song, const char* artist) {
  clearScreen();
  
  // 1) Draw album art placeholder on left side (120x120 rectangle)
  int imageX = 10, imageY = 20, imageW = 120, imageH = 120;
  tft.drawRect(imageX, imageY, imageW, imageH, ST77XX_WHITE);
  
  // 2) Right-side text window parameters
  int textX = 150;
  int availableWidth = 160;
  int nowPlayingY = 30;
  int songY = 60;
  int artistY = 90;
  
  displayText(textX, nowPlayingY, "Now Playing:", ST77XX_CYAN, 2);
  
  int textSize = 2;
  int charWidth = 6 * textSize;
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
      if (currentMillis - songScrollDelayStart >= 5000) { // wait 5 sec before scrolling
        if (currentMillis - lastScrollUpdateSong >= 1000) { // update offset every 1 sec
          scrollOffsetSong++;
          int16_t x1, y1;
          uint16_t textWidth, textHeight;
          tft.getTextBounds(song, textX, songY, &x1, &y1, &textWidth, &textHeight);
          if (scrollOffsetSong > (int)(textWidth - availableWidth)) {
            scrollOffsetSong = 0;
          }
          lastScrollUpdateSong = currentMillis;
        }
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
      if (currentMillis - artistScrollDelayStart >= 5000) { // wait 5 sec before scrolling
        if (currentMillis - lastScrollUpdateArtist >= 1000) { // update offset every 1 sec
          scrollOffsetArtist++;
          int16_t ax1, ay1;
          uint16_t artistWidth, artistHeight;
          tft.getTextBounds(artist, textX, artistY, &ax1, &ay1, &artistWidth, &artistHeight);
          if (scrollOffsetArtist > (int)(artistWidth - availableWidth)) {
            scrollOffsetArtist = 0;
          }
          lastScrollUpdateArtist = currentMillis;
        }
      }
      String displayArtistStr = scrollString(artistStr, scrollOffsetArtist, maxChars);
      tft.setCursor(textX, artistY);
      tft.setTextColor(ST77XX_GREEN);
      tft.print(displayArtistStr);
    }
  }
  
  // 3) Draw a progress bar below the track info.
  unsigned long currentProgress = millis() - trackStartTime;
  if (currentProgress > trackDuration) currentProgress = trackDuration;
  int progressPercent = (trackDuration > 0) ? (100 * currentProgress / trackDuration) : 0;
  
  int barX = textX, barY = 120, barWidth = availableWidth, barHeight = 15;
  tft.drawRect(barX, barY, barWidth, barHeight, ST77XX_WHITE);
  int filledWidth = (barWidth * progressPercent) / 100;
  tft.fillRect(barX, barY, filledWidth, barHeight, ST77XX_GREEN);
}

// -------------------------
// Wi-Fi and mDNS Setup
void connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi!");
  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());
  
  if (!MDNS.begin("esp32spotify")) {
    Serial.println("mDNS setup failed. Using IP-based redirect.");
    redirectURI = "http://" + WiFi.localIP().toString() + "/callback";
  } else {
    Serial.println("mDNS started! Access at http://esp32spotify.local");
    redirectURI = "http://esp32spotify.local/callback";
  }
  Serial.println("Redirect URI: " + redirectURI);
}

// -------------------------
// Web Server Handlers
void handleRoot() {
  String loginURL = "https://accounts.spotify.com/authorize?client_id=" + String(clientID) +
                    "&response_type=code&redirect_uri=" + redirectURI +
                    "&scope=user-read-playback-state%20user-read-currently-playing%20user-modify-playback-state";
  String html = "<h1>ESP32 Spotify Authentication</h1>"
                "<p><a href='" + loginURL + "'><button>Login with Spotify</button></a></p>";
  server.send(200, "text/html", html);
}

void handleSpotifyCallback() {
  if (server.hasArg("code")) {
    String authCode = server.arg("code");
    Serial.println("Received Spotify Auth Code: " + authCode);
    server.send(200, "text/html", "<h1>Auth Successful! Exchanging token...</h1>");
    // Reset scroll offsets when new token is acquired.
    scrollOffsetSong = 0;
    scrollOffsetArtist = 0;
    songScrollDelayStart = millis();
    artistScrollDelayStart = millis();
    exchangeCodeForAccessToken(authCode);
  } else {
    server.send(400, "text/html", "<h1>Error: No authorization code received.</h1>");
    Serial.println("Error: No authorization code received.");
  }
}

// -------------------------
// Spotify Token Management
void exchangeCodeForAccessToken(String authCode) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, "https://accounts.spotify.com/api/token")) {
    Serial.println("Failed to connect to Spotify API!");
    return;
  }
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  String requestBody = "grant_type=authorization_code&code=" + authCode +
                       "&redirect_uri=" + redirectURI +
                       "&client_id=" + String(clientID) +
                       "&client_secret=" + String(clientSecret);
  Serial.println("Sending Token Request to Spotify at " + String(millis()));
  int httpResponseCode = http.POST(requestBody);
  String response = http.getString();
  Serial.println("HTTP Response: " + response);
  Serial.println("HTTP Code: " + String(httpResponseCode));
  if (httpResponseCode == 200) {
    StaticJsonDocument<1024> doc;
    deserializeJson(doc, response);
    accessToken = doc["access_token"].as<String>();
    refreshToken = doc["refresh_token"].as<String>();
    tokenExpiryTime = millis() + (doc["expires_in"].as<int>() * 1000);
    Serial.println("New Access Token: " + accessToken);
    Serial.println("Refresh Token: " + refreshToken);
  } else {
    Serial.println("Failed! HTTP Code: " + String(httpResponseCode));
  }
  http.end();
}

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

// -------------------------
// Spotify Control Functions
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
  // Reset scroll variables when skipping to a new track.
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
  // Reset scroll variables when returning to a new track.
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

// -------------------------
// Fetch Currently Playing Song and Update Display
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
  // No timeout is set here, so the client waits for the response naturally.
  
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
        // Reset scroll offsets if a new song is detected.
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
unsigned long apiInterval = 500;       // update API every 500ms

// -------------------------
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
  
  // Set up EasyButton for debouncing
  setupButtons();
  
  connectToWiFi();
  server.on("/", handleRoot);
  server.on("/callback", handleSpotifyCallback);
  server.on("/now-playing", getSpotifyNowPlaying);
  server.begin();
}

void loop() {
  server.handleClient();
  
  loopButtons();
  
  if (millis() - lastAPIUpdate >= apiInterval) {
    getSpotifyNowPlaying();
    lastAPIUpdate = millis();
  }
  
  // Update display only if mode has changed
  int currentDisplayMode;
  if (accessToken == "") {
    currentDisplayMode = 0; // default (login)
  } else if (trackPaused) {
    currentDisplayMode = 1; // paused
  } else if (lastSong != "") {
    currentDisplayMode = 2; // playing
  } else {
    currentDisplayMode = 0; // default
  }
  
  if (currentDisplayMode != lastDisplayMode) {
    if (currentDisplayMode == 0) {
      displayDefault();
    } else if (currentDisplayMode == 1) {
      displayPaused();
    } else if (currentDisplayMode == 2) {
      displayTrackInfo(lastSong.c_str(), lastArtist.c_str());
    }
    lastDisplayMode = currentDisplayMode;
  }
  
  delay(10);  // Short delay to prevent CPU overload
}
