# T-Internet-COM Lab

Учебно-практический проект для платы **LILYGO T-Internet-COM на ESP32**.

Цель проекта — последовательно превратить плату из набора заводских примеров в понятный коммуникационный узел для реальных систем: Ethernet, Wi-Fi, Bluetooth, microSD, локальная диагностика и, позднее, резервный LTE-канал через модемный модуль формата Mini PCIe.

> Текущий этап: сотовый модем не установлен. Основная прошивка показывает общий статус модемного слота, но не включает питание и не выполняет AT-опрос.

## Основная диагностическая прошивка

Скетч `firmware/arduino/ticom_board_check/ticom_board_check.ino` объединяет:

- расширенную системную диагностику ESP32;
- Ethernet LAN8720;
- Wi-Fi scan/connect/off;
- microSD status/list/read-write-delete test;
- Bluetooth Classic SPP;
- BLE scan;
- RGB WS2812;
- общий статус модемного слота.

Bluetooth по умолчанию выключен и занимает дополнительную память только после команды `BT CLASSIC START` или во время `BLE SCAN`.

Отдельный скетч `firmware/arduino/ticom_bluetooth_check/ticom_bluetooth_check.ino` сохранён как изолированный стенд для повторной проверки Bluetooth без Ethernet, Wi-Fi и microSD.

## Проверенная конфигурация Arduino IDE

- ESP32 by Espressif Systems **3.3.11**;
- Board: **ESP32 Dev Module**;
- CPU Frequency: **240 MHz (WiFi/BT)**;
- Flash Frequency: **80 MHz**;
- Flash Mode: **QIO**;
- Flash Size: **4 MB (32 Mb)**;
- Partition Scheme: **Huge APP (3 MB No OTA / 1 MB SPIFFS)**;
- PSRAM: **Enabled**;
- Upload Speed: **921600**;
- библиотека **Adafruit NeoPixel**.

`Network`, `WiFi`, `ETH`, `FS`, `SD`, `SPI`, `BluetoothSerial` и `BLE` входят в Arduino-ESP32.

## Быстрый старт

1. Откройте `firmware/arduino/ticom_board_check/ticom_board_check.ino`.
2. Установите **Adafruit NeoPixel**.
3. Выберите параметры Arduino IDE из раздела выше.
4. Загрузите прошивку.
5. Откройте Serial Monitor на `115200`.
6. Введите `HELP`.

## Команды

```text
HELP                         список команд
STATUS                       полный отчёт по всем узлам
INFO                         расширенная диагностика ESP32
SYSTEM                       то же, что INFO
ETH                          состояние Ethernet
WIFI STATUS                  состояние Wi-Fi
WIFI SCAN                    поиск Wi-Fi-сетей
WIFI CONNECT <SSID>|<PASS>   подключение к Wi-Fi
WIFI OFF                     отключение Wi-Fi
SD                           состояние microSD
SD LIST                      список файлов в корне
SD TEST                      запись, чтение, проверка и удаление файла
BT STATUS                    состояние Bluetooth и MAC
BT CLASSIC START             запуск Bluetooth Classic SPP
BT CLASSIC STOP              остановка Bluetooth Classic SPP
BT SEND <TEXT>               отправка строки SPP-клиенту
BLE SCAN                     поиск BLE-устройств в течение 5 секунд
BT OFF                       остановка всех Bluetooth-режимов
MODEM STATUS                 установлен ли модем и поле модели
LED RED                      красный
LED GREEN                    зелёный
LED BLUE                     синий
LED WHITE                    белый
LED OFF                      выключить RGB
REBOOT                       перезагрузить ESP32
```

Пример Wi-Fi:

```text
WIFI CONNECT MyNetwork|MyPassword
```

Пример Bluetooth Classic:

```text
BT CLASSIC START
BT SEND Hello from T-Internet-COM
```

В Windows используется исходящий COM-порт сервиса `ESP32SPP`. После открытия порта обмен двунаправленный: данные с компьютера выводятся в USB Serial и возвращаются эхом, а `BT SEND` передаёт строку с ESP32 на компьютер.

## Модем

Пока в проверенной плате модем отсутствует:

```text
Module installed: no
Model: unavailable
```

В `board_config.h` предусмотрены:

```cpp
constexpr bool MODEM_INSTALLED = false;
constexpr const char *MODEM_MODEL = "";
```

После установки модуля сначала будет добавлен безопасный AT-опрос. Марка и модель появятся только если модем реально ответит на стандартные команды вроде `ATI`, `AT+CGMM` или эквивалентные. До аппаратной проверки прошивка ничего не угадывает.

## Подтверждённые аппаратные испытания

На реальной плате подтверждены:

- ESP32, PSRAM и системная диагностика;
- Ethernet 100 Mbps Full Duplex, DHCP, шлюз и DNS;
- microSD около 32 ГБ и полный цикл записи/чтения/удаления;
- Wi-Fi scan;
- Bluetooth Classic SPP через Windows COM-порт;
- двунаправленная передача и эхо;
- команда `BT SEND`;
- BLE scan;
- запуск без установленного модема.

Фактические результаты находятся в `docs/TEST_LOG.md`.

## Структура

```text
firmware/arduino/ticom_board_check/      общая диагностическая прошивка
firmware/arduino/ticom_bluetooth_check/  отдельный Bluetooth-стенд
docs/PINMAP.md                           распиновка
docs/ROADMAP.md                          план развития
docs/TEST_LOG.md                         журнал испытаний
hardware/enclosure/README.md             требования к держателю
```

## План развития

1. проверить общую прошивку после интеграции Bluetooth;
2. журналировать события на microSD;
3. добавить локальный веб-интерфейс с вкладками;
4. добавить безопасное обнаружение модема и чтение его модели;
5. реализовать переключение Ethernet → Wi-Fi → LTE;
6. добавить RS-485 и защищённое питание.

## Лицензия

MIT для исходного кода и документации проекта. Названия и товарные знаки LILYGO, ESP32 и SIMCom принадлежат соответствующим правообладателям.
