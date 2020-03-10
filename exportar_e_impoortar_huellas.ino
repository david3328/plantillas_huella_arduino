#include <Adafruit_Fingerprint.h>
#include <SoftwareSerial.h>
#include <Ethernet.h>

int getFingerprintIDez();

// pin #2 is IN from sensor (GREEN wire)
// pin #3 is OUT from arduino  (WHITE wire)
SoftwareSerial mySerial(2, 3);

Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

const byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED}; 
const byte ip[] = {192, 168, 70, 116};                       // Dirección IP estática (Shield Ethernet)
const byte server[] = {192, 168, 20, 6};                   // Dirección IP Servidor (API)
//const char url_conexion[] PROGMEM="";
//const String url_conexion[] PROGMEM="/api/cimapersonal/asistencia";
EthernetClient client;

void setup()  {
  while(!Serial);
  Serial.begin(9600);

  // set the data rate for the sensor serial port
  finger.begin(57600);
  
  if (finger.verifyPassword()) {
    Serial.println(F("Found fingerprint sensor!"));
  } else {
    Serial.println(F("Did not find fingerprint sensor :("));
    while (1);
  }

  //Inicializar Ethernet con la MAC e IP definidas
  Ethernet.begin(mac, ip);

  Serial.println(F("Esperando conexión con el servidor"));

  while(true){
    if (client.connect(server,80)){
      client.flush();
      client.stop();
      break;
    }else{
      Serial.print(F("."));    
    }
  }
  Serial.println();

//exportar_huella(404);

  importar_huella(404);
  //for(uint16_t i=403; i<405; i++){ //Exportamos o importamos 1000 huellas.
  // exportar_huella(i);
//}
}

uint8_t exportar_huella(uint16_t id){
  // one data packet is 139 bytes. in one data packet, 11 bytes are 'usesless' :D
  uint8_t data_huella[556]; // 4 data packets
  memset(data_huella, 0xff, 556);
  uint32_t starttime;
  int i = 0;
  Serial.println(F("------------------------------------"));
  Serial.print(F("Attempting to load #")); Serial.println(id);
  uint8_t p = finger.loadModel(id);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.print(F("Template ")); Serial.print(id); Serial.println(F(" loaded"));
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println(F("Communication error"));
      return p;
    default:
      Serial.print(F("Unknown error ")); Serial.println(p);
      return p;
  }

  // OK success!

  Serial.print(F("Attempting to get #")); Serial.println(id);
  p = finger.getModel();
  switch (p) {
    case FINGERPRINT_OK:
      break;
   default:
      Serial.print(F("Unknown error ")); Serial.println(p);
      return p;
  }
  
  starttime = millis();
  while (i < 556 && (millis() - starttime) < 1000) {//20000
      if (mySerial.available()) {
        data_huella[i++] = mySerial.read();
      }
  }

  Serial.print(F("Template ")); Serial.print(id); Serial.println(F(" transferring:"));
  Serial.print(i); Serial.println(F(" bytes read."));
  Serial.println(F("Decoding packet..."));
  for(int i=0;i<556;i++){
    printHex(data_huella[i], 2);
  }
  Serial.println("\ndone.");
  
  unsigned int json_length = 576;
  json_length += String(id).length();
  for(int i=0; i < 556; i++){ //512
    json_length += Retornar_Byte(data_huella[i]).length();
  }

  unsigned long timeout = 0;
  unsigned long timeout_last = 0;

  p=2;
  timeout_last=millis();
  while(true){
    timeout= millis();
    if(timeout-timeout_last>=10) break;
    if (client.connect(server,80)){
      // Write response headers
      client.println(F("POST /ProyectoCima/VistaApp/huella.php HTTP/1.1"));
      //client.println(F("POST /api/cimapersonal/asistencia/exportar_huellas.php HTTP/1.1"));
      client.println(F("Host: 192.168.20.6"));       
      client.println(F("Connection: close"));
      client.println(F("Content-Type: application/json"));
      client.println(F("User-Agent: Arduino/1.0"));
      client.print(F("Content-Length: "));
      client.println(json_length);
      client.println();
  
      client.print(F("{\"id\":"));
      client.print(String(id));
      client.print(F(",\"template\":["));
      for(int i=0; i < 556; i++){
        client.print(Retornar_Byte(data_huella[i]));
        if(i!=555) {client.print(F(","));}
      }
      client.print(F("]}"));

      Serial.println(F("\nData de la Huella Enviada.\n"));

      unsigned long timeout2 = 0;
      unsigned long timeout_last2 = 0;
    
      timeout_last2=millis();
      //Read and parse all the lines of the reply from server
      while(true){
        timeout2= millis();
        if(timeout2-timeout_last2>=2500) break;
        if(client.available()){
          String line = client.readStringUntil('\n');
          line.replace("\r","");
          Serial.println(line);
          if(line=="{\"aviso\":\"OK\"}"){
            Serial.println(F("DATA DE LA HUELLA ALMACENADA"));
            p=0;
            break;
          }else{
            if(line=="{\"aviso\":\"OK_2\"}"){
              Serial.println(F("NO SE PUDO ALMACENAR HUELLA"));
              p=0;
              break;
            }else{
              if(line=="{\"aviso\":\"ERROR\"}"){
                Serial.println(F("ERROR DE TRANSMISION"));
                p=1;
                break;
              }else{
                p=2;      
              }
            }
          }
          timeout_last2 = timeout2;
        }
      }
      client.flush();
      client.stop();
      timeout_last = timeout;
    }else{
      Serial.println(F("\nNo se pudo enviar Data de la Huella.\n"));
      Ethernet.begin(mac, ip);
    }
    if(p==0) break;
  }
  
  return p;
}

void printHex(int num, int precision) {
    char tmp[16];
    char format[128];
 
    sprintf(format, "%%.%dX", precision);
 
    sprintf(tmp, format, num);
    Serial.print(tmp);
}

String Retornar_Byte(int num) {
    unsigned char tmp[8];
    sprintf(tmp, "%d", num);
    return tmp;
}

uint8_t importar_huella(uint16_t id){
  String json = "{\"id\":"+String(id)+"}";
  unsigned long timeout = 0;
  unsigned long timeout_last = 0;

  uint8_t data_huella[556]; // the real template
  memset(data_huella, 0xff, 556);
  uint16_t contador_bytes = 0;

  uint8_t p=2;
  timeout_last=millis();
  while(true){
    timeout= millis();
    if(timeout-timeout_last>=10000) break;
    if (client.connect(server,80)){
      // Write response headers
      client.println(F("POST /ProyectoCima/VistaApp/huellalistar.php HTTP/1.1"));
      //client.println(F("POST /api/cimapersonal/asistencia/importar_huellas.php HTTP/1.1"));
      client.println(F("Host: 192.168.20.6"));       
      client.println(F("Connection: close"));
      client.println(F("Content-Type: application/json"));
      client.println(F("User-Agent: Arduino/1.0"));
      client.print(F("Content-Length: "));
      client.println(json.length());
      client.println();
  
      client.print(json);

      Serial.println("\n");
      Serial.print(json);
      Serial.println(F("\nSolicitud de la Data de la Huella Enviada.\n"));

      unsigned long timeout2 = 0;
      unsigned long timeout_last2 = 0;
      boolean estado_mensaje = false;
      String valor_byte="";
      
      contador_bytes = 0;
      timeout_last2=millis();
      //Read and parse all the lines of the reply from server

       while (client.available())
        {
          char c = client.read();
          if(c==','){
            data_huella[contador_bytes] = response.toInt();
            response = "";
            contador_bytes++;
          }else if(c!='[' && c!=']'){
            response = response + c;
          }
          
          // Serial.print(c);
        }


      // Serial.print(response)

      // while(true){
      //   timeout2= millis();
      //   if(timeout2-timeout_last2>=2500) break;
      //   if(client.available()){
      //     char c=client.read();
      //     Serial.print(c);
          
      //     if(estado_mensaje){
      //       if(c==']')
      //       {
      //         p=0;
      //         break;  
      //       }
      //       if(c==','){
      //         data_huella[contador_bytes] = valor_byte.toInt();
      //         valor_byte = "";
      //         contador_bytes++;
      //       }else{
      //         valor_byte = valor_byte + c;  
      //       }
      //     }else{
      //       if(c=='[')
      //       {
      //         estado_mensaje=true;
      //       }
      //     }
          
      //     timeout_last2 = timeout2;
      //   }
      // }
      client.flush();
      client.stop();
      timeout_last = timeout;
    }else{
      Serial.println(F("\nNo se pudo enviar la Solicitud de la Data de la Huella.\n"));
      Ethernet.begin(mac, ip);
    }
    if(p==0) break;
  }

  Serial.println();

  if(contador_bytes==556){
    p = finger.setModel();
    switch (p) {
      case FINGERPRINT_OK:
        Serial.print(F("Template ")); Serial.print(id); Serial.println(F(" transferring:"));
        break;
      default:
        Serial.print(F("Unknown error ")); Serial.println(p);
        return p;
    }
    for(uint16_t i=0;i<556;i++){
      finger.sendTemplate(data_huella[i]);
    }
    mySerial.flush();
    Serial.print("ID "); Serial.println(id);
    p = finger.storeModel(id);
    if (p == FINGERPRINT_OK) {
      Serial.println("Stored!");
    } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
      Serial.println("Communication error");
      return p;
    } else if (p == FINGERPRINT_BADLOCATION) {
      Serial.println("Could not store in that location");
      return p;
    } else if (p == FINGERPRINT_FLASHERR) {
      Serial.println("Error writing to flash");
      return p;
    } else {
      Serial.println("Unknown error");
      return p;
    }
  }else{
    Serial.println(F("\nNo se pudo importar la Huella.\n"));
  }
  
  return p;
}

void loop()
{

}
