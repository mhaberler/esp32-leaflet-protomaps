# ESP32-Leaflet-Protomaps

A web server hosting [Protomap](https://protomaps.com/) map tiles and a [Leaflet](https://leafletjs.com/) demo application, serving from an SD card.


## function

The code mounts the SD card and prints some detail about the card discovered:
`````
SD mounted.
SdFat version: 2.2.2
card size: 255.869321 GB
FS type: exFat
Manufacturer ID: 0x3
OEM ID: 0x53 0x44
Product: 'SR256'
Revision: 8.0
Serial number: 2307514230
Manufacturing date: 8/2019
`````

The server will connect to a WiFi access point on startup - modify for your AP in platformio.ini:
``````
[common]
build_flags = 
	-DWIFI_SSID=\"${sysenv.WIFI_SSID}\"
	-DWIFI_PASSWORD=\"${sysenv.WIFI_PASSWORD}\"
``````
Once running and connected, the server will announce itself via mDNS.

If the access point cant be found, the server runs in access point mode using SSID `protomaps` password `brandonrocks` .

![nMDS announcement](https://github.com/mhaberler/esp32-leaflet-protomaps/raw/master/images/zeroconf.png "nMDS announcement")

click on the link and it should take you to the demo app:

![Leaflet demo](https://github.com/mhaberler/esp32-leaflet-protomaps/raw/master/images/leaflet.png "Leaflet protomaps demo")


## Platforms tested
- M5Stack Core2
- M5Stack CoreS3
- Espressif ESP32-S3-DevKitM-1-N8 development board

## Installation
`````
git clone https://github.com/mhaberler/esp32-leaflet-protomaps.git
cd esp32-leaflet-protomaps
git submodule update --init --recursive
`````
choose target, build and upload.
## Preparing the demo SD card
I recommend a brand high-speed SD card - I used a [SanDisk Extreme PRO 256GB](https://www.westerndigital.com/en-ie/products/memory-cards/sandisk-extreme-pro-uhs-i-microsd-170-mbps#SDSQXCZ-256G-GN6MA
).
- format as an exFat file system (I did this on a Macbook M1)
- set volume label to LEAFLETPM
- if you are not on MacOS, adapt the `data_dest_dir = /Volumes/LEAFLETPM` setting in platformio.ini for
the proper mount point on your platform.
- in Platformio, select the custom target 'Prepare SD' which populates 
the card with the demo app and a small protomap of Southeast Austria.
- eject, and insert SD card into esp32 system

## Obtaining a world-scale basemap
see https://protomaps.com/docs/downloads/basemaps .
The current basemap is about 110GB  so download takes a while.

Copy the file to the /maps directory on the SD card, and edit /www/index.html to reflect the actual map pathname.

## Parts used
- [Brandon Liu's](https://github.com/bdon) phenomenal [PMtiles](https://github.com/protomaps/PMTiles) format and the [protomap.js](https://github.com/protomaps/protomaps.js) client library 
- Bill Greiman's [SdFat Version 2 - Arduino FAT16/FAT32 exFAT Library](https://github.com/greiman/SdFat.git#57900b2) (as-is, current master)
- a [fork](https://github.com/BalloonWare/ESPAsyncWebServer/tree/mah) of the [ESPAsyncWebServer](https://github.com/esphome/ESPAsyncWebServer) modified to support [HTTP Range requests]/(https://github.com/BalloonWare/ESPAsyncWebServer/commit/0bc9b3474cd05fdbebf9db74954eff7ea0f590f0), and [serve files from an exFat file system](https://github.com/BalloonWare/ESPAsyncWebServer/commit/3cff86b455ee2c099144993da80a11633feab30b) on SD
- A [fork](https://github.com/BalloonWare/AsyncTCP/commits/mah) of the [AsyncTCP](https://github.com/me-no-dev/AsyncTCP) library by [@me-no-dev](https://github.com/me-no-dev) for [ESPHome](https://esphome.io), modified to [make the async_tcp event queue size configurable](https://github.com/BalloonWare/AsyncTCP/commit/214f3841cd00c36ee4c077605e27f1d1bff2155c)

### tuning knobs twisted:
Stock ESPAsyncWebServer understands the Arduino file system as implemented in [arduino-esp32](https://github.com/espressif/arduino-esp32). As SD cards go, this limits filesystem choice to FAT and variants like FAT32. The file size and partition size limits are too small to server large protomap files.

The solution was to use exFat (technically a 64bit file system), use the SdFat library which supports exFat,
and extend ESPAsyncWebServer to serve static files from exFat. The code is not as elegant as I liked it to be,
but I saw no easy way to subclass a 32bit file system with the 64bit exFat.

SdFat would perform even better if one could use the ENABLE_DEDICATED_SPI define. 
However, this assumes a single thread is doing all the SPI work. 
This is not the case when using ESPAsyncWebServer: the open() happens in user code (whatever thread that happens to be at the time),
whereas the reads are done by the `async_tcp` service task of the AsyncTCP stack as needed. This leads to crashes like described [here](https://github.com/greiman/SdFat/issues/349) .

Protomaps relies on HTTP range requests, and stock ESPAsyncWebServer does not support that. I extended it to
support range requests, which was suprisingly easy.

The stock [AsyncTCP](https://github.com/me-no-dev/AsyncTCP) request queue is [size 32](https://github.com/me-no-dev/AsyncTCP/blob/master/src/AsyncTCP.cpp#L98). With rapid scroll-zooming in/out the web server can easiyl overwhelmed by requests.
Increasing the [request queue size](https://github.com/BalloonWare/AsyncTCP/blob/mah/src/AsyncTCP.cpp#L100) to 256 fixed that (see platformio.ini).

With a slow SD card and an active client, it is easy to stall the server with requests. In a standard setup (without fixes above) this typically would trigger a watchdog reset.
I've set the watchdog timer to a liberal 10 sec.

On the M5Stack Core2 and CoreS3 platforms, SPI speed must be below 25Mhz to run reliably.