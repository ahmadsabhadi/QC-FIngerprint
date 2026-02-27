//SQL
#undef SQLITE_ESP32VFS_BUFFERSZ
#define SQLITE_ESP32VFS_BUFFERSZ 1024

//BLUETOOTH
#include <BluetoothSerial.h>
#undef RX_QUEUE_SIZE
#define RX_QUEUE_SIZE 6000

#include <Arduino.h>

#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include "time.h"
#include "sntp.h"
RTC_DATA_ATTR uint32_t nomor_serial;
const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";
const long  gmtOffset_sec = 7*3600;
const int   daylightOffset_sec = 3600;

BluetoothSerial SerialBT;
JsonDocument newsn;
void merge(JsonVariant dst, JsonVariantConst src)
{
  if (src.is<JsonObjectConst>())
  {
    for (JsonPairConst kvp : src.as<JsonObjectConst>())
    {
      if (dst[kvp.key()]) 
        merge(dst[kvp.key()], kvp.value());
      else
        dst[kvp.key()] = kvp.value();
    }
  }
  else
  {
    dst.set(src);
  }
}

uint32_t getID32(){
          uint32_t low     = ESP.getEfuseMac() & 0xFFFFFFFF; 
          uint32_t high    = ( ESP.getEfuseMac() >> 32 ) % 0xFFFFFFFF;
          //Serial.println(low);
          //Serial.println(high);
          uint32_t id32=low+high;
          return id32;
}

String mac_wifi_to_bt(const char* mac){
  char mac_wifi [25];
  strcpy(mac_wifi,mac);
  char hex [3];
  hex[0]=mac_wifi[15];
  hex[1]=mac_wifi[16];
  hex[2]=0;
  //Serial.println(mac_wifi);
  uint8_t number=strtol(hex,NULL,16);
  char hexStringNew[3];
  hexStringNew[2]=0;
  sprintf(hexStringNew,"%02X",number+2);
  //Serial.println(hexStringNew);
  mac_wifi[15]=hexStringNew[0];
  mac_wifi[16]=hexStringNew[1];
  mac_wifi[17]=0;
  //Serial.println(mac_wifi);
  //doc["mac_bluetooth"]=mac_wifi;
  return mac_wifi;
}

uint32_t mac_to_chip_id(const char* mac){
  char mac_wifi [25];
  strcpy(mac_wifi,mac);
  //94:B5:55:25:86:68
  //Low 25:55:B5:94
  //HIGH 68:86
  uint8_t bssid[6];
  sscanf(mac_wifi, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &bssid[0], &bssid[1], &bssid[2], &bssid[3], &bssid[4], &bssid[5]);
  uint32_t high     = bssid[4]|bssid[5]<<8&0xFFFFFFFF;
  uint32_t low    = bssid[0]|bssid[1]<<8|bssid[2]<<16|bssid[3]<<24;
  //Serial.println(low);
  //Serial.println(high);
  //doc["chip_id"]=low+high;
  return low+high;
}
char httpreq[1500];
bool reupload_qc(){
    HTTPClient http;    
    char reupload[300]="http://103.150.191.136/owl_inventory/produksi/api_inventaris.php?no_sn=";
    char sn_char[15]="";
    http.setTimeout(15000);
    http.setConnectTimeout(15000);
    sprintf(sn_char,"%u",nomor_serial);
    strcat(reupload,sn_char);
    http.begin(reupload);
    int httpCode = http.GET();
    delay(2000);
    Serial.println(reupload);
    Serial.println(httpCode);
    Serial.println(http.getString());
    if(httpCode==200){
      
      return true;
    }
    else
      return false;
     
}
bool upload_qc(JsonDocument &doc){//TODO: KONFIRM
  while (WiFi.status()!=WL_CONNECTED)
  {
    delay(1000);
    Serial.println("connecting wifi....");
  }

  doc["mac_bluetooth"]=mac_wifi_to_bt(doc["mac"].as<const char*>());
  doc["chip_id"]=mac_to_chip_id(doc["mac"].as<const char *>());
  //serializeJson(doc,Serial);
  if(WiFi.status()==WL_CONNECTED){
    HTTPClient http;    
    http.begin("http://103.150.191.136/owl_inventory/produksi/api_inventaris.php");
    http.setTimeout(15000);
    http.setConnectTimeout(15000);
    http.addHeader(F("Content-Type"), F("application/x-www-form-urlencoded"));
    IPAddress ipa = WiFi.localIP();
    uint8_t IP_array[4]= {ipa[0],ipa[1],ipa[2],ipa[3]};
    sprintf(httpreq,"chip_id=%u&produk=%s&firmware_version=%s&hardware_version=%s&bat=%s&temperature=%s&ip_address=%d.%d.%d.%d&",doc["chip_id"].as<uint32_t>(),doc["name"].as<const char*>(),doc["firmware"].as<const char*>(),doc["hardware"].as<const char*>(),doc["bat"].as<const char*>(),doc["temp"].as<const char*>(),ipa[0],ipa[1],ipa[2],ipa[3]);
    strcat(httpreq,"type_produk=FPOWL&nama_client=OWL");
    strcat(httpreq,"&mac_wifi=");
    strcat(httpreq,doc["mac"].as<const char*>());
    strcat(httpreq,"&mac_bluetooth=");
    strcat(httpreq,doc["mac_bluetooth"].as<const char*>());
    strcat(httpreq,"&free_ram=");
    strcat(httpreq,doc["free_ram"].as<const char*>()); 
    strcat(httpreq,"&min_ram=");
    strcat(httpreq,doc["min_ram"].as<const char*>());  
    strcat(httpreq,"&batt_low=");
    strcat(httpreq,"1500");            
    strcat(httpreq,"&batt_high=");
    strcat(httpreq,"2400");   
    strcat(httpreq,"&temperature=");
    strcat(httpreq,doc["temp"].as<const char*>());
    strcat(httpreq,"&status_error=");
    strcat(httpreq,doc["status_error"].as<const char*>());            
    strcat(httpreq,"&gps_latitude=");
    strcat(httpreq,doc["latitude"].as<const char*>());                
    strcat(httpreq,"&gps_longitude=");
    strcat(httpreq,doc["longitude"].as<const char*>());  
    //Serial.println(httpreq);              
    strcat(httpreq,"&status_qc_sensor_1=");
    strcat(httpreq,doc["status_qc_sensor_1"].as<const char*>());
    strcat(httpreq,"&status_qc_sensor_2=");
    strcat(httpreq,doc["status_qc_sensor_2"].as<const char*>());                    
    strcat(httpreq,"&status_qc_sensor_3=");
    strcat(httpreq,doc["status_qc_sensor_3"].as<const char*>());
    strcat(httpreq,"&status_qc_sensor_4=");
    strcat(httpreq,doc["status_qc_sensor_4"].as<const char*>());
    strcat(httpreq,"&status_qc_sensor_5=");
    strcat(httpreq,doc["status_qc_sensor_5"].as<const char*>());
    strcat(httpreq,"&status_qc_sensor_6=ok");
    Serial.println(httpreq);
    int httpCode = http.POST(httpreq);
    if(httpCode > 0) {
      if(httpCode == HTTP_CODE_OK) {
        //Serial.println(http.getString());
        deserializeJson(newsn,http.getString());
        http.end();
        return true;
      }
    }
    else {
      Serial.println("gagal upload");
      http.end();
      return false;
    } 
    Serial.printf("http code %d",httpCode);
    Serial.println(http.getString());          
  }
};


void clear_bluetooth(){
  while (SerialBT.available()>0)
  {
    delay(100);
    byte b=SerialBT.read();
  }

}

void clear_serial(){
  while (Serial.available()>0)
  {
    delay(100);
    byte b=Serial.read();
  }

}
bool send_command(const char* command, const char* comment, JsonDocument &doc, bool isRemove=false){
  SerialBT.print(command);
  SerialBT.write(10);
  JsonDocument remove;
  Serial.println(comment);
  delay(500);
  if(isRemove){
    deserializeJson(remove,SerialBT);
    //Serial.println("Removed json");
    //Serial.println(SerialBT.available());
    //serializeJson(remove,Serial);
    SerialBT.read();
    SerialBT.read();
  }
  delay(2000);
  while (SerialBT.available()==0)
  {
    Serial.println(comment);
    delay(1000);
  }
JsonDocument newDoc;
DeserializationError err = deserializeJson(newDoc,SerialBT);
if (err) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(err.c_str());
    clear_bluetooth();
    return false;
}
clear_bluetooth();
merge(doc,newDoc);
return true;
}

bool send_command_battery(const char* command, const char* comment, JsonDocument &doc, const char* firstrow, const char* secondrow){
  SerialBT.print(command);
  SerialBT.write(10);
  char row [200];
  while (SerialBT.available()==0)
  {
    Serial.println(comment);
    delay(1000);
  }
int i=SerialBT.readBytesUntil('\n',row,sizeof(row));
row[i-1]=0;
doc[firstrow]=row;
i=SerialBT.readBytesUntil('\n',row,sizeof(row));
row[i-1]=0;
doc[secondrow]=row;
//serializeJson(doc,Serial);
clear_bluetooth();
return true;
}

bool send_command_inject(const char* command, const char* comment){
  SerialBT.print(command);
  SerialBT.write(10);
  char row [200];
  return true;
}

JsonDocument alldoc;
void setup() {
  bool connected;
  
  Serial.begin(115200); //START GPS && OUTPUT DEBUG
  
  configTime(gmtOffset_sec, 0, ntpServer1, ntpServer2);
  WiFi.begin("Devices","87654321");
  while (WiFi.status()!=WL_CONNECTED)
  {
    delay(5000);
    Serial.println("connecting wifi....");
  }
  
  if(esp_sleep_get_wakeup_cause()==ESP_SLEEP_WAKEUP_TIMER){
        Serial.println("Sedang Aktivasi .........");
        if(reupload_qc())
      Serial.println("QC SUKSES...");
    else 
      Serial.println("gagal aktivasi..");
      esp_deep_sleep_start();
  }

  SerialBT.begin("QC_FP", true);
  Serial.println("connecting...");
  connected = SerialBT.connect("OWL_FINGER");
  if(connected) {
    Serial.println("Connected Successfully!");
  } 
  else {
    while(!SerialBT.connected(10000)) {
      Serial.println("Failed to connect. Make sure remote device is available and in range, then restart app.");
    }
  }

  //reset initial
  send_command("5\n12345\n?\n1234","reset device sebelum qc...",alldoc,true);
  //-----------------

  //cek info device
  while(!send_command("y","cek info device...",alldoc)); 
  
  Serial.println("INI CHIP ID DEVICE YANG SEDANG DI QC");
  Serial.println("---------------------------------------------------------------");
  Serial.println(mac_to_chip_id(alldoc["mac"]));
  Serial.println("---------------------------------------------------------------");

  Serial.println("INI SN NYA...");
  Serial.println("---------------------------------------------------------------");
  Serial.println(alldoc["sn"].as<const char*>());
  Serial.println("---------------------------------------------------------------");
  //---------------
  //alldoc["latitude"]="0.0";
  //alldoc["longitude"]="0.0";
  //cek info gps loop terus sampai berhasil
  while(!send_command("c\ngps","cek gps...",alldoc)||(alldoc["latitude"].as<float>()==0)||(alldoc["longitude"].as<float>()==0)) //cek info gps loop sampai sukses
    delay(2000);
  //---------------

  //registrasi loop terus sampai berhasil
  char hasil_perintah [100] = "gagal";
  while(!send_command("r\n12345\ntest qc\n?\n1234","mohon registrasi...",alldoc,true)||(!strcmp(hasil_perintah,"registrasi berhasil")==0)){
    delay(2000);
    strlcpy(hasil_perintah,alldoc["perintah"]|"gagal",sizeof(hasil_perintah));
    if(strcmp(hasil_perintah,"registrasi berhasil")==0)
      break;
  }
  alldoc["status_qc_sensor_1"]=alldoc["perintah"];
  //----------------
  
  //backup sidik jari
  while(!send_command("6\n12345\n?\n1234","download template...",alldoc,true)||(!strcmp(hasil_perintah,"12345")==0)){
    delay(2000);
    strlcpy(hasil_perintah,alldoc["nik"]|"gagal",sizeof(hasil_perintah));
    if(strcmp(hasil_perintah,"12345")==0)
      break;
  }
  alldoc["status_qc_sensor_2"]="backup berhasil";
  //serializeJson(alldoc,Serial);
  //----------------

  //delete sidik jari
  while(!send_command("5\n12345\n?\n1234","delete sidik jari...",alldoc,true)||(!strcmp(hasil_perintah,"delete datakar berhasil")==0)){
    strlcpy(hasil_perintah,alldoc["perintah"]|"gagal",sizeof(hasil_perintah));
    if(strcmp(hasil_perintah,"delete datakar berhasil")==0)
      break;
  }
  alldoc["status_qc_sensor_3"]=alldoc["perintah"];
  //serializeJson(alldoc,Serial);
  //----------------


  //restore sidik jari....
  char restore_command [2048]="8\n";
  strcat(restore_command,alldoc["nik"].as<const char*>());
  strcat(restore_command,"\n");
  strcat(restore_command,alldoc["nama"].as<const char*>());
  strcat(restore_command,"\n");
  strcat(restore_command,alldoc["template"].as<const char*>());
  strcat(restore_command,"\n");    
  while(!send_command(restore_command,"restore template...",alldoc)||(!strcmp(hasil_perintah,"oke")==0)){
    delay(2000);
    strlcpy(hasil_perintah,alldoc["result"]|"gagal",sizeof(hasil_perintah));
    if(strcmp(hasil_perintah,"oke")==0)
      break;
  }
  alldoc["status_qc_sensor_4"]="restore berhasil";
  //serializeJson(alldoc,Serial);
  //----------------------------------


  //abensi
  SerialBT.print("m\n");
  while (Serial.available()==0){
  Serial.println("Mohon absen...");
  delay(2000);
  if(SerialBT.available()>0){
    clear_bluetooth();
    break;
  }
  }
  alldoc["status_qc_sensor_5"]="absensi berhasil";
  alldoc.remove("template");
  alldoc.shrinkToFit();

  Serial.println("mohon charge sampai penuh");
  Serial.println("ketik ok di serial monitor jika sudah");
  while(Serial.available()==0);
  clear_serial();
  //send_command_battery("C\nokem","kalibrasi baterai...",alldoc,"batlow","bathigh");

  Serial.println("Upload hasil QC ke server...");
  while(!upload_qc(alldoc)){
    delay(10000);
  }
  Serial.println("Injecting SN...");
  SerialBT.write('c');
  SerialBT.write(10);
  SerialBT.print("sn_inject");
  SerialBT.write(10);
  SerialBT.print(newsn["no_sn"].as<uint32_t>());
  SerialBT.write(10);
  delay(2000);
  
  Serial.write('c');
  Serial.write(10);
  Serial.print("sn_inject");
  Serial.write(10);
  Serial.print(newsn["no_sn"].as<uint32_t>());
  Serial.write(10);

  /*SerialBT.write('c');
  SerialBT.write(10);
  SerialBT.print("sn_inject");
  SerialBT.write(10);
  SerialBT.print(newsn["no_sn"].as<uint32_t>());
  SerialBT.write(10);
  */
  Serial.println("setting time");
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("No time available (yet)");
    //return;
  }
  else{
    SerialBT.print("t\n");
    SerialBT.print(&timeinfo, "%Y-%m-%d %H:%M:%S");
    SerialBT.print("\n?\n");
    SerialBT.print("1234\n");
  }
  Serial.println("mohon tunggu 20 detik...");
  delay(10000);
  Serial.println("mohon tunggu 10 detik...");
  SerialBT.print("n\n?\n1234");
  delay(10000);
  connected = SerialBT.connect("OWL_FINGER");
  if(connected) {
    Serial.println("Connected Successfully!");
  } 
  else {
    while(!SerialBT.connected(10000)) {
      SerialBT.connect("OWL_FINGER");
      Serial.println("Failed to connect. Make sure remote device is available and in range, then restart app.");
    }
  }
  clear_bluetooth();
  JsonDocument docReqc;
  //send_command("y","cek info device...",docReqc); 
  send_command("y","cek info device...",docReqc); 
  serializeJson(docReqc,Serial);
  WiFi.disconnect();
  delay(2000);
  WiFi.begin("Devices","87654321");
  while (WiFi.status()!=WL_CONNECTED)
  {
    Serial.println("connecting wifi....");
    delay(5000);
  }
  if(atoi(docReqc["sn"].as<const char*>())==newsn["no_sn"].as<uint32_t>() && atoi(docReqc["status_error"].as<const char*>())==0){
    nomor_serial=newsn["no_sn"].as<uint32_t>();
    esp_sleep_enable_timer_wakeup(9000000);
    esp_deep_sleep_start();
  }
  //24082008
  else
    Serial.println("MASIH ADA STATUS ERROR GAGAL....");
  vTaskDelete(NULL);
}

void loop(){


}