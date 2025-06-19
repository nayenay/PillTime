#include <WiFi.h>
#include <FirebaseESP32.h>
#include <time.h>      // Para la función de tiempo NTP
#include <vector>      // Para usar std::vector para almacenar múltiples dosis
#include <algorithm>   // Para std::sort

// Datos de la red WiFi
#define WIFI_SSID "Mega-2.4G-2FAD"
#define WIFI_PASSWORD "nXBSaB2QfT"

// Datos de Firebase
#define FIREBASE_HOST "logincorreo-9d4c9-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "yJRwvdfruaBitv4ek2A5iOkWnXrXuznjDRpFocy1"

// Pines de los componentes
#define BUZZER_PIN 25        // GPIO para el Buzzer
#define MOTOR_VIBRADOR_PIN 26 // GPIO para el Motor Vibrador
#define BUTTON_GREEN_PIN 34  // GPIO para el botón verde
#define BUTTON_RED_PIN 35    // GPIO para el botón rojo

// Array de pines para los LEDs (ajusta según tu circuito)
// Los pines están definidos en el diagrama de izquierda a derecha.
int ledPins[] = {16, 17, 5, 18, 19, 21, 22, 23};
const int NUM_LEDS = sizeof(ledPins) / sizeof(ledPins[0]);

// Objetos globales de Firebase
FirebaseData firebaseData;
FirebaseAuth auth;
FirebaseConfig config;

// Variables para la lógica de la alarma
bool alarmaActiva = false;
bool alarmaSilenciadaPorBotonVerde = false;
unsigned long tiempoInicioAlarma = 0;
unsigned long tiempoSilencioLedVerde = 0;
int compartimientoActivo = -1; // -1 significa ningún compartimiento activo

// --- Variables para la hora NTP ---
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -21600; // Offset para CST (GMT-6 horas) - Zacatecas
const int   daylightOffset_sec = 0; // No hay horario de verano en Zacatecas actualmente

// Estructura para almacenar los datos de una dosis de medicamento
struct MedicamentoDosis {
  String nombreMedicamento; // Nombre del medicamento (ej. Ibuprofeno)
  String dosisTimestampKey; // La clave del nodo de la dosis (ej. "2025-06-06T13:23:00Z")
  String scheduledTimestamp; // El valor de 'scheduled' (ej. "2025-06-06T13:23:00Z")
  time_t scheduledUnixTime; // El tiempo programado en formato UNIX para fácil comparación
  int compartimiento;       // Compartimiento asociado a este medicamento/dosis
  bool tomado;              // true si 'status_code' es 0, false si es 1 (o según tu lógica)

  // Constructor para facilitar la inicialización
  MedicamentoDosis(String name, String key, String scheduled, time_t unixTime, int comp, bool taken)
    : nombreMedicamento(name), dosisTimestampKey(key), scheduledTimestamp(scheduled), 
      scheduledUnixTime(unixTime), compartimiento(comp), tomado(taken) {}
};

// Vector para almacenar todas las dosis pendientes que encontramos
std::vector<MedicamentoDosis> dosisPendientes;

// La dosis que está activa o a punto de activarse
// Usamos un puntero para poder indicar si no hay dosis para monitorear (nullptr)
MedicamentoDosis* dosisActualAlarma = nullptr; 

// Función para parsear la fecha y hora completa de un string ISO 8601 a una estructura tm
bool parseIsoTimestampToTm(const String& isoString, struct tm& targetTm) {
  if (isoString.length() < 19) { // Mínimo "YYYY-MM-DDTHH:MM:SS"
    return false;
  }
  char timeStr[20]; // YYYY-MM-DDTHH:MM:SS + null terminator
  isoString.substring(0, 19).toCharArray(timeStr, sizeof(timeStr));
  char* ret = strptime(timeStr, "%Y-%m-%dT%H:%M:%S", &targetTm); // %Y-%m-%dT%H:%M:%S para el formato
  return (ret != NULL);
}

void setup() {
  Serial.begin(115200);

  // Configurar pines de salida
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(MOTOR_VIBRADOR_PIN, OUTPUT);
  for (int i = 0; i < NUM_LEDS; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW); // Asegurarse de que todos los LEDs estén apagados al inicio
  }

  // Configurar pines de entrada para los botones
  pinMode(BUTTON_GREEN_PIN, INPUT); 
  pinMode(BUTTON_RED_PIN, INPUT);   

  // Apagar buzzer y motor vibrador al inicio
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(MOTOR_VIBRADOR_PIN, LOW);

  // Conectar a Wi-Fi
  Serial.print("Conectando a Wi-Fi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
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
    // Sincronizar la hora NTP
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    printLocalTime(); 

    // Obtener la hora actual para filtrar dosis futuras
    struct tm nowTm;
    if (!getLocalTime(&nowTm)) {
        Serial.println("Error al obtener la hora NTP para filtrar dosis.");
        return; 
    }
    time_t nowUnixTime = mktime(&nowTm);

    // **** LECTURA DINÁMICA DE MEDICAMENTOS Y DOSIS DESDE FIREBASE ****
    String medicamentosBasePath = "DataBase/WKiH1ESIcCgYgIUjyBKcfpgdAnu1/Medicamentos";
    
    Serial.print("Intentando leer medicamentos desde Firebase en la ruta: ");
    Serial.println(medicamentosBasePath);

    // Primera llamada a Firebase: Obtener todos los medicamentos
    if (Firebase.get(firebaseData, medicamentosBasePath)) {
      if (firebaseData.dataType() == "json") { 
        FirebaseJson jsonMedicamentos;
        jsonMedicamentos.setJsonData(firebaseData.jsonString()); 

        size_t medCount = jsonMedicamentos.iteratorBegin(); 

        if (medCount > 0) {
          Serial.println("Medicamentos encontrados:");
          dosisPendientes.clear(); 

          for (size_t i = 0; i < medCount; i++) {
            int current_json_type; 
            String nombreMedicamento, medValueJsonString; 
            jsonMedicamentos.iteratorGet(i, current_json_type, nombreMedicamento, medValueJsonString);
            
            // Verificamos que sea un objeto JSON
            if (current_json_type == FirebaseJson::JSON_OBJECT) { 
                if (medValueJsonString.length() == 0 || medValueJsonString == "null") {
                    Serial.print("  El medicamento "); Serial.print(nombreMedicamento); Serial.println(" está vacío o es nulo. Saltando.");
                    continue;
                }

                Serial.print("  Procesando medicamento: "); Serial.println(nombreMedicamento);

                FirebaseJson singleMedJson;
                singleMedJson.setJsonData(medValueJsonString); 

                // Intentar leer el compartimiento del medicamento
                FirebaseJsonData medDataResult;
                int medicamentoCompartimiento = -1; 
                // Aquí, simplemente comprobamos si 'get' fue exitoso.
                // Si lo fue, asumimos que medDataResult.intValue es válido.
                if (singleMedJson.get(medDataResult, "compartimiento")) { 
                    medicamentoCompartimiento = medDataResult.intValue;
                    Serial.print("    Compartimiento: "); Serial.println(medicamentoCompartimiento);
                } else {
                    Serial.print("    Advertencia: No se encontró 'compartimiento' para "); Serial.print(nombreMedicamento);
                    Serial.println(". Usando compartimiento por defecto 1.");
                    medicamentoCompartimiento = 1; 
                }

                // Ahora, obtener el nodo "Dosis" para este medicamento con una NUEVA PETICIÓN.
                // Esta es la forma más compatible hacia atrás si FirebaseJsonData no tiene dataType/jsonString.
                String currentDosisPath = medicamentosBasePath + "/" + nombreMedicamento + "/Dosis";
                
                Serial.print("    Intentando leer dosis para "); Serial.print(nombreMedicamento); 
                Serial.print(" en: "); Serial.println(currentDosisPath);

                if (Firebase.get(firebaseData, currentDosisPath)) { 
                    if (firebaseData.dataType() == "json") { // Esto sí lo tiene FirebaseData
                        FirebaseJson jsonDosis;
                        jsonDosis.setJsonData(firebaseData.jsonString()); // Esto sí lo tiene FirebaseData

                        size_t dosisCount = jsonDosis.iteratorBegin();

                        if (dosisCount > 0) {
                            Serial.print("    Dosis encontradas para "); Serial.print(nombreMedicamento); Serial.println(":");
                            for (size_t j = 0; j < dosisCount; j++) {
                                int dosisType;
                                String dosisTimestampKey, singleDoseDetailsJsonString;
                                jsonDosis.iteratorGet(j, dosisType, dosisTimestampKey, singleDoseDetailsJsonString); 
                                
                                if (dosisType == FirebaseJson::JSON_OBJECT) {
                                    FirebaseJson singleDoseDetailsJson;
                                    singleDoseDetailsJson.setJsonData(singleDoseDetailsJsonString); 

                                    FirebaseJsonData doseFieldResult;
                                    String scheduledTimeStr;
                                    int statusCode = 0; 
                                    bool isTaken = false;

                                    // Sin .dataType() aquí, solo comprobamos si .get() fue exitoso
                                    if (singleDoseDetailsJson.get(doseFieldResult, "scheduled")) {
                                      scheduledTimeStr = doseFieldResult.stringValue;
                                    }
                                    if (singleDoseDetailsJson.get(doseFieldResult, "status_code")) {
                                      statusCode = doseFieldResult.intValue;
                                      isTaken = (statusCode == 0); // Asumiendo 0 = tomado, 1 = no tomado
                                    }

                                    struct tm scheduledTm;
                                    if (parseIsoTimestampToTm(scheduledTimeStr, scheduledTm)) {
                                      time_t scheduledUnixTime = mktime(&scheduledTm);

                                      long diff = scheduledUnixTime - nowUnixTime; 

                                      if (!isTaken && (diff >= 0 || (abs(diff) < (5 * 60)))) { // Futuro o dentro de los últimos 5 minutos
                                        dosisPendientes.emplace_back(
                                            nombreMedicamento, 
                                            dosisTimestampKey, 
                                            scheduledTimeStr, 
                                            scheduledUnixTime, 
                                            medicamentoCompartimiento, 
                                            isTaken
                                        );
                                        Serial.print("      -> Dosis pendiente añadida: "); 
                                        Serial.print(nombreMedicamento); Serial.print(" @ "); 
                                        Serial.println(scheduledTimeStr);
                                      }
                                    } else {
                                      Serial.print("      Error parseando timestamp para "); Serial.println(dosisTimestampKey);
                                    }
                                }
                            }
                        } else {
                            Serial.print("    No se encontraron dosis para "); Serial.println(nombreMedicamento);
                        }
                        jsonDosis.iteratorEnd(); 
                    } else {
                         Serial.print("    El nodo 'Dosis' para "); Serial.print(nombreMedicamento); 
                         Serial.print(" no es un objeto JSON en Firebase. Tipo: "); Serial.println(firebaseData.dataType());
                    }
                } else {
                    Serial.print("    Fallo al leer el nodo 'Dosis' para "); Serial.print(nombreMedicamento); 
                    Serial.print(": "); Serial.println(firebaseData.errorReason());
                }
            } else {
                Serial.print("  El nodo de medicamento "); Serial.print(nombreMedicamento); 
                Serial.println(" no es un objeto JSON válido. Tipo: "); Serial.println(current_json_type);
            }
          }
        } else {
          Serial.println("No se encontraron medicamentos en la ruta base.");
        }
        jsonMedicamentos.iteratorEnd(); 
      } else {
        Serial.print("Tipo de dato de Firebase NO es JSON para la ruta de medicamentos base: ");
        Serial.println(medicamentosBasePath);
        Serial.print("Tipo obtenido: "); Serial.println(firebaseData.dataType());
      }
    } else {
      Serial.print("Fallo al leer medicamentos de Firebase en la ruta ");
      Serial.print(medicamentosBasePath);
      Serial.print(": ");
      Serial.println(firebaseData.errorReason());
    }

    // --- ENCONTRAR LA PRÓXIMA DOSIS A ACTIVAR ---
    if (!dosisPendientes.empty()) {
      // Ordenar las dosis pendientes por su tiempo programado (ascendente)
      std::sort(dosisPendientes.begin(), dosisPendientes.end(), 
                [](const MedicamentoDosis& a, const MedicamentoDosis& b) {
                    return a.scheduledUnixTime < b.scheduledUnixTime;
                });

      // La primera dosis en la lista ordenada es la más próxima
      dosisActualAlarma = &dosisPendientes[0]; 
      Serial.println("\n*** Próxima dosis a monitorear: ***");
      Serial.print("Medicamento: "); Serial.println(dosisActualAlarma->nombreMedicamento);
      Serial.print("Programado: "); Serial.println(dosisActualAlarma->scheduledTimestamp);
      Serial.print("Compartimiento: "); Serial.println(dosisActualAlarma->compartimiento);
    } else {
      Serial.println("\nNo hay dosis pendientes futuras o recientes para monitorear.");
      dosisActualAlarma = nullptr; 
      Serial.println("No se configurará ninguna alarma activa hasta la próxima carga.");
    }

  } else {
    Serial.println("Firebase NO está listo.");
  }
}


void loop() {
  // Asegurarse de que Firebase esté conectado antes de hacer operaciones
  if (!Firebase.ready()) {
    return;
  }

  // Si no hay dosis para monitorear, no hacemos nada en el loop de alarma
  if (dosisActualAlarma == nullptr) {
    delay(1000); 
    return;
  }

  // Obtener la hora actual
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Error al obtener la hora NTP");
    delay(1000);
    return;
  }
  time_t currentUnixTime = mktime(&timeinfo); // Tiempo actual en segundos UNIX

  // --- Lógica de la alarma ---
  if (!alarmaActiva && !alarmaSilenciadaPorBotonVerde && !dosisActualAlarma->tomado) { 
    
    long diffSeconds = currentUnixTime - dosisActualAlarma->scheduledUnixTime; 

    if ( diffSeconds >= 0 && diffSeconds <= (5 * 60) ) { 
      
      Serial.println("¡Es hora de tomar el medicamento!");
      Serial.print("Dosis programada: "); Serial.println(dosisActualAlarma->scheduledTimestamp);
      Serial.print("Hora actual: "); Serial.println(&timeinfo, "%Y-%m-%d %H:%M:%S");

      activarAlarma(dosisActualAlarma->compartimiento);
      tiempoInicioAlarma = millis(); 
      alarmaActiva = true;
    }
  }

  // --- Manejo de botones cuando la alarma está activa ---
  if (alarmaActiva) {
    // Caso 1: Presiona el botón verde (dentro de los 5 min que suena la alarma)
    if (digitalRead(BUTTON_GREEN_PIN) == HIGH) { 
      if (millis() - tiempoInicioAlarma <= 5 * 60 * 1000) { 
        Serial.println("Botón Verde presionado: Medicamento tomado.");
        desactivarAlarma(); 

        if (dosisActualAlarma != nullptr) {
          dosisActualAlarma->tomado = true; 
          actualizarEstadoDosisFirebase(dosisActualAlarma->nombreMedicamento, dosisActualAlarma->dosisTimestampKey, true); 
          delay(1000); 
          setup(); // Recargar todas las dosis y encontrar la siguiente.
          return; 
        }
        alarmaSilenciadaPorBotonVerde = true; 
        tiempoSilencioLedVerde = millis(); 
      } else {
        Serial.println("Botón Verde presionado pero fuera del tiempo permitido (5 min).");
      }
      delay(200); 
    }

    // Caso 2: Presiona el botón rojo
    if (digitalRead(BUTTON_RED_PIN) == HIGH) { 
      Serial.println("Botón Rojo presionado: Medicamento NO tomado.");
      desactivarAlarma(); 

      if (dosisActualAlarma != nullptr) {
        dosisActualAlarma->tomado = false; 
        actualizarEstadoDosisFirebase(dosisActualAlarma->nombreMedicamento, dosisActualAlarma->dosisTimestampKey, false); 
        delay(1000); 
        setup(); 
        return; 
      }
      
      if (compartimientoActivo != -1 && compartimientoActivo >= 1 && compartimientoActivo <= NUM_LEDS) {
        digitalWrite(ledPins[compartimientoActivo - 1], LOW); 
      }
      alarmaActiva = false; 
      compartimientoActivo = -1; 
      delay(200); 
    }

    // Caso 3: Alarma activa pero no se presiona ningún botón después de 5 minutos
    if (millis() - tiempoInicioAlarma > 5 * 60 * 1000 && !alarmaSilenciadaPorBotonVerde) {
      if (alarmaActiva) { 
        Serial.println("Tiempo de alarma agotado (5 minutos). Medicamento NO tomado.");
        if (dosisActualAlarma != nullptr) {
          dosisActualAlarma->tomado = false; 
          actualizarEstadoDosisFirebase(dosisActualAlarma->nombreMedicamento, dosisActualAlarma->dosisTimestampKey, false); 
          delay(1000); 
          setup(); 
          return; 
        }
        
        desactivarAlarma(); 
        if (compartimientoActivo != -1 && compartimientoActivo >= 1 && compartimientoActivo <= NUM_LEDS) {
            digitalWrite(ledPins[compartimientoActivo - 1], LOW); 
        }
        alarmaActiva = false; 
        compartimientoActivo = -1; 
      }
    }
  }

  // Lógica para apagar el LED después de 30s si se presionó el botón verde
  if (alarmaSilenciadaPorBotonVerde && compartimientoActivo != -1 && millis() - tiempoSilencioLedVerde >= 30 * 1000) {
    Serial.println("Apagando LED después de 30 segundos (Botón Verde).");
    if (compartimientoActivo >= 1 && compartimientoActivo <= NUM_LEDS) {
        digitalWrite(ledPins[compartimientoActivo -1], LOW);
    }
    alarmaSilenciadaPorBotonVerde = false; 
    compartimientoActivo = -1; 
  }

  delay(50);
}

// --- Funciones de utilidad ---

void activarAlarma(int compartimiento) {
  Serial.print("Activando alarma para compartimiento: ");
  Serial.println(compartimiento);

  digitalWrite(BUZZER_PIN, HIGH);        // Encender Buzzer
  digitalWrite(MOTOR_VIBRADOR_PIN, HIGH); // Encender Motor Vibrador

  // Encender el LED correspondiente al compartimiento
  if (compartimiento >= 1 && compartimiento <= NUM_LEDS) {
    digitalWrite(ledPins[compartimiento - 1], HIGH); // -1 porque los arrays son base 0
    compartimientoActivo = compartimiento;
  } else {
    Serial.println("Número de compartimiento inválido. No se puede encender el LED.");
  }
}

void desactivarAlarma() {
  Serial.println("Desactivando alarma (Buzzer y Motor Vibrador).");
  digitalWrite(BUZZER_PIN, LOW);         // Apagar Buzzer
  digitalWrite(MOTOR_VIBRADOR_PIN, LOW);  // Apagar Motor Vibrador
}

// Función para actualizar el estado de una dosis específica en Firebase
void actualizarEstadoDosisFirebase(const String& nombreMedicamento, const String& dosisTimestampKey, bool tomado) {
  int statusCode = tomado ? 0 : 1; // 0 para tomado, 1 para no tomado (según tu imagen)
  String tomadoAt = ""; // Inicializar vacío para no tomado

  if (tomado) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      char timeStringBuff[50];
      strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%dT%H:%M:%SZ", &timeinfo); 
      tomadoAt = String(timeStringBuff);
    }
  }

  String path = "DataBase/WKiH1ESIcCgYgIUjyBKcfpgdAnu1/Medicamentos/" + nombreMedicamento + "/Dosis/" + dosisTimestampKey;

  FirebaseJson jsonUpdate;
  jsonUpdate.set("status_code", statusCode); // Guardar como número
  jsonUpdate.set("taken_at", tomadoAt); // Guardar el timestamp o vacío/null

  Serial.print("Actualizando dosis en Firebase: ");
  Serial.println(path);
  Serial.print("  status_code: "); Serial.println(statusCode);
  Serial.print("  taken_at: "); Serial.println(tomadoAt.isEmpty() ? "N/A" : tomadoAt);


  if (Firebase.updateNode(firebaseData, path.c_str(), jsonUpdate)) {
    Serial.println("Dosis actualizada correctamente en Firebase.");
  } else {
    Serial.print("Fallo al actualizar dosis en Firebase: ");
    Serial.println(firebaseData.errorReason());
  }
}

void registrarTomaMedicamento(bool tomado, String nombreMedicamento) {
  Serial.println("Advertencia: Se llamó a la función obsoleta registrarTomaMedicamento. Use actualizarEstadoDosisFirebase en su lugar.");
}

void printLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Fallo al obtener la hora NTP");
    return;
  }
  Serial.print("Hora actual: ");
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}