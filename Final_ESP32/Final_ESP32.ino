/*
  Reto: Sistema de Infoentretenimiento
  Instituto Tecnológico de Estudios Superiores de Monterrey
  Diseño de sistemas en chip (Gpo 501)
  
  Profesores: Jorge Andrade Román
              David Antonio Torres
              Carlos Leopoldo Carreón Díaz de León
  
  Equipo 4: Daniel Castillo López A01737357
            Ana Itzel Hernández García A01737526
            Paola Rojas Domínguez A01737136
  
  Código correspondiente a la programación de la ESP32
*/

//Librerías
#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <Keypad.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// Definición de las credenciales de la red WiFi
#define WIFI_SSID "CelPao"
#define WIFI_PASSWORD "Ch1mu3l0"

// Clave API y URL de la base de datos de Firebase
#define API_KEY "AIzaSyCfowCIIFNHU_QGtRH0hKxNfshiyvsdSkk"
#define DATABASE_URL "https://soc-reto-default-rtdb.firebaseio.com/"

// Inicialización de objetos de Firebase y configuración de autenticación
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
bool signupOK = false;

// Pines para la transmisión del UART
#define RXD2 16
#define TXD2 17

// Definición de filas y columnas del teclado matricial (Keypad)
#define ROW_NUM 4
#define COLUMN_NUM 4

// Mapeo de teclas del Keypad
char keys[ROW_NUM][COLUMN_NUM] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

// Pines de las filas y columnas del Keypad
byte pin_rows[ROW_NUM] = {23, 22, 21, 19};
byte pin_column[COLUMN_NUM] = {18, 5, 4, 15};

// Creación del objeto Keypad
Keypad keypad = Keypad(makeKeymap(keys), pin_rows, pin_column, ROW_NUM, COLUMN_NUM);

// Variables
String numbers;
String letters;
bool isReproductorMode = false; // Variable para rastrear el modo actual

void setup() {
  // Inicialización de la comunicación serial con los pines definidos y un baud rate de 19200
  Serial2.begin(19200, SERIAL_8N1, RXD2, TXD2);
  Serial2.setTimeout(10); // Establece un tiempo de espera para la función Serial2.read()

  // Conexión a la red WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
    delay(300);
  }
  if (WiFi.status() != WL_CONNECTED) {
    ESP.restart();
  }

  // Configuración de Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  // Registro en Firebase
  if (Firebase.signUp(&config, &auth, "", "")) {
    signupOK = true;
  }

  // Configuración del callback para el estado del token
  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Reserva de memoria para las variables String
  numbers.reserve(32);
  letters.reserve(32);

  // Creación de tareas para la lectura de datos desde Firebase y desde el Keypad
  xTaskCreate(taskFirebaseGet, "Firebase Get", 8192, NULL, 1, NULL);
  xTaskCreate(taskKeypadRead, "Keypad Read", 8192, NULL, 1, NULL);
}

// Función para manejar eventos seriales en el puerto 2
void serialEvent2() {
  String data = Serial2.readStringUntil('\n');
  if (data.length() > 0) {
    if (Firebase.ready() && signupOK) {
      Firebase.RTDB.setString(&fbdo, "test/reproductor", data);
    }
  }
}

// Tarea para obtener datos desde Firebase
void taskFirebaseGet(void * parameter) {
  while(1) {
    if (Firebase.RTDB.getString(&fbdo, "test/motor")) {
      Serial2.println(fbdo.stringData());
    }
    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
}

// Tarea para leer el teclado matricial (Keypad)
void taskKeypadRead(void * parameter) {
  while(1) {
    char key = keypad.getKey();
    if (key) {
      // Cuando la tecla es "*"
      if (key == '*') {
        numbers = "";
        letters = "";
        isReproductorMode = !isReproductorMode; // Cambia el modo

        // Enviar el modo actual a Firebase
        String modo = isReproductorMode ? "reproductor" : "mBot";
        Firebase.RTDB.setString(&fbdo, "test/modo", modo);
      }
      // Cuando la tecla es "#"
      else if (key == '#') {
        // Mandar datos a la Firebase
        if (Firebase.ready() && signupOK) {
          // Si la tecla es un número
          if (!numbers.isEmpty()) {
            if (isReproductorMode) {
              Firebase.RTDB.setString(&fbdo, "test/cancion", numbers);
            }
            else {
              Firebase.RTDB.setString(&fbdo, "test/motor", numbers);
            }
            numbers = "";
          }
          // Si la tecla es una letra
          if (!letters.isEmpty()) {
            if (isReproductorMode) {
              Firebase.RTDB.setString(&fbdo, "test/reproductor", letters);
            }
            else {
              Firebase.RTDB.setString(&fbdo, "test/motor", letters);
            }
            letters = "";
          }
        }
      }
      else {
        // Cuando la tecla es un número
        if (isdigit(key)) {
          numbers += key;
        }
        // Cuando la tecla es una letra
        else if (isalpha(key)) {
          letters += key;
        }
      }
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

//Loop con delay
void loop() {
  delay(100);
}
