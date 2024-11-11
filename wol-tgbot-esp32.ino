#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "WiFiUDP.h"
#include "WakeOnLan.h"
#include "UniversalTelegramBot.h"
#include "ArduinoJson.h"
#include "esp_task_wdt.h"

// WiFi
const char *WIFI_SSID = "XXXXXXXX";
const char *WIFI_PASSWORD = "XXXXXXXX";

// Telegram Bot API
const char *BOT_TOKEN = "0000000000:xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
const char *BOT_ALLOWED_ID = "0000000000";

// MAC Address of WakeOnLan device
const char *MAC_ADDR = "XX:XX:XX:XX:XX:XX";

// LED Pin Configuration
const int WIFI_LED_PIN = LED_BUILTIN; // Built-in LED on most ESP32 dev boards, change if using different pin

// Constants
const int WDT_TIMEOUT = 60;               // Watchdog timeout in seconds
const int WIFI_RETRY_DELAY = 5000;        // WiFi retry delay in milliseconds
const unsigned long WIFI_TIMEOUT = 20000; // WiFi connection timeout
const unsigned long BOT_MTBS = 1000;      // mean time between scan messages

// Global variables
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
WiFiUDP UDP;
WakeOnLan WOL(UDP);
unsigned long bot_lasttime = 0;
unsigned long lastWiFiRetry = 0;

bool setupWiFi()
{
  Serial.print("[WiFi] Connecting to ");
  Serial.println(WIFI_SSID);

  // Turn off LED while connecting
  digitalWrite(WIFI_LED_PIN, LOW);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startAttemptTime = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - startAttemptTime < WIFI_TIMEOUT)
  {
    Serial.print(".");
    delay(500);
  }

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("\nFailed to connect to WiFi");
    digitalWrite(WIFI_LED_PIN, LOW); // Ensure LED is off if connection fails
    return false;
  }

  Serial.print("\nWiFi connected. IP address: ");
  Serial.println(WiFi.localIP());
  digitalWrite(WIFI_LED_PIN, HIGH); // Turn on LED when connected
  return true;
}

bool setupTime()
{
  Serial.print("Retrieving time: ");
  configTime(0, 0, "time.cloudflare.com", "pool.ntp.org");

  unsigned long startAttempt = millis();
  time_t now = time(nullptr);

  while (now < 24 * 3600 && millis() - startAttempt < 10000)
  {
    delay(100);
    Serial.print(".");
    now = time(nullptr);
  }

  if (now < 24 * 3600)
  {
    Serial.println("\nFailed to get NTP time");
    return false;
  }

  Serial.println(now);
  return true;
}

void sendWOL()
{
  WOL.sendMagicPacket(MAC_ADDR);
  delay(300);
}

// Functions for status command
String formatMemory(uint32_t bytes)
{
  String result;

  // Convert to different units if needed
  if (bytes > 1024 * 1024)
  {
    float mb = bytes / (1024.0 * 1024.0);
    result = String(mb, 2) + " MB";
  }
  else if (bytes > 1024)
  {
    float kb = bytes / 1024.0;
    result = String(kb, 2) + " KB";
  }
  else
  {
    result = String(bytes) + " bytes";
  }

  return result;
}

String getSystemStatus()
{
  String status = "📊 System Status\n";
  status += "━━━━━━━━━\n";
  status += "🗃️ Memory: " + formatMemory(ESP.getFreeHeap()) + " / " + formatMemory(ESP.getHeapSize()) + "\n";
  status += "📡 WiFi RSSI: " + String(WiFi.RSSI()) + " dBm\n";
  status += "🌐 IP: " + WiFi.localIP().toString() + "\n";
  status += "📝 SSID: " + String(WiFi.SSID()) + "\n";
  return status;
}

void handleNewMessages(int numNewMessages)
{
  for (int i = 0; i < numNewMessages; i++)
  {
    esp_task_wdt_reset(); // Reset watchdog timer while processing messages

    if (bot.messages[i].from_id != BOT_ALLOWED_ID)
      continue;

    String chat_id = bot.messages[i].chat_id;
    String text = bot.messages[i].text;
    String from_name = bot.messages[i].from_name;
    if (from_name == "")
      from_name = "Guest";

    if (text == "/wol")
    {
      sendWOL();
      bot.sendMessage(chat_id, "Magic Packet sent!", "");
    }
    else if (text == "/ping")
    {
      bot.sendMessage(chat_id, "Pong.", "");
    }
    else if (text == "/status")
    {
      bot.sendMessage(chat_id, getSystemStatus(), "");
    }
    else if (text == "/start" || text == "/help")
    {
      String welcome = "Welcome to **WoL Bot**, " + from_name + ".\n";
      welcome += "Use is restricted to the bot owner.\n\n";
      welcome += "/wol : Send the Magic Packet\n";
      welcome += "/status : Check the bot status\n";
      welcome += "/ping : Pong.\n";
      bot.sendMessage(chat_id, welcome, "Markdown");
    }
  }
}

void setup()
{
  Serial.begin(115200);
  delay(100);
  Serial.println("Starting up...");

  // Initialize LED pin
  pinMode(WIFI_LED_PIN, OUTPUT);
  digitalWrite(WIFI_LED_PIN, LOW); // Start with LED off

  // Initialize Watchdog
  esp_task_wdt_config_t wdt_config = {
      .timeout_ms = WDT_TIMEOUT,
      .idle_core_mask = (1 << 0), // Monitor core 0 idle task
      .trigger_panic = true       // Trigger panic handler on timeout
  };
  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL); // add current thread to WDT watch

  // Setup WiFi with retry
  while (!setupWiFi())
  {
    delay(WIFI_RETRY_DELAY);
  }

  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);

  // Setup time with retry
  while (!setupTime())
  {
    delay(WIFI_RETRY_DELAY);
  }
}

void loop()
{
  esp_task_wdt_reset(); // Reset watchdog timer

  // Handle bot messages
  if (millis() - bot_lasttime > BOT_MTBS)
  {
    bool shouldCheckMessages = true;

    // Check WiFi first
    if (WiFi.status() != WL_CONNECTED)
    {
      Serial.println("WiFi connection lost. Reconnecting...");
      digitalWrite(WIFI_LED_PIN, LOW); // Turn off LED when connection is lost
      WiFi.disconnect();
      shouldCheckMessages = setupWiFi(); // Only check messages if WiFi reconnection successful
    }

    // Check messages if WiFi is connected
    if (shouldCheckMessages)
    {
      int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
      if (numNewMessages)
      {
        Serial.println("Response received");
        handleNewMessages(numNewMessages);
      }
    }

    bot_lasttime = millis();
  }

  delay(10);
}
