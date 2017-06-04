#include <EtherCard.h>

const char pageA[] PROGMEM =
"HTTP/1.0 200 OK\r\n"
"Content-Type: text/plain\r\n"
"\r\n";

#define STATIC 0  // set to 1 to disable DHCP (adjust myip/gwip values below)

#define ETH_RS_PIN 18

#define ETH_CS_PIN 8

#if STATIC
// ethernet interface ip address
static byte myip[] = { 192,168,1,111 };
// gateway ip address
static byte gwip[] = { 192,168,1,1 };
#endif

// ethernet mac address - must be unique on your network
static byte mymac[] = { 0x74,0x69,0x69,0x2D,0x30,0x31 };

byte Ethernet::buffer[512]; // tcp/ip send and receive buffer

BufferFiller bfill;

// called when a ping comes in (replies to it are automatic)
static void gotPinged (byte* ptr) {
  ether.printIp(">>> ping from: ", ptr);
}

static uint32_t timer;                  // This is a ping timer.
static uint32_t timerWatchdog;  // This is a watchdog timer
static uint32_t watchdogDuration;  // This is a watchdog timer
static int numTries = 1;       //  total number of attempts at tripping the switch
static int resetsCounter = 0;

void etherInit() {
  digitalWrite(ETH_RS_PIN, LOW); // this resets the ENC28J60 hardware
  pinMode(ETH_RS_PIN, OUTPUT); // this lets you pull the pin low.
  delay(100); // this makes sure the ENC28j60 resets OK.
  pinMode(ETH_RS_PIN, INPUT); // this releases the pin,(puts it in high impedance); lets the pullup in the board do its job.

    if (ether.begin(sizeof Ethernet::buffer, mymac, ETH_CS_PIN) == 0) 
    Serial.println( "Failed to access Ethernet controller");
#if STATIC
  ether.staticSetup(myip, gwip);
#else
  if (!ether.dhcpSetup())
    Serial.println("DHCP failed");
#endif
  // call this to report others pinging us
  ether.registerPingCallback(gotPinged);
  
  ether.copyIp(ether.hisip, ether.gwip);  

  ether.printIp("IP:  ", ether.myip);
  ether.printIp("GW:  ", ether.gwip);  
  ether.printIp("DNS: ", ether.dnsip);
  ether.printIp("SRV: ", ether.hisip);
    
  // call this to report others pinging us
  ether.registerPingCallback(gotPinged);
  
  timer = -9999999; // start timing out right away
  timerWatchdog = millis(); // reset the watchdog
}

void networkLoop() {
    word len = ether.packetReceive(); // go receive new packets
  word pos = ether.packetLoop(len); // respond to incoming pings
  
  // First attempt triggers in every 1 minutes
  watchdogDuration = numTries*1*60000;
  
  // report whenever a reply to our outgoing ping comes back 
  if (len > 0 && ether.packetLoopIcmpCheckReply(ether.hisip)) {
    Serial.print("  ");
    Serial.print((micros() - timer) * 0.001, 3);
    Serial.println(" ms");
    
    // reset the watchdog and its fail counter
    Serial.println(">> RESETTING WATCHDOG");
    timerWatchdog = millis(); 
    numTries = 1;
  }
  else if (pos)  {// check if valid tcp data is received
    char* d = (char *) Ethernet::buffer + pos;
    //Serial.println((char *) Ethernet::buffer + pos);

    ether.httpServerReplyAck(); // send ack to the request
    
    processGet((char *) Ethernet::buffer + pos);
        
    memcpy_P(ether.tcpOffset(), pageA, sizeof pageA);//only the first part will sended
    ether.httpServerReply_with_flags(sizeof pageA - 1,TCP_FLAGS_ACK_V);

    byte toutputs[sizeof(outputs) / sizeof(OUTPUT_STRUCT)];
    for (int i = 0; i < sizeof(toutputs); i++) {
      sendOutput(i);
      ether.httpServerReply_with_flags(bfill.position(),TCP_FLAGS_ACK_V);
    }
    
    sendUptime();
    ether.httpServerReply_with_flags(bfill.position(), TCP_FLAGS_ACK_V|TCP_FLAGS_FIN_V);
  }
  
  // ping a remote server once every few seconds (10 secs)
  if (micros() - timer >= 10*1000000) {
    ether.printIp("Pinging: ", ether.hisip);
    timer = micros();
    ether.clientIcmpRequest(ether.hisip);
    //Watchdog status
    Serial.print("noTries :");
    Serial.print(numTries);
    Serial.print(", Watchdog Duration: ");
    Serial.println(millis() - timerWatchdog);
  }
  
  // Trip a relay after 1 minutes minimum
  if (millis() - timerWatchdog >= watchdogDuration) {
    Serial.println(">> WATCHDOG TIMED OUT");
    // Wait for the internet to try and reconnect, then reinitailize
     Serial.println(">> Waiting for reconnection");
    delay(numTries*1000); 
    etherInit();
    // Reset the watchdog and increment attempt
    timerWatchdog = millis(); // reset the watchdog
    numTries ++;
    resetsCounter++;
    Serial.print("Attempt :");
    Serial.println(numTries);

  }
}

void sendUptime() {
    long t = millis() / 1000;
    word h = t / 3600;
    byte m = (t / 60) % 60;
    byte s = t % 60;

    bfill = ether.tcpOffset();
    bfill.emit_p(
      PSTR("\r\nCzas pracy: $D$D:$D$D:$D$D   r:$D\r\n"),
        h/10, h%10, m/10, m%10, s/10, s%10, resetsCounter);
}

void sendOutput(byte index) {
    bfill = ether.tcpOffset();
    byte value = outputs[index].value;
    if (value > 0) {
      long t = (long) 60 * value - (millis() / 1000 - outputs[index].timestmap);
      word h = t / 3600;
      byte m = (t / 60) % 60;
      byte s = t % 60;

      bfill.emit_p(
        PSTR("Wyj $D: $D:$D$D:$D$D\r\n"),
          index + 1 , h%10, m/10, m%10, s/10, s%10);
    }
    else {
      bfill.emit_p(
        PSTR("Wyj $D: WYL\r\n"), index + 1);
    }
}

const int MAX_PARAM = 10;

void processGet (const char * data)
  {
  // find where the parameters start
  const char * paramsPos = strchr (data, '?');
  if (paramsPos == NULL)
    return;  // no parameters
  // find the trailing space
  const char * spacePos = strchr (paramsPos, ' ');
  if (spacePos == NULL)
    return;  // no space found
  // work out how long the parameters are
  int paramLength = spacePos - paramsPos - 1;
  // see if too long
  if (paramLength >= MAX_PARAM)
    return;  // too long for us
  // copy parameters into a buffer
  char param [MAX_PARAM];
  memcpy (param, paramsPos + 1, paramLength);  // skip the "?"
  param [paramLength] = 0;  // null terminator

  // do things depending on argument (GET parameters)

  Serial.print (millis() / 1000);
  Serial.print (" : ");
  Serial.println (param);
  String paramStr = String(param);
  if (paramStr.startsWith("sw") && param[3] == '=' && isAlphaNumeric(param[4])) {
    byte outputState = (byte) (param[4] - 48);
    int paramValue = String(param + 4).toInt();
    if (isDigit(param[2])) {
      byte output = (byte) (param[2] - 48);
      Serial.println (output);
      if (output < sizeof(outputs) / sizeof(OUTPUT_STRUCT)) {
        processOutput(output, paramValue);
      }
    }
    else if (param[2] == 'a') {
      for (byte i = 0; i < 8; i++){
          processOutput(i, paramValue);
      }
    }
  }
  else if (paramStr.startsWith("dv") && param[2] == '=' && isAlphaNumeric(param[3])  && isAlphaNumeric(param[4])) {
      defaultValue = String(param + 3).toInt() + String(param + 4).toInt();
      Serial.print("defaultValue: ");Serial.println(defaultValue);
      EEPROM.write(DEFAULT_VALUE_POS, defaultValue);
    }
}  // end of processGet
