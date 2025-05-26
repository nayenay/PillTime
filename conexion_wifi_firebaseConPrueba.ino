//Sí funciona, se conecta a WiFi y a Firebase, también escribe en mi cuenta 
#include <WiFi.h>
#include <FirebaseESP32.h>

// Datos de la red WiFi
#define WIFI_SSID "Mega-2.4G-2FAD"
#define WIFI_PASSWORD "nXBSaB2QfT"

// Datos de Firebase
#define FIREBASE_HOST "logincorreo-9d4c9-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "yJRwvdfruaBitv4ek2A5iOkWnXrXuznjDRpFocy1"

// Objetos globales
FirebaseData firebaseData;
FirebaseJson json;

FirebaseAuth auth;
FirebaseConfig config;

void setup() {
  Serial.begin(115200);

  // Conectar a Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Conectando a Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.println("Conectado a Wi-Fi");
  Serial.println(WiFi.localIP());

  // Configurar Firebase
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  Firebase.setwriteSizeLimit(firebaseData, "tiny");

  if (Firebase.ready()) {
    Serial.println("Firebase listo.");
  } else {
    Serial.println("Firebase NO está listo.");
  }

  // Escribir en Firebase
  String ruta = "DataBase/WKiH1ESIcCgYgIUjyBKcfpgdAnu1/Medicamentos/Chocofresa/prueba1";

  if (Firebase.setString(firebaseData, ruta.c_str(), "ESP32")) {
    Serial.println("Dato escrito correctamente en Firebase.");
  } else {
    Serial.print("Fallo al escribir en Firebase: ");
    Serial.println(firebaseData.errorReason());
  }
}


void loop() {
  // Código principal aquí
}
