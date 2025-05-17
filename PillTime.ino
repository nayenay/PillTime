// librerias
#include <WiFi.h>              //Permite conectar el ESP32(harware) a una red WiFi
#include <FirebaseESP32.h>     //Librería para usar Firebase con ESP32
#include <NTPClient.h>         // Para obtener hora exacta
#include <WiFiUdp.h>           // Comunicación UDP (protocolo) para NTP(Network Time Protocol)



//Dirección del proyecto en Firebase
#define FIREBASE_HOST "https://logincorreo-9d4c9-default-rtdb.firebaseio.com"
//Clave secreta de autenticación
#define FIREBASE_AUTH "yJRwvdfruaBitv4ek2A5iOkWnXrXuznjDRpFocy1"
//Nombre de la red Wi-Fi
#define WIFI_SSID "Mega-5g-2FAD"  //si van a hacer pruebas, la cambian a su WiFi
//Contreseña del WiFi
#define WIFI_PASSWORD "nXBSaB2QfT"

// Define NTP Client(ESP32) to get time
WiFiUDP ntpUDP; //WiFiUDP librería de Arduino que permite enviar y resivir datos, ntpUDP objeto para la comunicación con el servidor NTP
NTPClient timeClient(ntpUDP); //clase para obtener hora desde un servidor NTP

// Variables para guardar fecha y hora
String formattedDate;  //variable principal, guarda en el formato "2025-05-17T15:26:03Z" Z es de hora universal
String dayStamp;       //solo fecha "2025-05-17"
String timeStamp;      //solo hora   "15:26:03Z"



//Onjetos en Firebase
FirebaseData firebaseData;    //para mejorar la comunicación con Firebase, FirebaseData es una librería, el objeto firebaseData envía datos, recive respuestas, informa si hubo errores, guarda resultados de una operación   
FirebaseJson json;            //para etructurar los datos a enviar en formato JSON

//Ruta y datos a enviar
String path = "/esp32";       //Ruta base en Firebase        
String user = "jazmin";       //Usuario (parte de la ruta)
String sensor = "prueba";    //En este ejemplo es el nombre del sensor (renombrar)
int valuePrueba = 12;        //Valor a envia (cambiar a bool, solo presionará un botón verde o rojo)






//Configuración inicial   (configurar piens, iniciar conexión WiFi, iniciar sensores, comenzar cominicación con FIrebase, actuadores...)
void setup(){
  Serial.begin(115200);                        //comunicación del ESP32(hardware) con la computadora, igual no lo veo necesario por el momento
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);        //Inicia conexión WiFi con dos variables, nombre de la red y contraseña
  Serial.print("Connecting to Wi-Fi");          //comentario 
  while (WiFi.status() != WL_CONNECTED)  {      //Bucle hasta que el ESP32(hardware) esté conectado al WiFI
    Serial.print(".");                          //imprime un punto para saber que está esperando
    delay(300);                                  //espera 300 milisegundos para hacer otra iteración
  }

  Serial.println();                              //salto de linea
  Serial.print("Connected with IP: ");          //imprime "Connected with IP: "
  Serial.println(WiFi.localIP());                // imprime dirección IP que recibió el ESP32(hardware) del router WiFi
  Serial.println();

  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);  //inicia conexión con Firebase
  Firebase.reconnectWiFi(true);                  //reconecta si se pierde la conexion
 //Size and its write timeout e.g. tiny (1s), small (10s), medium (30s) and large (60s).
  Firebase.setwriteSizeLimit(firebaseData, "tiny");// establece un límite de tamaño para los datos escritos ***
}





//Bucle principal(loop)
void loop(){
    while(!timeClient.update()) {                  //mientras no se haya podido obtener la hora correctamente
      timeClient.forceUpdate();                    //actualiza hora
    }
// modificar de acuerdo a nuestras necesidades, y deabtir como organizar la información para la pantalla progreso 
  formattedDate = timeClient.getFormattedDate();    //Ej: "2025-05-15T10:35:47Z", timeClient obtiene la fehca y hora actual, la cuerda en formattedDate, T es de time hora
    json.clear().add("Value", valuePrueba);        //Agrega el valor a JSON, primiero limpia el json, agrega una nueva clave -valor  "value"-"ValuePrueba",  "Value": 12,
    json.add("Date", formattedDate);                // Agrega la fecha y hora al JSON, agrega "Date":"formattedDate" , "Date": "2025-05-15T10:35:47Z"



  
// modificar de acuerdo a nuestras necesidades, y deabtir como organizar la información para la pantalla progreso 
  // Envía los datos al path(ruta de firebase) /esp32/jazmin/prueba en Firebase
    if (Firebase.pushJSON(firebaseData, path + "/" + user + "/" + sensor, json)) { //Agrega un nuevo nodo hijo con un ID único en la ruta dada (/esp32/jazmin/prueba)
      Serial.println("PASSED");                              //"PASSED"
      Serial.println("PATH: " + firebaseData.dataPath());     //"PATH: /esp32/jazmin/prueba/-Nr12345abcXYZ"
      Serial.print("PUSH NAME: ");                            //
      Serial.println(firebaseData.pushName());                //"PUSH NAME: -Nr12345abcXYZ"
      Serial.println("ETag: " + firebaseData.ETag());          //'ETag: "wxyz4567"'              Muestra el identificador de versión del dato (para saber si fue modificado más tarde)
      Serial.println("------------------------------------");
      Serial.println();
    }

    else
    {
      Serial.println("FAILED");
      Serial.println("REASON: " + firebaseData.errorReason());  //imprime error
      Serial.println("------------------------------------");
      Serial.println();
    }

 

    delay(1000);                                                //espera un segundo antes de enviar de nuevo los datos 

}
