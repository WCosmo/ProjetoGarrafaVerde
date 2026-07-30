/*
  Projeto: Garrafa Verde
  Objetivo: Monitorar a temperatura e a turbidez de rios e lagos.
 
  Hardware:
    - ESP32-C3 Mini
    - Sensor de Temperatura e Umidade SHT31
    - Sensor de Turbidez DigiKey Turbidity Sensor 1.0
 
  Monitoramento IoT:
    - ESP32-S3 Xiao
   - Plataforma: ThingSpeak
 
  Data: 21/07/2026
 
  Autores:
    - Bruna
    - Giovanna
    - Gabryella
    - Stefany
 
*/

//Bibliotecas
#include <Arduino.h>
#include <Wire.h>
#include "Adafruit_SHT31.h"
#include <WiFi.h>
#include "secrets.h"
#include "ThingSpeak.h" 

// ==================== Configuração dos sensores de Turbidez ====================
#define TURBIDITY_PIN_1 4
// Valores de calibração 
#define VALUE_CLEAR_WATER   3500   // ADC em água limpa
#define VALUE_TURBID_WATER  800    // ADC em água turva

// Filtro de média móvel
#define NUM_SAMPLES 10


// Estrutura para armazenar dados de cada sensor de turbidez
struct TurbiditySensor {
  int pin;
  float samples[NUM_SAMPLES];
  int sampleIndex;
  float filteredRaw;
  float voltage;
  float percent;
};

// Instâncias dos sensores de turbidez
TurbiditySensor sensor1 = {TURBIDITY_PIN_1, {0}, 0, 0, 0, 0};


// ==================== PROTÓTIPOS DAS FUNÇÕES ====================
float movingAverageFilter(TurbiditySensor *sensor, float newValue);
float readFilteredAnalog(TurbiditySensor *sensor);
float calculateTurbidityPercentage(float rawValue);
void updateTurbiditySensor(TurbiditySensor *sensor);

int contador = 0;

char ssid[] = SECRET_SSID;   // SSID (nome) da sua rede Wi-Fi.SSID (nome) da sua rede Wi-Fi.
char pass[] = SECRET_PASS;   // Senha da sua rede Wi-Fi.
int keyIndex = 0;            //Número do índice da chave da rede (necessário apenas para WEP).
WiFiClient  client;

unsigned long myChannelNumber = SECRET_CH_ID;
const char * myWriteAPIKey = SECRET_WRITE_APIKEY;

Adafruit_SHT31 sht31_1 = Adafruit_SHT31();

//Inicializar nossos valores.
int number1 = 0;
int number2 = random(0,100);
int number3 = random(0,100);
int number4 = random(0,100);
String myStatus = "";

void setup() {
  Serial.begin(115200);  //Inicializar a comunicação serial.
  while (!Serial) {
    ; //Aguarda a conexão da porta serial. Necessário apenas para a porta USB nativa do Arduino.
  }
  

  
  WiFi.mode(WIFI_STA);   
  ThingSpeak.begin(client);  // Inicializar o ThingSpeak.

  Serial.println("Iniciando teste de UM SHT31...");

//  Inicializa o primeiro sensor (0x44)
  if (!sht31_1.begin(0x44)) {
    Serial.println("Sensor 1 (0x44) não encontrado!");
    while (1) delay(1);
  }
  // --- Inicialização dos sensores de turbidez ---
  analogReadResolution(12);
  pinMode(sensor1.pin, INPUT);


  // Inicializa buffers dos filtros
  for (int i = 0; i < NUM_SAMPLES; i++) {
    sensor1.samples[i] = 0;
  
  }


}


void loop() {
// --- Leitura dos sensores de turbidez ---
  updateTurbiditySensor(&sensor1);
  

  // --- Leitura da temperatura (apenas se disponível) ---
 

  // --- Saída CSV ---
  Serial.print(sensor1.percent, 1);


  
  Serial.println();

 // Lê os dados do primeiro sensor (0x44)
  float t1 = sht31_1.readTemperature();  
  int t2 = sensor1.percent;
  // Exibe os dados do Sensor 
  if (!isnan(t1)) {
    Serial.print("Sensor 1 - Temp. *C = "); Serial.print(t1); Serial.print("\t");
  } else {
    Serial.println("Sensor 1 - Falha na leitura da temperatura");
  }

 

  Serial.println("-----------------------------------");


  //Conecta ou reconecta ao Wi-Fi
  if(WiFi.status() != WL_CONNECTED){
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(SECRET_SSID);
    while(WiFi.status() != WL_CONNECTED){
      WiFi.begin(ssid, pass);  // Conecta-se a uma rede WPA/WPA2. Altere esta linha caso esteja utilizando uma rede aberta ou com criptografia WEP.
      Serial.print(".");
      delay(5000);     
    } 
    Serial.println("\nConnected.");
  }

  // Define os campos com os valores.
  ThingSpeak.setField(1, t1);
  ThingSpeak.setField(2, t2);
  
  
  
  
  contador = contador + 1;

  myStatus = String("Leitura número ") + String(contador) + (" realizada");
  Serial.println(myStatus);


  // Define o status.
  ThingSpeak.setStatus(myStatus);
  
  // Envia os dados para o canal do ThingSpeak.
  int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
  if(x == 200){
    Serial.println("Channel update successful.");
  }
  else{
    Serial.println("Problem updating channel. HTTP error code " + String(x));
  }
  
 
  
  delay(20000); //Aguarda 20 segundos para atualizar o canal novamente
}

/**
 * Atualiza todos os parâmetros de um sensor de turbidez
 */
void updateTurbiditySensor(TurbiditySensor *sensor) {
  sensor->filteredRaw = readFilteredAnalog(sensor);
  sensor->voltage = (sensor->filteredRaw / 4095.0) * 3.3;
  sensor->percent = calculateTurbidityPercentage(sensor->filteredRaw);
}

/**
 * Lê o valor do ADC e aplica o filtro de média móvel
 */
float readFilteredAnalog(TurbiditySensor *sensor) {
  float raw = analogRead(sensor->pin);
  return movingAverageFilter(sensor, raw);
}

/**
 * Filtro de média móvel (mantém os últimos N valores)
 */
float movingAverageFilter(TurbiditySensor *sensor, float newValue) {
  sensor->samples[sensor->sampleIndex] = newValue;
  sensor->sampleIndex = (sensor->sampleIndex + 1) % NUM_SAMPLES;
  
  float sum = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    sum += sensor->samples[i];
  }
  return sum / NUM_SAMPLES;
}

/**
 * Converte o valor bruto do ADC (filtrado) em porcentagem de turbidez
 */
float calculateTurbidityPercentage(float rawValue) {
  float constrained = rawValue;
  if (constrained > VALUE_CLEAR_WATER) constrained = VALUE_CLEAR_WATER;
  if (constrained < VALUE_TURBID_WATER) constrained = VALUE_TURBID_WATER;
  
  float percent = map(constrained, VALUE_TURBID_WATER, VALUE_CLEAR_WATER, 100, 0);
  
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;
  return percent;
}
