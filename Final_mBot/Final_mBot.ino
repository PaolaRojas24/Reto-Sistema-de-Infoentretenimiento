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
  
  Código correspondiente a la programación del mBot
*/

// Librerías de C
#include <avr/io.h>
#include <Arduino_FreeRTOS.h>
#include <util/delay.h>
#include <string.h>
#include "queue.h"

// Declaraciones de la tasa de comunicación serial
#define F_CPU 16000000UL
#define USART_BAUDRATE 19200
#define UBRR_VALUE (((F_CPU / (USART_BAUDRATE * 16UL))) - 1)

// Variables globales
unsigned char mybuffer[25];
char RX_Byte;

QueueHandle_t uartQueue; // Cola para manejar UART
TaskHandle_t xKeyboardTaskHandle = NULL;

void setup() {
  // Creación de tareas
  xTaskCreate(vKeyboardTask,"KEYBOARD TASK",100, NULL, 1, &xKeyboardTaskHandle);
  xTaskCreate(vSensoresTask,"SENSORES TASK",100, NULL, 2, NULL);
  xTaskCreate(vUARTTask,    "UART TASK",    100, NULL, 1, NULL);

  // Configuración del puerto serial
  UBRR2H = (uint8_t)(UBRR_VALUE >> 8); // Configurar baud rate alto
  UBRR2L = (uint8_t)UBRR_VALUE;        // Configurar baud rate bajo
  UCSR2C = 0x06; // Formato del frame: 8 bits de datos, 1 bit de stop
  UCSR2B |= (1 << RXEN2) | (1 << TXEN2); // Habilitar TX, RX
  UCSR2A = 0x00; // Limpiar banderas

  // Se declaran salidas y entradas
  DDRB |= (1 << 5); // OUTPUT en el bit 5 puerto B (Pin 11)
  DDRB |= (1 << 6); // OUTPUT en el bit 6 puerto B (Pin 12)
  DDRH |= (1 << 4); // OUTPUT en el bit 4 puerto H (Pin 7)
  DDRH |= (1 << 5); // OUTPUT en el bit 5 puerto H (Pin 8)
  DDRC = 0xFF;      // OUTPUT en todo el puerto C (A0 a A7)
  DDRG |= (1 << 0); // OUTPUT en el bit 0 puerto G (Pin 41)
  DDRG |= (1 << 1); // OUTPUT en el bit 1 puerto G (Pin 40)
  
  // Valor de comparación equivalente al 75%
  OCR1A = 192;
  OCR1B = 192;
  OCR4B = 192;
  OCR4C = 192;

  // Clear OCnX on compare match
  TCCR1A = 0;
  TCCR4A = 0;

  // Fast PWM, 8-bit
  TCCR1A = 0xA1; // COM1A1 = 1 COM1A0 = 0 COM1B1 = 1 COM1B0 = 0 WGM11 = 0 WGM10 = 1
  TCCR1B = 0x0B; // WGM12 = 1, WGM13 = 0, CS11 = 1, CS10 = 1
  TCCR4A = 0xA9; // COM4A1:0 = 10 (no inverso para OC4A) COM4B1:0 = 10 (no inverso para OC4B) COM4C1:0 = 10 (no inverso para OC4C) WGM41:40 = 01 (para modo "Phase Correct PWM")
  TCCR4B = 0x0B; // WGM12 = 1, WGM13 = 0, CS11 = 1, CS10 = 1

  // Se declaran entradas a los sensores 
  DDRF &= ~(1 << 6); // Sensor barrier 60 (Izquierda)
  DDRF &= ~(1 << 7); // Sensor barrier 61 (arriba)
  DDRK &= ~(1 << 0); // Sensor barrier 62 (derecha)
  DDRK &= ~(1 << 3); // Sensor de atrás izquierdo 
  DDRK &= ~(1 << 4); // Sensor de atrás izquierdo 

  // Crear la cola para manejar UART
  uartQueue = xQueueCreate(100, sizeof(mybuffer));
}

// Función para ir hacia adelante
void MoodA() {
  // Líneas de control M1
  PORTB |= (1 << 5);  // Asigna 1 en PB5
  PORTC |= (1 << 4);  // Asigna 1 en PC4
  PORTC &= ~(1 << 5); // Asigna 0 en PC5
  // Líneas de control M2
  PORTB |= (1 << 6);  // Asigna 1 en PB6
  PORTC |= (1 << 2);  // Asigna 1 en PC2
  PORTC &= ~(1 << 3); // Asigna 0 en PC3
  // Líneas de control M3
  PORTH |= (1 << 4);  // Asigna 1 en PH4
  PORTG |= (1 << 0);  // Asigna 1 en PG0
  PORTG &= ~(1 << 1); // Asigna 0 en PG1
  // Líneas de control M4
  PORTH |= (1 << 5);  // Asigna 1 en PH5
  PORTC |= (1 << 0);  // Asigna 1 en PC0
  PORTC &= ~(1 << 1); // Asigna 0 en PC1
}

// Función para ir hacia atrás 
void MoodC() {
  // Líneas de control M1
  PORTB |= (1 << 5);  // Asigna 1 en PB5
  PORTC &= ~(1 << 4); // Asigna 0 en PC4
  PORTC |= (1 << 5);  // Asigna 1 en PC5
  // Líneas de control M2
  PORTB |= (1 << 6);  // Asigna 1 en PB6
  PORTC &= ~(1 << 2); // Asigna 0 en PC2
  PORTC |= (1 << 3);  // Asigna 1 en PC3
  // Líneas de control M3
  PORTH |= (1 << 4);  // Asigna 1 en PH4
  PORTG &= ~(1 << 0); // Asigna 0 en PG0
  PORTG |= (1 << 1);  // Asigna 1 en PG1
  // Líneas de control M4
  PORTH |= (1 << 5);  // Asigna 1 en PH5
  PORTC &= ~(1 << 0); // Asigna 0 en PC0
  PORTC |= (1 << 1);  // Asigna 1 en PC1
}

// Función para girar a la izquierda
void MoodB() {
  // Líneas de control M1
  PORTB |= (1 << 5);  // Asigna 1 en PB5
  PORTC |= (1 << 4);  // Asigna 1 en PC4
  PORTC &= ~(1 << 5); // Asigna 0 en PC5
  // Líneas de control M2
  PORTB |= (1 << 6);  // Asigna 1 en PB6
  PORTC |= (1 << 2);  // Asigna 1 en PC2
  PORTC &= ~(1 << 3); // Asigna 0 en PC3
  // Líneas de control M3
  PORTH |= (1 << 4);  // Asigna 1 en PH4
  PORTG &= ~(1 << 0); // Asigna 0 en PG0
  PORTG &= ~(1 << 1); // Asigna 0 en PG1
  // Líneas de control M4
  PORTH |= (1 << 5);  // Asigna 1 en PH5
  PORTC &= ~(1 << 0); // Asigna 0 en PC0
  PORTC &= ~(1 << 1); // Asigna 0 en PC1
}

// Función para girar a la derecha
void MoodD() {
  // Líneas de control M1
  PORTB |= (1 << 5);  // Asigna 1 en PB5
  PORTC &= ~(1 << 4); // Asigna 0 en PC4
  PORTC &= ~(1 << 5); // Asigna 0 en PC5
  // Líneas de control M2
  PORTB |= (1 << 6);  // Asigna 1 en PB6
  PORTC &= ~(1 << 2); // Asigna 0 en PC2
  PORTC &= ~(1 << 3); // Asigna 0 en PC3
  // Líneas de control M3
  PORTH |= (1 << 4);  // Asigna 1 en PH4
  PORTG |= (1 << 0);  // Asigna 1 en PG0
  PORTG &= ~(1 << 1); // Asigna 0 en PG1
  // Líneas de control M4
  PORTH |= (1 << 5);  // Asigna 1 en PH5
  PORTC |= (1 << 0);  // Asigna 1 en PC0
  PORTC &= ~(1 << 1); // Asigna 0 en PC1
}

// Función para que se pare por completo
void Mood0() {
  // Líneas de control M1
  PORTB |= (1 << 5);  // Asigna 1 en PB5
  PORTC &= ~(1 << 4); // Asigna 0 en PC4
  PORTC &= ~(1 << 5); // Asigna 0 en PC5
  // Líneas de control M2
  PORTB |= (1 << 6);  // Asigna 1 en PB6
  PORTC &= ~(1 << 2); // Asigna 0 en PC2
  PORTC &= ~(1 << 3); // Asigna 0 en PC3
  // Líneas de control M3
  PORTH |= (1 << 4);  // Asigna 1 en PH4
  PORTG &= ~(1 << 0); // Asigna 0 en PG0
  PORTG &= ~(1 << 1); // Asigna 0 en PG1
  // Líneas de control M4
  PORTH |= (1 << 5);  // Asigna 1 en PH5
  PORTC &= ~(1 << 0); // Asigna 0 en PC0
  PORTC &= ~(1 << 1); // Asigna 0 en PC1
}

//Modo Danza 
void MoodDanza() {
    //Paso uno del baile 
    // Líneas de control M1
    PORTB |= (1 << 5);  // Asigna 1 en PB5
    PORTC &= ~(1 << 4); // Asigna 0 en PC4
    PORTC &= ~(1 << 5); // Asigna 0 en PC5
    // Líneas de control M2
    PORTB |= (1 << 6);  // Asigna 1 en PB6
    PORTC &= ~(1 << 2); // Asigna 0 en PC2
    PORTC &= ~(1 << 3);  // Asigna 0 en PC3
    // Líneas de control M3
    PORTH |= (1 << 4);  // Asigna 1 en PH4
    PORTG &= ~(1 << 0); // Asigna 0 en PG0
    PORTG |= (1 << 1);  // Asigna 1 en PG1
    // Líneas de control M4
    PORTH |= (1 << 5);  // Asigna 1 en PH5
    PORTC &= ~(1 << 0); // Asigna 0 en PC0
    PORTC |= (1 << 1);  // Asigna 1 en PC1
    _delay_ms(3000); // Espera 3 segundos
    
    //Paso 2
    PORTB |= (1 << 5);  // Asigna 1 en PB5
    PORTC &= ~(1 << 4); // Asigna 0 en PC4
    PORTC &= ~(1 << 5); // Asigna 0 en PC5
    // Líneas de control M2
    PORTB |=  (1 << 6);  // Asigna 1 en PB6
    PORTC &= ~(1 << 2);  // Asigna 0 en PC2
    PORTC &= ~(1 << 3);  // Asigna 0 en PC3
    // Líneas de control M3
    PORTH |= (1 << 4);  // Asigna 1 en PH4
    PORTG |= (1 << 0);  // Asigna 1 en PG0
    PORTG &= ~(1 << 1); // Asigna 0 en PG1
    // Líneas de control M4
    PORTH |= (1 << 5);  // Asigna 1 en PH5
    PORTC |= (1 << 0);  // Asigna 1 en PC0
    PORTC &= ~(1 << 1); // Asigna 0 en PC1
    _delay_ms(3000); // Espera 3 segundos
    
    //Paso 4 de baile 
    // Líneas de control M1
    PORTB |= (1 << 5);  // Asigna 1 en PB5
    PORTC |= (1 << 4);  // Asigna 1 en PC4
    PORTC &= ~(1 << 5); // Asigna 0 en PC5
    // Líneas de control M2
    PORTB |= (1 << 6);  // Asigna 1 en PB6
    PORTC |= (1 << 2);  // Asigna 1 en PC2
    PORTC &= ~(1 << 3); // Asigna 0 en PC3
    // Líneas de control M3
    PORTH |= (1 << 4);   // Asigna 1 en PH4
    PORTG &= ~(1 << 0);  // Asigna 0 en PG0
    PORTG &= ~(1 << 1); // Asigna 0 en PG1
    // Líneas de control M4
    PORTH |= (1 << 5); // Asigna 1 en PH5
    PORTC &= ~(1 << 0);// Asigna 1 en PC0
    PORTC &= ~(1 << 1);// Asigna 0 en PC1
    _delay_ms(3000); // Espera 3 segundos

    //Paso 3 de baile 
    // Líneas de control M1
    PORTB |= (1 << 5);  // Asigna 1 en PB5
    PORTC &= ~(1 << 4); // Asigna 0 en PC4
    PORTC |= (1 << 5);  // Asigna 1 en PC5
    // Líneas de control M2
    PORTB |= (1 << 6);  // Asigna 1 en PB6
    PORTC &= ~(1 << 2); // Asigna 0 en PC2
    PORTC |= (1 << 3);  // Asigna 1 en PC3
    // Líneas de control M3
    PORTH |= (1 << 4);  // Asigna 1 en PH4
    PORTG &= ~(1 << 0); // Asigna 0 en PG0
    PORTG &= ~(1 << 1); // Asigna 0 en PG1
    // Líneas de control M4
    PORTH |= (1 << 5);  // Asigna 1 en PH5
    PORTC &= ~(1 << 0); // Asigna 0 en PC0
    PORTC &= ~(1 << 1); // Asigna 0 en PC1
    _delay_ms(3000); // Espera 3 segundos
}

// Tarea para recibir valores del teclado
void vKeyboardTask(void *pvParameters) {
  while (1) {
    if (UCSR2A & (1 << 7)) {
      RX_Byte = UDR2;

      // A
      if (RX_Byte == 65) {
        MoodA();
      }
      // B
      else if (RX_Byte == 66) {
        MoodB();
      }
      // C
      else if (RX_Byte == 67) {
        MoodC();
      }
      // D
      else if (RX_Byte == 68) {
        MoodD();
      }
      // 0
      else if (RX_Byte == 48) {
        Mood0();
      }
      else if (RX_Byte == 49) {
        MoodDanza();
      }
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

//Tarea para que los sensores envíen datos
void vSensoresTask(void *pvParameters) {
  while (1) {
    //Lectura de los datos de los sensores
    uint8_t sensor_65 = PINK & (1 << 3);
    uint8_t sensor_66 = PINK & (1 << 4);
    uint8_t sensor_60 = PINF & (1 << 6);
    uint8_t sensor_61 = PINF & (1 << 7);
    uint8_t sensor_62 = PINK & (1 << 0);
    int32_t valueToSend;

    //Pausar canción
    if (sensor_60 && !sensor_61 && sensor_62){
      sprintf(mybuffer, "\nB");
      USART_Transmit_String((unsigned char *)mybuffer);
      xTaskNotifyGive(xKeyboardTaskHandle);
    }
    //Repetir canción
    else if (sensor_60 && sensor_61 && !sensor_62){
      sprintf(mybuffer, "\nD");
      USART_Transmit_String((unsigned char *)mybuffer);
      xTaskNotifyGive(xKeyboardTaskHandle);
    }
    //Atrás canción
    else if (!sensor_65 && sensor_66){
      valueToSend = rand()% 51;
      sprintf(mybuffer, "\nA",valueToSend);
      USART_Transmit_String((unsigned char *)mybuffer);
      xTaskNotifyGive(xKeyboardTaskHandle);
    }
    //Siguiente canción
    else if (sensor_65 && !sensor_66){
      valueToSend = rand()% 51+50;
      sprintf(mybuffer, "\nC",valueToSend);
      USART_Transmit_String((unsigned char *)mybuffer);
      xTaskNotifyGive(xKeyboardTaskHandle);
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}


// Tarea para manejar UART
void vUARTTask(void *pvParameters) {
  while (1) {
    if (xQueueReceive(uartQueue, &mybuffer, portMAX_DELAY) == pdPASS) {
      USART_Transmit_String((unsigned char *)mybuffer);
    }
  }
}

//Funciones de transmisión del UART
void USART_Transmit(unsigned char data) {
  // Wait for empty transmit buffer
  while (!(UCSR2A & (1 << UDRE2)));
  // Put data into buffer, send data
  UDR2 = data;
}

void USART_Transmit_String(unsigned char *pdata) {
  unsigned char i;
  // Calculate string length
  unsigned char len = strlen((char *)pdata);
  // Transmit byte by byte
  for (i = 0; i < len; i++) {
    // Wait for empty transmit buffer
    while (!(UCSR2A & (1 << UDRE2)));
    // Put data into buffer, send data
    UDR2 = pdata[i];
  }
}

//Loop vacío
void loop() {}
