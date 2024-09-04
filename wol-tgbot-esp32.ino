#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "WiFiUDP.h"

#include "WakeOnLan.h"
#include "UniversalTelegramBot.h"
#include "ArduinoJson.h"

// WiFi
const char *WIFI_SSID = "XXXXXXXX";
const char *WIFI_PASSWORD = "XXXXXXXX";

// Telegram Bot API
const char *BOT_TOKEN = "0000000000:xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
const char *BOT_ALLOWED_ID = "0000000000";

// MAC Address of WakeOnLan device
const char *MAC_ADDR = "XX:XX:XX:XX:XX:XX";

// UniversalTelegramBot setup
WiFiClientSecure secured_client;
const unsigned long BOT_MTBS = 1000;  // mean time between scan messages
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
unsigned long bot_lasttime;  // last time messages' scan has been done

// WakeOnLan setup
WiFiUDP UDP;
WakeOnLan WOL(UDP);

void sendWOL() {
  WOL.sendMagicPacket(MAC_ADDR);  // send WOL on default port (9)
  delay(300);
}

void handleNewMessages(int numNewMessages) {
  Serial.print("handleNewMessages ");
  Serial.println(numNewMessages);

  for (int i = 0; i < numNewMessages; i++) {
    Serial.println(bot.messages[i].from_id);
    if (bot.messages[i].from_id != BOT_ALLOWED_ID) continue;

    String chat_id = bot.messages[i].chat_id;
    String text = bot.messages[i].text;

    String from_name = bot.messages[i].from_name;
    if (from_name == "") from_name = "Guest";

    if (text == "/wol") {
      sendWOL();
      bot.sendMessage(chat_id, "Magic Packet sent!", "");
    } else if (text == "/ping") {
      bot.sendMessage(chat_id, "Pong.", "");
    } else if (text == "/start") {
      String welcome = "Welcome to **WoL Bot**, " + from_name + ".\n";
      welcome += "Use is restricted to the bot owner.\n\n";
      welcome += "/wol : Send the Magic Packet\n";
      welcome += "/ping : Check the bot status\n";
      bot.sendMessage(chat_id, welcome, "Markdown");
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(10);

  Serial.println();
  Serial.print("[WiFi] Connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.print("\nWiFi connected. IP address: ");
  Serial.println(WiFi.localIP());

  Serial.print("Retrieving time: ");
  // get UTC time via NTP
  configTime(0, 0, "time.cloudflare.com");

  time_t now = time(nullptr);

  // Add a timeout for NTP sync
  unsigned long start = millis();

  while (now < 24 * 3600) {
    // 10 seconds timeout
    if (millis() - start > 10000) {
      Serial.println("\nFailed to get NTP time. Continuing without it.");
      break;
    }

    Serial.print(".");
    delay(100);
    now = time(nullptr);
  }
  Serial.println(now);
}

void loop() {
  if (millis() - bot_lasttime > BOT_MTBS) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
      Serial.println("Response received");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    bot_lasttime = millis();
  }

  delay(10);
}
