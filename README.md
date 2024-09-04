# Wake on Lan ESP32 Telegram Bot

_An embedded Telegram bot for generic ESP32 boards to send a Wake on LAN magic packets._


This is a sketch for **generic ESP32** boards. A telegram bot is hosted on the board and listens for messages. When the `/wol` command is received, a Wake-on-Lan magic packet is broadcasted on the local network to turn on a target device.


## Installation

- Install the following libraries from the Library Manager of the Arduino IDE:
  - [WakeOnLan](https://www.arduino.cc/reference/en/libraries/wakeonlan/): for sending the magic packet
  - [UniversalTelegramBot](https://www.arduino.cc/reference/en/libraries/universaltelegrambot/): for using the Telegram API
- Create a new Telegram bot and configure your `BOT_TOKEN` and `ALLOWED_ID`  
  _You can use [@Botfather](https://t.me/botfather) to create a new bot and [@userinfobot](https://t.me/userinfobot) to get your ID_
- Fill your _WiFi configuration_ and the _MAC address_ of the PC you want to power on
- Compile and flash to your ESP32 board

## Usage

- Use `/start` to get a list of the available commands
- Use the `/wol` command to turn on your PC
- Use the `/ping` command to check if the bot is online

## Credits

This project is based off the [Wake on Lan 🤖 ESP32 Telegram Bot for M5Atom](https://github.com/daniele-salvagni/wol-bot-esp32) by [Daniele Salvagni](https://github.com/daniele-salvagni). The main difference is that this version targets generic ESP32 boards instead of the M5Atom.
