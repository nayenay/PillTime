// librerias
#include <WiFi.h>              //Permite conectar el ESP32(harware) a una red WiFi
#include <FirebaseESP32.h>     //Librería para usar Firebase con ESP32
#include <NTPClient.h>         // Para obtener hora exacta
#include <WiFiUdp.h>           // Comunicación UDP (protocolo) para NTP(Network Time Protocol)

//Dirección del proyecto en Firebase
#define FIREBASE_HOST "YOUR_FIRABSE_HOST"
//Clave secreta de autenticación
#define FIREBASE_AUTH "YOUR_FIREBASE_AUTH"
//Nombre de la red Wi-Fi
#define WIFI_SSID "YOUR_WIFI_SID"
//Contreseña del WiFi
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// Define NTP Client(ESP32) to get time
WiFiUDP ntpUDP; //WiFiUDP librería de Arduino que permite enviar y resivir datos, ntpUDP objeto para la comunicación con el servidor NTP
NTPClient timeClient(ntpUDP); //clase para obtener hora desde un servidor NTP

// Variables para guardar fecha y hora
String formattedDate;  //variable principal, guarda en el formato "2025-05-17T15:26:03Z" Z es de hora universal
String dayStamp;        
String timeStamp;   


//Onjetos en Firebase
FirebaseData firebaseData;    //para mejorar la comunicación con Firebase
FirebaseJson json;            //para etructurar los datos a enviar en formato JSON

//Ruta y datos a enviar
String path = "/esp32";        
String user = "jazmin";
String sensor = "prueba";
int valuePrueba = 12;

//Configuración inicial
void setup(){
  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED)  {
    Serial.print(".");
    delay(300);
  }

  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);
 //Size and its write timeout e.g. tiny (1s), small (10s), medium (30s) and large (60s).
  Firebase.setwriteSizeLimit(firebaseData, "tiny");
}



//Bucle principal(loop)
void loop()

{

    while(!timeClient.update()) {

      timeClient.forceUpdate();

    }

    formattedDate = timeClient.getFormattedDate();

    json.clear().add("Value", valuePrueba);

    json.add("Date", formattedDate);

    if (Firebase.pushJSON(firebaseData, path + "/" + user + "/" + sensor, json))

    {

      Serial.println("PASSED");

      Serial.println("PATH: " + firebaseData.dataPath());

      Serial.print("PUSH NAME: ");

      Serial.println(firebaseData.pushName());

      Serial.println("ETag: " + firebaseData.ETag());

      Serial.println("------------------------------------");

      Serial.println();

    }

    else

    {

      Serial.println("FAILED");

      Serial.println("REASON: " + firebaseData.errorReason());

      Serial.println("------------------------------------");

      Serial.println();

    }

 

    delay(1000);

}
