

#include "SPI.h"
#include "SdFat.h"
#include "SdFatConfig.h"
#ifdef M5UNIFIED
#include <M5Unified.h>
#endif
#include <Arduino.h>
#ifdef CARD_DETECT_PIN
#include "FunctionalInterrupt.h"
#endif
#include <AsyncTCP.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_task_wdt.h>

#define SD_SCK_KHZ(maxkHz) (1000UL * (maxkHz))

DNSServer dnsServer;
AsyncWebServer server(80);

const char *ssid = WIFI_CLIENT_SSID;
const char *password = WIFI_CLIENT_PASSWORD;
const char *ap_ssid = WIFI_AP_SSID;
const char *ap_password = WIFI_AP_PASSWORD;
const char *hostname = HOSTNAME;

// SD card
#define SD_WAIT_MS 2000
#define MAX_OPEN_FILES 10
#define SD_MOUNTPOINT "/sd"
#define SD_CONFIG                                                              \
  SdSpiConfig(SDCARD_CS_PIN, SHARED_SPI, SD_SCK_KHZ(SD_MAX_INIT_RATE_KHZ))

SdBaseFile file;
bool run_dns = false;

volatile int numCdInterrupts = 0;
volatile bool cdLastState;
volatile uint32_t cdDebounceTimeout = 0;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
uint32_t saveCdDebounceTimeout;
bool saveLastState;
int save;

AsyncStaticSdFatWebHandler *handler;

void describeCard(SdFat &sd);

#ifdef CARD_DETECT_PIN
void IRAM_ATTR cardInsertISR(void) {
  portENTER_CRITICAL_ISR(&mux);
  numCdInterrupts++;
  cdLastState = digitalRead(CARD_DETECT_PIN);
  cdDebounceTimeout =
      xTaskGetTickCount(); // version of millis() that works from interrupt
  portEXIT_CRITICAL_ISR(&mux);
}
#endif

class CaptiveRequestHandler : public AsyncWebHandler {
public:
  CaptiveRequestHandler() {}
  virtual ~CaptiveRequestHandler() {}

  bool canHandle(AsyncWebServerRequest *request) { return true; }

  void handleRequest(AsyncWebServerRequest *request) {
    request->redirect(HTML);
  }
};

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

void setup() {
#ifdef ARDUINO_USB_CDC_ON_BOOT
  delay(3000); // wait for USB to come uo before chatting
#endif

#ifdef M5UNIFIED
  auto cfg = M5.config();
  cfg.serial_baudrate = BAUD;
  M5.begin(cfg);
#else
  Serial.begin(BAUD);
#endif

  while (!Serial) {
    yield();
  }

#ifdef CARD_DETECT_PIN
  // Adafruit SD card breakout: this pin shorts to ground if NO card is inserted
  pinMode(CARD_DETECT_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(CARD_DETECT_PIN), cardInsertISR,
                  CHANGE);
  Serial.printf("attaching card detect IRQ on pin %u, current state: %d\n",
                CARD_DETECT_PIN, digitalRead(CARD_DETECT_PIN));
#endif

#ifdef GENERIC_ESP32S3
  // if the default SPI has not been initialized (eg by initing a display)
  // do so now
  //
  // for pins, see
  // https://github.com/espressif/esp-idf/blob/master/examples/storage/sd_card/sdspi/README.md#pin-assignments
  SPI.begin(36, 37, 35);
#endif

  handler = new AsyncStaticSdFatWebHandler("/", "/", "");

  Serial.printf("trying to mount SD...\n");

  uint32_t start = millis();
  bool sd_mounted;
  while (1) {
    sd_mounted = handler->begin(SD_CONFIG);
    if (sd_mounted) {
      Serial.printf("SD mounted.\n");
      break;
    }
    delay(500);
    if ((millis() - start) > SD_WAIT_MS) {
      Serial.printf("giving up on SD.\n");
      break;
    }
    Serial.printf("SD waiting... %.f s\n", (millis() - start) / 1000.0);
  }
  if (!sd_mounted) {
    while (true)
      yield();
  }
  describeCard(handler->_fs);

  Serial.printf("trying to connect to Wifi AP %s using %s\n", ssid, password);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult(5000) != WL_CONNECTED) {
    Serial.printf("WiFi connect failed, switching to AP mode using %s %s\n",
                  ap_ssid, ap_password);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ap_ssid, ap_password);
    WiFi.softAPsetHostname(hostname);
    dnsServer.start(53, "*", WiFi.softAPIP());
    run_dns = true;
    Serial.printf("IP address: %s\n", WiFi.softAPIP().toString().c_str());
  } else {
    Serial.printf("connected to AP %s\n", ssid);
    WiFi.printDiag(Serial);
  }
  if (MDNS.begin(hostname)) {
    MDNS.addService("http", "tcp", 80);
    MDNS.addServiceTxt("http", "tcp", "path", HTML);
  }

#ifdef WDT_TIMEOUT
  Serial.printf("Configuring watchdog timer to %d seconds\n", WDT_TIMEOUT);
  esp_task_wdt_init(WDT_TIMEOUT, true); // enable panic so ESP32 restarts
  esp_task_wdt_add(NULL);               // add current thread to WDT watch
#endif

  server.onNotFound(notFound);
  server.addHandler(handler);
  server.addHandler(new CaptiveRequestHandler());
  server.begin();
}

void loop() {
#ifdef WDT_TIMEOUT
  esp_task_wdt_reset();
#endif
  if (run_dns)
    dnsServer.processNextRequest();

#ifdef CARD_DETECT_PIN

  portENTER_CRITICAL_ISR(&mux);
  save = numCdInterrupts;
  saveCdDebounceTimeout = cdDebounceTimeout;
  saveLastState = cdLastState;
  portEXIT_CRITICAL_ISR(&mux);

  bool currentState = digitalRead(CARD_DETECT_PIN);

  if ((save != 0) && (currentState == saveLastState) &&
      (millis() - saveCdDebounceTimeout > CARD_DETECT_DEBOUNCE_MS)) {

    if (currentState == LOW) {
      Serial.printf("SD card ejected, current tick=%lu\n", millis());
    } else {
      Serial.printf("SD card inserted, current tick=%lu\n", millis());
    }
    portENTER_CRITICAL_ISR(&mux);
    numCdInterrupts = 0;
    portEXIT_CRITICAL_ISR(&mux);
  }
#endif
  yield();
}

void describeCard(SdFat &sd) {
  Serial.printf("SdFat version: %s\n", SD_FAT_VERSION_STR);
  Serial.printf("card size: %f GB\n", sd.card()->sectorCount() * 512E-9);
  switch (sd.fatType()) {
  case FAT_TYPE_EXFAT:
    Serial.printf("FS type: exFat\n");
    break;
  case FAT_TYPE_FAT32:
    Serial.printf("FS type: FAT32\n");
    break;
  case FAT_TYPE_FAT16:
    Serial.printf("FS type: FAT16\n");
    break;
  case FAT_TYPE_FAT12:
    Serial.printf("FS type: FAT12\n");
    break;
  }
  cid_t cid;
  if (!sd.card()->readCID(&cid)) {
    Serial.printf("readCID failed\n");
  } else {
    Serial.printf("Manufacturer ID: 0x%x\n", int(cid.mid));
    Serial.printf("OEM ID: 0x%x 0x%x\n", cid.oid[0], cid.oid[1]);
    Serial.printf("Product: '%5.5s'\n", cid.pnm);
    Serial.printf("Revision: %d.%d\n", cid.prvN(), cid.prvM());
    Serial.printf("Serial number: %u\n", cid.psn());
    Serial.printf("Manufacturing date: %d/%d\n", cid.mdtMonth(), cid.mdtYear());
  }
}
