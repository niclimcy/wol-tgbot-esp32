#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "WiFiUDP.h"
#include "WakeOnLan.h"
#include "UniversalTelegramBot.h"
#include "ArduinoJson.h"
#include "esp_task_wdt.h"

// WiFi
const char WIFI_SSID[] PROGMEM = "XXXXXXXX";
const char WIFI_PASSWORD[] PROGMEM = "XXXXXXXX";

// Telegram Bot API
const char BOT_TOKEN[] PROGMEM = "0000000000:xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
const char BOT_ALLOWED_ID[] PROGMEM = "0000000000";

// MAC Address of WakeOnLan device
const char MAC_ADDR[] PROGMEM = "XX:XX:XX:XX:XX:XX";

// Constants
constexpr uint8_t WIFI_LED_PIN = LED_BUILTIN; // Built-in LED on most ESP32 dev boards, change if using different pin
constexpr uint8_t WDT_TIMEOUT = 60;
constexpr uint16_t WIFI_RETRY_DELAY = 5000;
constexpr uint16_t WIFI_TIMEOUT = 20000;
constexpr uint16_t BOT_MTBS = 1000;

// Global variables (minimized)
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
WiFiUDP UDP;
WakeOnLan WOL(UDP);
uint32_t bot_lasttime = 0;

bool setupWiFi() {
  Serial.print(F("[WiFi] Connecting to "));
  Serial.println(WIFI_SSID);

  digitalWrite(WIFI_LED_PIN, HIGH);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  uint32_t startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < WIFI_TIMEOUT) {
    Serial.print(".");
    delay(500);
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("\nFailed to connect to WiFi"));
    return false;
  }

  Serial.print(F("\nWiFi connected. IP address: "));
  Serial.println(WiFi.localIP());
  digitalWrite(WIFI_LED_PIN, LOW);
  return true;
}

bool setupTime() {
  Serial.print(F("Retrieving time: "));
  configTime(0, 0, "time.cloudflare.com", "pool.ntp.org");

  time_t now = time(nullptr);
  uint32_t startTime = millis();

  while (now < 24 * 3600 && millis() - startTime < 10000) {
    delay(100);
    Serial.print(".");
    now = time(nullptr);
  }

  if (now < 24 * 3600) {
    Serial.println(F("\nFailed to get NTP time"));
    return false;
  }

  Serial.println(now);
  return true;
}

void setupWDT() {
  // Delete any existing watchdog timer first
  esp_task_wdt_deinit();

  // Initialize Watchdog with new config
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = WDT_TIMEOUT * 1000,
    .idle_core_mask = (1 << 0),
    .trigger_panic = true
  };
  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);  // Add current thread to WDT watch
}

void sendWOL() {
  WOL.sendMagicPacket(MAC_ADDR);
  delay(300);
}

void sendSystemStatus(const String& chat_id) {
  String status =
    "📊 System Status\n"
    "━━━━━━━━━\n";
  status += "🗃️ Memory: " + String(ESP.getFreeHeap() / 1024.0, 1) + " / " + String(ESP.getHeapSize() / 1024.0, 1) + " KB\n";
  status += "📡 RSSI: " + String(WiFi.RSSI()) + " dBm\n";
  status += "🌐 IP: " + WiFi.localIP().toString() + "\n";
  status += "📝 SSID: " + WiFi.SSID();

  bot.sendMessage(chat_id, status, "");
}

void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    esp_task_wdt_reset();

    if (String(BOT_ALLOWED_ID) != bot.messages[i].from_id) continue;

    const String& chat_id = bot.messages[i].chat_id;
    const String& text = bot.messages[i].text;
    String from_name = bot.messages[i].from_name;
    if (from_name.isEmpty()) {
      from_name = F("Guest");
    }

    if (text == F("/wol")) {
      sendWOL();
      bot.sendMessage(chat_id, F("Magic Packet sent!"), "");
    } else if (text == F("/ping")) {
      bot.sendMessage(chat_id, F("Pong."), "");
    } else if (text == F("/status")) {
      sendSystemStatus(chat_id);
    } else if (text == F("/start") || text == F("/help")) {
      String welcome = "Welcome to **WoL Bot**, " + from_name + ".\n";
      welcome += F("Use is restricted to the bot owner.\n\n");
      welcome += F("/wol : Send the Magic Packet\n");
      welcome += F("/status : Check the bot status\n");
      welcome += F("/ping : Pong.");
      bot.sendMessage(chat_id, welcome, "Markdown");
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println(F("Starting up..."));

  pinMode(WIFI_LED_PIN, OUTPUT);
  digitalWrite(WIFI_LED_PIN, HIGH);

  // Initialize Watchdog
  setupWDT();

  while (!setupWiFi()) {
    delay(WIFI_RETRY_DELAY);
  }

  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);

  while (!setupTime()) {
    delay(WIFI_RETRY_DELAY);
  }
}

void loop() {
  esp_task_wdt_reset();

  if (millis() - bot_lasttime > BOT_MTBS) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println(F("WiFi connection lost. Reconnecting..."));
      digitalWrite(WIFI_LED_PIN, HIGH);
      WiFi.disconnect();
      if (!setupWiFi()) {
        bot_lasttime = millis();
        return;
      }
    }

    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    if (numNewMessages) {
      Serial.println(F("Response received"));
      handleNewMessages(numNewMessages);
    }
    bot_lasttime = millis();
  }
  delay(10);
}
