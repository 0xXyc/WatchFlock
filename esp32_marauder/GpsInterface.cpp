#include "GpsInterface.h"

#ifdef HAS_GPS

extern GpsInterface gps_obj;

char nmeaBuffer[100];

MicroNMEA nmea(nmeaBuffer, sizeof(nmeaBuffer));

// Serial2 is provided by arduino-esp32 core 3.x for the C5; do not redefine.
// HardwareSerial Serial2(GPS_SERIAL_INDEX);

// Pick the correct HardwareSerial instance for GPS based on board config.
// MARAUDER_C5 wires GPS to UART1 (Serial1), other boards to UART2 (Serial2).
#if GPS_SERIAL_INDEX == 1
  #define GpsSerial Serial1
#elif GPS_SERIAL_INDEX == 0
  #define GpsSerial Serial
#else
  #define GpsSerial Serial2
#endif

static const char *PCAS_SET_115200 = "$PCAS01,5*19\r\n";

static const uint32_t PROBE_MS = 1200;

void GpsInterface::begin() {

  
  GpsSerial.begin(9600, SERIAL_8N1, GPS_TX, GPS_RX);

  uint32_t gps_baud = this->initGpsBaudAndForce115200();

  if ((gps_baud != 9600) && (gps_baud != 115200))
    Serial.println(F("Could not detect GPS baudrate"));

  // Track what baud the UART is actually at after probing. If detection
  // failed (gps_baud == 0) the last probeBaud() call in init left us at
  // 9600, record that so the recovery logic in main() knows what to
  // swap away from. If detection succeeded, lock to that baud.
  this->gps_current_baud = (gps_baud == 115200) ? 115200 : 9600;
  this->gps_baud_locked  = (gps_baud != 0);

  delay(1000);

  MicroNMEA::sendSentence(GpsSerial, "$PSTMSETPAR,1201,0x00000042");
  MicroNMEA::sendSentence(GpsSerial, "$PSTMSAVEPAR");

  // Soft reset ($PSTMSRR). Earlier session commented this out to avoid
  // wiping almanac/ephemeris on every C5 boot (kokollc adapter doesn't
  // power the GPS module's VBAT backup line, so every reset = full cold
  // start ~12 min TTFF). BUT removing the reset put the module into a
  // near-silent state (~3 bytes/min observed 2026-05-20), apparently
  // because the prior $PSTMSETPAR/$PSTMSAVEPAR doesn't take effect until
  // the module restarts its NMEA pipeline. Restoring SRR: we eat the
  // cold-start TTFF cost as the cost of having a talking module at all.
  // A smarter fix later: skip SRR if last fix is recent and seed via
  // $PSTMINITGPS instead of forcing a full restart.
  MicroNMEA::sendSentence(GpsSerial, "$PSTMSRR");

  delay(1000);

  if (GpsSerial.available()) {
    this->gps_enabled = true;
    while (GpsSerial.available()) {
      //Fetch the character one by one
      char c = GpsSerial.read();
      //Serial.print(c);
      //Pass the character to the library
      nmea.process(c);
    }
  }
  else {
    this->gps_enabled = false;
    Serial.println(F("GPS Not Found"));
  }
  

  this->type_flag=GPSTYPE_NATIVE; //enforce default
  this->disable_queue(); //init the queue, disabled, kill NULLs

  nmea.setUnknownSentenceHandler(gps_nmea_notimp);

}

bool GpsInterface::probeBaud(uint32_t baud) {
  GpsSerial.end();
  delay(50);

  GpsSerial.begin(baud, SERIAL_8N1, GPS_TX, GPS_RX);

  uint32_t start = millis();
  bool sawDollar = false;
  bool parsedSentence = false;

  while (millis() - start < PROBE_MS) {
    while (GpsSerial.available()) {
      char c = (char)GpsSerial.read();

      if (c == '$') {
        sawDollar = true;
      }

      // Feed characters directly to MicroNMEA
      if (nmea.process(c)) {
        parsedSentence = true;
      }

      // If we’ve seen real NMEA traffic and MicroNMEA parsed something,
      // this baud is almost certainly correct
      if (sawDollar && parsedSentence) {
        return true;
      }
    }
    delay(1);
  }

  return false;
}

void GpsInterface::setGpsTo115200From9600() {
  GpsSerial.print(PCAS_SET_115200);
  GpsSerial.flush();
  delay(200);
}

uint32_t GpsInterface::initGpsBaudAndForce115200() {
  if (probeBaud(115200)) {
    return 115200;
  }

  if (probeBaud(9600)) {
    setGpsTo115200From9600();

    if (probeBaud(115200)) {
      return 115200;
    }

    probeBaud(9600);
    return 9600;
  }

  probeBaud(9600);
  return 0;
}

//passthrough for other objects
void gps_nmea_notimp(MicroNMEA& nmea){
  gps_obj.enqueue(nmea);
}

void GpsInterface::enqueue(MicroNMEA& nmea){
  std::string nmea_sentence = std::string(nmea.getSentence());

  if(nmea_sentence.length()){
    this->notimp_nmea_sentence = nmea_sentence.c_str();

    bool unparsed=1;
    bool enqueue=1;

    char system=nmea.getTalkerID();
    String msg_id=nmea.getMessageID();
    int length=nmea_sentence.length();

    if(length>0&&length<256){
      if(system){
        if(msg_id=="TXT"){
          if(length>8){
            std::string content=nmea_sentence.substr(7,std::string::npos);

            int tot_brk=content.find(',');
            int num_brk=content.find(',',tot_brk+1);
            int txt_brk=content.find(',',num_brk+1);
            int chk_brk=content.rfind('*');

            if(tot_brk!=std::string::npos && num_brk!=std::string::npos && txt_brk!=std::string::npos && chk_brk!=std::string::npos
                && chk_brk>txt_brk && txt_brk>num_brk && num_brk>tot_brk && tot_brk>=0){
              std::string total_str=content.substr(0,tot_brk);
              std::string num_str=content.substr(tot_brk+1,num_brk-tot_brk-1);
              std::string type_str=content.substr(num_brk+1,txt_brk-num_brk-1);
              std::string text_str=content.substr(txt_brk+1,chk_brk-txt_brk-1);
              std::string checksum=content.substr(chk_brk+1,std::string::npos);

              int total=0;
              if(total_str.length()) total=atoi(total_str.c_str());

              int num=0;
              if(num_str.length()) num=atoi(num_str.c_str());

              int type=0;
              if(type_str.length()) type=atoi(type_str.c_str());

              if(text_str.length() && checksum.length()){
                String text=text_str.c_str();
                if(type>1){
                  char type_cstr[4];
                  snprintf(type_cstr, 4, "%02d ", type);
                  type_cstr[3]='\0';
                  text=type_cstr+text;
                }

                if((num<=1||total<=1) && this->queue_enabled_flag){
                  if(this->text){
                    if(this->text_in){
                      int size=text_in->size();
                      if(size){
                        #ifdef GPS_TEXT_MAXCYCLES
                          if(this->text_cycles>=GPS_TEXT_MAXCYCLES){
                        #else
                          if(this->text_cycles){
                        #endif
                            if(this->text->size()){
                              LinkedList<String> *delme=this->text;
                              this->text=new LinkedList<String>;
                              delete delme;
                              this->text_cycles=0;
                            }
                          }
                        
                        for(int i=0;i<size;i++){
                          this->text->add(this->text_in->get(i));
                        }
                        LinkedList<String> *delme=this->text_in;
                        this->text_in=new LinkedList<String>;
                        delete delme;
                        this->text_cycles++;

                        this->gps_text=text;
                      }
                    }
                    else
                      this->text_in=new LinkedList<String>;
                  }
                  else{
                    if(this->text_in){
                      this->text_cycles=0;
                      this->text=this->text_in;
                      if(this->text->size()){
                        if(this->gps_text=="") this->gps_text=this->text->get(0);
                        this->text_cycles++;
                      }
                      this->text_in=new LinkedList<String>;
                    }
                    else {
                      this->text_cycles=0;
                      this->text=new LinkedList<String>;
                      this->text_in=new LinkedList<String>;
                    }
                  }

                  this->text_in->add(text);
                }
                else if(this->queue_enabled_flag){
                  if(!this->text_in) this->text_in=new LinkedList<String>;
                  this->text_in->add(text);
                  int size=this->text_in->size();

                  #ifdef GPS_TEXT_MAXLINES
                    if(size>=GPS_TEXT_MAXLINES){
                  #else
                    if(size>=5){
                  #endif
                      #ifdef GPS_TEXT_MAXCYCLES
                        if(this->text_cycles>=GPS_TEXT_MAXCYCLES){
                      #else
                        if(this->text_cycles){
                      #endif
                          if(this->text->size()){
                            LinkedList<String> *delme=this->text;
                            this->text=new LinkedList<String>;
                            delete delme;
                            this->text_cycles=0;
                          }
                        }
                      
                        for(int i=0;i<size;i++)
                          this->text->add(this->text_in->get(i));

                        LinkedList<String> *delme=this->text_in;
                        this->text_in=new LinkedList<String>;
                        delete delme;
                        this->text_cycles++;
                      }
                }
                else
                  if(num<=1||total<=1) this->gps_text=text;

                if(this->gps_text=="") this->gps_text=text;
                unparsed=0;
              }
            }
          }
        }
      }
    }

    if(unparsed)
      this->notparsed_nmea_sentence = nmea_sentence.c_str();

    if(this->queue_enabled_flag){
      if(enqueue){
        nmea_sentence_t line = { unparsed, msg_id, nmea_sentence.c_str() };

        if(this->queue){
          #ifdef GPS_NMEA_MAXQUEUE
            if(this->queue->size()>=GPS_NMEA_MAXQUEUE)
          #else
            if(this->queue->size()>=30)
          #endif
              this->flush_queue();
        }
        else
           this->new_queue();

        this->queue->add(line);
      }
      else
        if(!this->queue)
          this->new_queue();
    }
    else
      this->flush_queue();
  }
  else
    if(!this->queue_enabled_flag)
      this->flush_queue();
}

void GpsInterface::enable_queue(){
  if(this->queue_enabled_flag){
    if(!this->queue)
      this->new_queue();
    if(!this->text)
      this->text=new LinkedList<String>;
    if(!this->text_in)
      this->text_in=new LinkedList<String>;
  }
  else {
    this->flush_queue();
    this->queue_enabled_flag=1;
  }
}

void GpsInterface::disable_queue(){
  this->queue_enabled_flag=0;
  this->flush_queue();
}

bool GpsInterface::queue_enabled(){
  return this->queue_enabled_flag;
}

LinkedList<nmea_sentence_t>* GpsInterface::get_queue(){
  return this->queue;
}

void GpsInterface::new_queue(){
  this->queue=new LinkedList<nmea_sentence_t>;
}

void GpsInterface::flush_queue(){
  this->flush_queue_nmea();
  this->flush_text();
}

void GpsInterface::flush_queue_nmea(){
  if(this->queue){
    if(this->queue->size()){
      LinkedList<nmea_sentence_t> *delme=this->queue;
      this->new_queue();
      delete delme;
    }
  }
  else
    this->new_queue();
}

void GpsInterface::flush_text(){
  this->flush_queue_text();
  this->flush_queue_textin();
}

void GpsInterface::flush_queue_text(){
  this->text_cycles=0;

  if(this->text){
    if(this->text->size()){
      LinkedList<String> *delme=this->text;
      this->text=new LinkedList<String>;
      delete delme;
    }
  }
  else
    this->text=new LinkedList<String>;
}

void GpsInterface::flush_queue_textin(){
  if(this->text_in){
    if(this->text_in->size()){
      LinkedList<String> *delme=this->text_in;
      this->text_in=new LinkedList<String>;
      delete delme;
    }
  }
  else
    this->text_in=new LinkedList<String>;
}

void GpsInterface::sendSentence(const char* sentence){
  MicroNMEA::sendSentence(GpsSerial, sentence);
}

void GpsInterface::sendSentence(Stream &s, const char* sentence){
  MicroNMEA::sendSentence(s, sentence);
}

void GpsInterface::setType(String t){
  if(t == "native")
    this->type_flag=GPSTYPE_NATIVE;
  else if(t == "gps")
    this->type_flag=GPSTYPE_GPS;
  else if(t == "glonass")
    this->type_flag=GPSTYPE_GLONASS;
  else if(t == "galileo")
    this->type_flag=GPSTYPE_GALILEO;
  else if(t == "navic")
    this->type_flag=GPSTYPE_NAVIC;
  else if(t == "qzss")
    this->type_flag=GPSTYPE_QZSS;        
  else if(t == "beidou")
    this->type_flag=GPSTYPE_BEIDOU;
  else if(t == "beidou_bd")
    this->type_flag=GPSTYPE_BEIDOU_BD;    
  else
    this->type_flag=GPSTYPE_ALL;
}

String GpsInterface::generateGXgga(){
  String msg_type="$"+this->generateType()+"GGA,";

  char timeStr[8];
  snprintf(timeStr, 8, "%02d%02d%02d,", (int)(nmea.getHour()), (int)(nmea.getMinute()), (int)(nmea.getSecond()));

  long lat = nmea.getLatitude();
  char latDir = lat < 0 ? 'S' : 'N';
  lat = abs(lat);
  char latStr[12];
  snprintf(latStr, 12, "%02ld%08.5f,", lat / 1000000, ((lat % 1000000)*60) / 1000000.0);

  long lon = nmea.getLongitude();
  char lonDir = lon < 0 ? 'W' : 'E';
  lon = abs(lon);
  char lonStr[13];
  snprintf(lonStr, 13, "%03ld%08.5f,", lon / 1000000, ((lon % 1000000)*60) / 1000000.0);

  int fixQuality = nmea.isValid() ? 1 : 0;
  char fixStr[3];
  snprintf(fixStr, 3, "%01d,", fixQuality);

  int numSatellites = nmea.getNumSatellites();
  char satStr[4];
  snprintf(satStr, 4, "%02d,", numSatellites);

  unsigned long hdop = nmea.getHDOP();
  char hdopStr[13];
  snprintf(hdopStr, 13, "%01.2f,", 2.5 * (((float)(hdop))/10));

  long altitude;
  if(!nmea.getAltitude(altitude)) altitude=0;
  char altStr[9];
  snprintf(altStr, 9, "%01.1f,", altitude/1000.0);

  String message = msg_type + timeStr + latStr + latDir + ',' + lonStr + lonDir +
                    ',' + fixStr + satStr + hdopStr + altStr + "M,,M,,";

  return message;
}

String GpsInterface::generateGXrmc(){
  String msg_type="$"+this->generateType()+"RMC,";

  char timeStr[8];
  snprintf(timeStr, 8, "%02d%02d%02d,", (int)(nmea.getHour()), (int)(nmea.getMinute()), (int)(nmea.getSecond()));

  char dateStr[8];
  snprintf(dateStr, 8, "%02d%02d%02d,", (int)(nmea.getDay()), (int)(nmea.getMonth()), (int)(nmea.getYear()%100));

  char status = nmea.isValid() ? 'A' : 'V';
  char mode = nmea.isValid() ? 'A' : 'N';

  long lat = nmea.getLatitude();
  char latDir = lat < 0 ? 'S' : 'N';
  lat = abs(lat);
  char latStr[12];
  snprintf(latStr, 12, "%02ld%08.5f,", lat / 1000000, ((lat % 1000000)*60) / 1000000.0);

  long lon = nmea.getLongitude();
  char lonDir = lon < 0 ? 'W' : 'E';
  lon = abs(lon);
  char lonStr[13];
  snprintf(lonStr, 13, "%03ld%08.5f,", lon / 1000000, ((lon % 1000000)*60) / 1000000.0);

  char speedStr[8];
  snprintf(speedStr, 8, "%01.1f,", nmea.getSpeed() / 1000.0);

  char courseStr[7];
  snprintf(courseStr, 7, "%01.1f,", nmea.getCourse() / 1000.0);

  String message = msg_type + timeStr + status + ',' + latStr + latDir + ',' +
                    lonStr + lonDir + ',' + speedStr + courseStr + dateStr + ',' + ',' + mode;
  return message;
}

String GpsInterface::generateType(){
  String msg_type="";

  if(this->type_flag<8) //8=BeiDou in BD mode
    msg_type+='G';

  if(this->type_flag == GPSTYPE_NATIVE){ //type_flag=0
    char system=this->nav_system;
    if(system)
      msg_type+=system;
    else
      msg_type+='N';
  }
  else if(this->type_flag == GPSTYPE_GPS) //type_flag=2
    msg_type+='P';
  else if(this->type_flag == GPSTYPE_GLONASS) //type_flag=3
    msg_type+='L';
  else if(this->type_flag == GPSTYPE_GALILEO) //type_flag=4
    msg_type+='A';
  else if(this->type_flag == GPSTYPE_NAVIC) //type_flag=5
    msg_type+='I';
  else if(this->type_flag == GPSTYPE_QZSS) //type_flag=6
    msg_type+='Q';
  else if(this->type_flag == GPSTYPE_BEIDOU) //type_flag=7
    msg_type+='B';
  else if(this->type_flag == GPSTYPE_BEIDOU_BD){ //type_flag=8
    msg_type+='B';
    msg_type+='D';
  }
  else{ //type_flag=1=all ... also default if unset/wrong (obj default is type_flag=0=native)
    if(this->type_flag>=8) //catch uncaught first char, assume G if not already output
      msg_type+='G';
    msg_type+='N';
  }

  return msg_type;
}

// Thanks JosephHewitt
String GpsInterface::dt_string_from_gps(){
  //Return a datetime String using GPS data only.
  String datetime = "";
  if (nmea.isValid() && nmea.getYear() > 0){
    datetime += nmea.getYear();

    datetime += "-";

    uint8_t month = nmea.getMonth();
    if (month < 10)
      datetime += "0";
    datetime += month;

    datetime += "-";

    uint8_t day = nmea.getDay();
    if (day < 10)
      datetime += "0";
    datetime += day;

    datetime += " ";

    uint8_t hour = nmea.getHour();
    if (hour < 10)
      datetime += "0";
    datetime += hour;

    datetime += ":";

    uint8_t minute = nmea.getMinute();
    if (minute < 10)
      datetime += "0";
    datetime += minute;

    datetime += ":";
    
    uint8_t seconds = nmea.getSecond();
    if (seconds < 10)
      datetime += "0";
    datetime += seconds;
  }
  return datetime;
}

void GpsInterface::setGPSInfo() {
  String nmea_sentence = String(nmea.getSentence());
  if(nmea_sentence != "") this->nmea_sentence = nmea_sentence;

  this->good_fix = nmea.isValid();
  this->nav_system = nmea.getNavSystem();
  this->num_sats = nmea.getNumSatellites();

  this->datetime = this->dt_string_from_gps();

  this->lat_int = nmea.getLatitude();
  this->lon_int = nmea.getLongitude();

  this->lat = String((float)nmea.getLatitude()/1000000, 7);
  this->lon = String((float)nmea.getLongitude()/1000000, 7);
  long alt = 0;
  if (!nmea.getAltitude(alt)){
    alt = 0;
  }
  this->altf = (float)alt / 1000;

  this->accuracy = 2.5 * ((float)nmea.getHDOP()/10);

  //nmea.clear();
}

float GpsInterface::getAccuracy() {
  return this->accuracy;
}

String GpsInterface::getLat() {
  return this->lat;
}

String GpsInterface::getLon() {
  return this->lon;
}

int32_t GpsInterface::getLatInt() {
  return this->lat_int;
}

int32_t GpsInterface::getLonInt() {
  return this->lon_int;
}

float GpsInterface::getAlt() {
  return this->altf;
}

String GpsInterface::getDatetime() {
  return this->datetime;
}

String GpsInterface::getNumSatsString() {
  return (String)num_sats;
}

int GpsInterface::getNumSats() {
  return num_sats;
}

bool GpsInterface::getFixStatus() {
  return this->good_fix;
}

String GpsInterface::getFixStatusAsString() {
  if (this->getFixStatus())
    return "Yes";
  else
    return "No";
}

bool GpsInterface::getGpsModuleStatus() {
  return this->gps_enabled;
}

uint32_t GpsInterface::getParsedCount() {
  return this->gps_parsed_count;
}

uint32_t GpsInterface::getCurrentBaud() {
  return this->gps_current_baud;
}

uint32_t GpsInterface::getBytesTotal() {
  return this->gps_bytes_total;
}

String GpsInterface::getText() {
  return this->gps_text;
}

int GpsInterface::getTextQueueSize() {
  if(this->queue_enabled_flag){
    bool exists=0;
    if(this->text){
      int size=this->text->size();
      if(size) return size;
      exists=1;
    }
    if(this->text_in){
      int size=this->text_in->size();
      if(size) return size;
      exists=1;
    }
    if(exists)
      return 0;
    else
      return -2;
  }
  else
    return -1;
}

String GpsInterface::getTextQueue(bool flush) {
  if(this->queue_enabled_flag){
    if(this->text){
      int size=this->text->size();
      if(size){
        String text;
        for(int i=0;i<size;i++){
          String now=this->text_in->get(i);
          if(now!=""){
            if(text!=""){
              text+='\r';
              text+='\n';
            }
            text+=now;
          }
        }
        if(flush){
          LinkedList<String> *delme=this->text;
          this->text_cycles=0;
          this->text=this->text_in;
          if(!this->text) this->text=new LinkedList<String>;
          if(this->text->size()) this->text_cycles++;
          this->text_in=new LinkedList<String>;
          delete delme;
        }
        return text;
      }
    }
    else{
      this->text=new LinkedList<String>;
      this->text_cycles=0;
    }

    if(this->text_in){
      int size=this->text_in->size();
      if(size){
        LinkedList<String> *buffer=this->text_in;
        if(flush)
          this->text_in=new LinkedList<String>;
        String text;
        for(int i=0;i<size;i++){
          String now=buffer->get(i);
          if(now!=""){
            if(text!=""){
              text+='\r';
              text+='\n';
            }
            text+=now;
          }
        }
        if(flush)
          delete buffer;
        return text;
      }
    }
    else
      this->text_in=new LinkedList<String>;

    return this->gps_text;
  }
  else
    return this->gps_text;
}

String GpsInterface::getNmea() {
  return this->nmea_sentence;
}

String GpsInterface::getNmeaNotimp() {
  return this->notimp_nmea_sentence;
}

String GpsInterface::getNmeaNotparsed() {
  return this->notparsed_nmea_sentence;
}

void GpsInterface::main() {
  bool got_bytes = false;
  while (GpsSerial.available()) {
    char c = GpsSerial.read();
    this->gps_bytes_total++;
    if (nmea.process(c)) {
      this->gps_parsed_count++;
      this->gps_baud_locked = true;
    }
    got_bytes = true;
  }

  // Module-alive heartbeat: begin() probes once at boot, but on the kokollc
  // adapter the GPS module can be slow to wake and the begin-time probe
  // sometimes returns no data even though the module is healthy. If we
  // ever see bytes here, the module is talking, flip gps_enabled true so
  // STAT emits gps=ok|nofix correctly instead of gps=nofix forever.
  if (got_bytes) this->gps_enabled = true;

  // Runtime baud recovery: if bytes are flowing but no NMEA sentence has
  // ever parsed, we're talking to the module at the wrong rate. Swap
  // 9600 <-> 115200 every ~5 seconds until a parse succeeds, then lock.
  // Module wake on the kokollc adapter is non-deterministic, observed
  // 1-8 seconds after C5 power-on, so begin()'s one-shot probe misses
  // it on cold starts and leaves the UART at 9600 while the module
  // (NVRAM-configured to 115200) talks past it.
  if (!this->gps_baud_locked && this->gps_recovery_attempts < 8) {
    uint32_t now = millis();
    if (this->gps_last_recovery_ms == 0) this->gps_last_recovery_ms = now;
    if (now - this->gps_last_recovery_ms > 5000) {
      // Trigger on ANY bytes seen (framing errors at wrong baud often drop
      // most bytes). gps_enabled is the heartbeat, "we ever saw a byte."
      if (this->gps_enabled && this->gps_parsed_count == 0) {
        uint32_t new_baud = (this->gps_current_baud == 115200) ? 9600 : 115200;
        GpsSerial.end();
        delay(50);
        GpsSerial.begin(new_baud, SERIAL_8N1, GPS_TX, GPS_RX);
        this->gps_current_baud = new_baud;
        this->gps_recovery_attempts++;
        Serial.printf("GPS baud-recovery #%u: swapped to %u\n",
                      this->gps_recovery_attempts, new_baud);
      }
      this->gps_last_recovery_ms = now;
    }
  }

  uint8_t num_sat = nmea.getNumSatellites();

  if ((nmea.isValid()) && (num_sat > 0))
    this->setGPSInfo();

  else if ((!nmea.isValid()) && (num_sat <= 0)) {
    this->setGPSInfo();
  }
}
#endif
