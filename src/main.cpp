#include <EtherCard.h>
 
#define STATIC 1 // DHCP ( 1 = sim, 2 = não)
 
// mac
static byte mymac[] = { 0x74,0x69,0x69,0x2D,0x30,0x31 };
// ethernet  ip 
static byte myip[] = { 192,168,1,200 };
// gateway ip
static byte gwip[] = { 192,168,1,1 };
 

int current01 = A0;
int current02 = A1;
int current03 = A2;

int volts01 = A4;
int volts02 = A5;
int volts03 = A6;

int acStatus = 22;    //verifica a existencia de ac

int digitalIn01 = 23; //entrada digital 01
int digitalIn02 = 24; //entrada digital 02
int digitalIn03 = 25; //entrada digital 03

int releOut01 = 30; // saida relé
int releOut02 = 31; // saida relé
int releOut03 = 32; // saida relé
int releOut04 = 33; // saida relé
 
byte Ethernet::buffer[2000];
static uint32_t timer;

/* ------------------functions---------------------*/
void blinkLedOrRele(int pin,  int time, int loop,  bool reboot);

void server(
  float volt01, float volt02, float volt03,
  float amp01, float amp02, float amp03,
  int dig01, int dig02, int dig03, 
  int rele01, int rele02, int rele03, int rele04,
  int ac, char * msdg);

String converter(uint8_t *str);

void resAction(word pos);

float calculaCorrent(int corrente);

float calculaTensao(int tensao);

char const page[] PROGMEM =
"HTTP/1.1 200 OK\r\n"
"Content-Type: application/json\r\n"
"Access-Control-Allow-Origin: *\r\n"
"Access-Control-Allow-Headers: Content-Type\r\n"
"Access-Control-Allow-Methods: GET\r\n"
"Retry-After: 1000\r\n"
"\r\n"
"{\"Comand\": \"ok\"}"
;

// called when a ping comes in (replies to it are automatic)
static void gotPinged (byte* ptr) {
  ether.printIp(">>> ping from: ", ptr);
}

void setup () {

  //portas digitais in
  pinMode(acStatus, INPUT);
  pinMode(digitalIn01, INPUT);
  pinMode(digitalIn02, INPUT);
  pinMode(digitalIn03, INPUT);
  //portas para rele
  pinMode(releOut01, OUTPUT);
  pinMode(releOut02, OUTPUT);
  pinMode(releOut03, OUTPUT);
  pinMode(releOut04, OUTPUT);
  
  /* ------------------Conectando---------------------*/
  Serial.begin(9600);
  Serial.println("Trying to get an IP…");
  
  Serial.print("MAC: ");
  for (byte i = 0; i < 6; ++i) {
    Serial.print(mymac[i], HEX);
    if (i < 5)
      Serial.print(':');
  }
  
  if (ether.begin(sizeof Ethernet::buffer, mymac) == 0)
  {
    Serial.println( "Failed to access Ethernet controller");
  }
  else
  {
    Serial.println("Ethernet controller access: OK");
  }
  ;
  
  #if STATIC
    Serial.println( "Getting static IP.");
  if (!ether.staticSetup(myip, gwip)){
    Serial.println( "could not get a static IP");
    while (true){
      blinkLedOrRele(1, 500, 100, false); // blink forever to indicate a problem
    }
  }
  #else
  
  Serial.println("Setting up DHCP");
  if (!ether.dhcpSetup()){
    Serial.println( "DHCP failed");
    while (true){
      blinkLedOrRele(1, 500, 100, false); // blink forever to indicate a problem
    }
  }
  #endif
  // printa no terminal serial o ip conectado.
  ether.printIp("My IP: ", ether.myip);
  ether.printIp("Netmask: ", ether.netmask);
  ether.printIp("GW IP: ", ether.gwip);
  ether.printIp("DNS IP: ", ether.dnsip);

  // Definindo ip do servidor remoto
  ether.parseIp(ether.hisip, "192.168.1.23");
 // identificar outros servidores pingando nosso dispositivo
  ether.registerPingCallback(gotPinged);
  timer = -9999999; //cronometro iniciado
}
 
 //----------------------------loop-----------------------------------------------------
void loop () {

  int pingControl = 0;
  int pingReboot = 0;
  bool tentativaDePing = true;
  char* char_arr; // array de caracteres para pagina web
  
  word len = ether.packetReceive();
  word pos = ether.packetLoop(len);
  
  // verifica comandos recibidos via tcpIp, tipo GET
  resAction(pos);

  // liga lux de ponto automaticamente 
  if(digitalRead(digitalIn01)){
    digitalWrite(releOut02, HIGH);
  }
  if(!digitalRead(digitalIn01) && digitalRead(releOut02)){
    digitalWrite(releOut02, LOW);
  }

  // envia os dados de telemetria
  if(strstr((char *)Ethernet::buffer + pos, "GET /?results") != 0) {
      server(
        // leitura de sensor de voltage ate 75v
        calculaCorrent(analogRead(current01)), calculaCorrent(analogRead(current02)), calculaCorrent(analogRead(current03)), 
        // leitura de sensor de corrente 20A // 12 - 24 volts DC
        calculaTensao(analogRead(volts01)), calculaTensao(analogRead(volts02)), calculaTensao(analogRead(volts03)), 
        //leituras de entrdas digitais
        digitalRead(digitalIn01), digitalRead(digitalIn01) ,digitalRead(digitalIn01),
        //verificação do stado dos reles
        digitalRead(releOut01),digitalRead(releOut02),digitalRead(releOut03),digitalRead(releOut04),
        digitalRead(acStatus) // sensor de tensao alternada
        ,char_arr // arrey de caracteres
        );
  }

  // watchdog for ip test, with ping usage format, this allows to identify when the network is ok
  // reporte sempre que uma resposta ao nosso ping de saída voltar.
  if (len > 0 && ether.packetLoopIcmpCheckReply(ether.hisip)) {
    Serial.print("  ");
    Serial.print((micros() - timer) * 0.001, 3);
    Serial.println(" ms");
    pingControl += 0;
    tentativaDePing = true;
  }

  // pingar um servidor remoto uma vez a cada "n" segundos
  if (micros() - timer >= 5000000) {
    if(pingControl >= 2 && tentativaDePing){
        //reinicia rele de internet
        Serial.println("Reiniciando internet");
        blinkLedOrRele(releOut01, 600, 1, false);
        pingControl = 0;
        //verifica se já reiniciou 3 vezes e bloqueia os proximos reboots
        if(pingReboot >= 3){
          tentativaDePing = false;
        }
    }
    ether.printIp("Pinging: ", ether.hisip);
    timer = micros();
    ether.clientIcmpRequest(ether.hisip);
  }

}
  
  void blinkLedOrRele(int pin, int time, int loop, bool reboot){
    uint8_t comand;
    // se for para reiniciar, mandar reoot = true
    if(reboot){
      comand =  0x0;
    }else{
      comand = 0x0;
    }
    while (loop > 0){
      digitalWrite(pin, comand);
      delay(time);
      digitalWrite(pin, !comand);
      delay(time);
      loop -= 1; 
    }
}

float calculaTensao(int tensao){
 float result = (tensao * 10.24); //max 100V
  return result;
}

float calculaCorrent(int corrente){ 
  float result = corrente * 0.0244140625; //max 25A
  return result;
}

void comandResult(){
    memcpy_P(ether.tcpOffset(), page, sizeof page);
    ether.httpServerReply(sizeof page - 1);
}

void server(
  float volt01, float volt02, float volt03,
  float amp01, float amp02, float amp03,
  int dig01, int dig02, int dig03, 
  int rele01, int rele02, int rele03, int rele04,
  int ac, char * msdg
  ){

   String page = "HTTP/1.1 200 OK\r\n"
    "Content-Type: application/json\r\n"
    "Access-Control-Allow-Origin: *\r\n"
    "Access-Control-Allow-Headers: Content-Type\r\n"
    "Access-Control-Allow-Methods: GET\r\n"
    "Retry-After: 1000\r\n"
    "\r\n"
    "{";

    page = String(page +
      "\"voltimetro\":{" +
          "\"volt01\":" + volt01 +
          ",\"volt02\":" + volt02 +
          ",\"volt03\":" + volt03 +
      "},\"amperimetro\":{"+
          "\"amp01\":" + amp01 +
          ",\"amp02\":" + amp02 +
          ",\"amp03\":" + amp03 +
      "},\"digitalIn\":{" +
          "\"dig01\":" + dig01 +
          ",\"dig02\":" + dig02 +
          ",\"dig03\":" + dig03 +
      "},\"acIn\":{" + 
          "\"acStatus\":" + ac +
      "},\"releStatus\":{" + 
          "\"rele01\":" + rele01 +
          ",\"rele01\":" + rele02 +
          ",\"rele01\":" + rele03 +
          ",\"rele01\":" + rele04 +
      "}}  " 
      );

    msdg = &page[0];

    int s = page.length();

    memcpy(ether.tcpOffset(), msdg, s);
    ether.httpServerReply(s - 1);
}

String converter(uint8_t *str){
    return String((char *)str);
}

void resAction(word pos){
  // pulsa rele 01
  if(strstr((char *)Ethernet::buffer + pos, "GET /?RELE01_PULSE") != 0) {
    Serial.println("Received ON command");
    blinkLedOrRele(releOut01, 100, 1, false);
    comandResult();
  }
  // pulsa rele 02
  if(strstr((char *)Ethernet::buffer + pos, "GET /?RELE02_PULSE") != 0) {
    Serial.println("Received ON command");
    blinkLedOrRele(releOut02, 100, 1, false);
    comandResult();
  }
  // pulsa rele 03
  if(strstr((char *)Ethernet::buffer + pos, "GET /?RELE03_PULSE") != 0) {
    Serial.println("Received ON command");
    blinkLedOrRele(releOut03, 100, 1, false);
    comandResult();
  }
  // pulsa rele 04
  if(strstr((char *)Ethernet::buffer + pos, "GET /?RELE04_PULSE") != 0) {
    Serial.println("Received ON command");
    blinkLedOrRele(releOut04, 100, 1, false);
    comandResult();
  }
  // liga rele 01
  if(strstr((char *)Ethernet::buffer + pos, "GET /?RELE01=ON") != 0) {
    Serial.println("Received ON command");
    digitalWrite(releOut01, HIGH);
    comandResult();
  }
  // desliga rele 01
  if(strstr((char *)Ethernet::buffer + pos, "GET /?RELE01=OFF") != 0) {
    Serial.println("Received ON command");
    digitalWrite(releOut01, LOW);
    comandResult();
  }
  // liga rele 02
    if(strstr((char *)Ethernet::buffer + pos, "GET /?RELE02=ON") != 0) {
    Serial.println("Received ON command");
    digitalWrite(releOut02, HIGH);
    comandResult();
  }
  // desliga rele 02
  if(strstr((char *)Ethernet::buffer + pos, "GET /?RELE02=OFF") != 0) {
    Serial.println("Received ON command");
    digitalWrite(releOut02, LOW);
    comandResult();
  }
  // liga rele 03
  if(strstr((char *)Ethernet::buffer + pos, "GET /?RELE03=ON") != 0) {
    Serial.println("Received ON command");
    digitalWrite(releOut03, HIGH);
    comandResult();
  }
 // desliga rele 03
  if(strstr((char *)Ethernet::buffer + pos, "GET /?RELE03=OFF") != 0) {
    Serial.println("Received ON command");
    digitalWrite(releOut03, LOW);
    comandResult();
  }
  // liga rele 04
  if(strstr((char *)Ethernet::buffer + pos, "GET /?RELE04=ON") != 0) {
    Serial.println("Received ON command");
    digitalWrite(releOut04, HIGH);
    comandResult();
  }
  // desliga rele 04
  if(strstr((char *)Ethernet::buffer + pos, "GET /?RELE04=OFF") != 0) {
    Serial.println("Received ON command");
    digitalWrite(releOut04, LOW);
    comandResult();
  }
}