#define RXD2 16
#define TXD2 17

#include <OneWire.h>
#include <DallasTemperature.h>
#include <LoRa.h> // Include the LoRa library

#define ONE_WIRE_BUS 15
#define GPS_BAUD 9600
#define phPin 12  
#define TurPin 13 

// LoRa pins
#define LORA_NSS 5
#define LORA_RST 14
#define LORA_DIO0 2

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

HardwareSerial gpsSerial(2);

void adjustToIST(int &utcHour, int &utcMinute, int &utcSecond) {
  utcMinute += 30;
  utcHour += 5;

  if (utcMinute >= 60) {
    utcMinute -= 60;
    utcHour += 1;
  }
  if (utcHour >= 24) {
    utcHour -= 24;
  }

  Serial.printf("IST Time: %02d:%02d:%02d\n", utcHour, utcMinute, utcSecond);
}

float convertToDecimal(String coordinate, String direction) {
  if (coordinate.length() < 4) return 0.0;

  float degrees = coordinate.substring(0, coordinate.indexOf('.') - 2).toFloat();
  float minutes = coordinate.substring(coordinate.indexOf('.') - 2).toFloat();
  float decimal = degrees + (minutes / 60.0);

  if (direction == "S" || direction == "W") decimal = -decimal;
  return decimal;
}

void phSensor() {
  int buf[10], temp;
  unsigned long int avgValue = 0;

  for (int i = 0; i < 10; i++) {
    buf[i] = analogRead(phPin);
    delay(10);
  }

  for (int i = 0; i < 9; i++) {
    for (int j = i + 1; j < 10; j++) {
      if (buf[i] > buf[j]) {
        temp = buf[i];
        buf[i] = buf[j];
        buf[j] = temp;
      }
    }
  }

  for (int i = 2; i < 8; i++) avgValue += buf[i];

  float phValue = (float)avgValue * 5.0 / 1024 / 6;
  phValue = 3.5 * phValue - 33.4;

  Serial.print("pH: ");
  Serial.println(phValue, 2);
  return phValue;
}

void turbiditySensor() {
  int sensorValue = analogRead(TurPin);
  int turbidity = map(sensorValue, 100, 1679, 100, 0);

  Serial.print("Turbidity: ");
  Serial.println(turbidity);
  return turbidity;
}

void temperatureSensor() {
  sensors.requestTemperatures(); 
  float tempC = sensors.getTempCByIndex(0);
  Serial.print("Temperature: ");
  Serial.println(tempC);
  return tempC;
}

void sendLoRaData(float lat, float lon, int hour, int minute, int second, float ph, int turbidity, float temp) {
  // Create a JSON-like string to send over LoRa
  String data = "{\"lat\":" + String(lat, 4) + 
                ",\"lon\":" + String(lon, 4) + 
                ",\"time\":\"" + String(hour) + ":" + String(minute) + ":" + String(second) + "\"" +
                ",\"ph\":" + String(ph, 2) + 
                ",\"turbidity\":" + String(turbidity) + 
                ",\"temp\":" + String(temp, 2) + "}";

  // Send the data over LoRa
  LoRa.beginPacket();
  LoRa.print(data);
  LoRa.endPacket();

  Serial.println("Data sent over LoRa: " + data);
}

void setup() {
  Serial.begin(115200);
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, RXD2, TXD2);
  Serial.println("Serial2 started at 9600 baud rate");
  sensors.begin();

  // Initialize LoRa
  LoRa.setPins(LORA_NSS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(433E6)) { // Use 433 MHz frequency (change as needed)
    Serial.println("LoRa initialization failed!");
    while (1);
  }
  Serial.println("LoRa initialized successfully!");
}

void parseNMEA(String nmea) {
  if (nmea.startsWith("$GPRMC")) {
    String fields[12];
    int fieldIndex = 0;
    int startIndex = 0;

    for (int i = 0; i < nmea.length(); i++) {
      if (nmea[i] == ',' || i == nmea.length() - 1) {
        fields[fieldIndex++] = nmea.substring(startIndex, i);
        startIndex = i + 1;
        if (fieldIndex >= 12) break;
      }
    }

    String time = fields[1];        
    String status = fields[2];      
    String latitude = fields[3];    
    String latDirection = fields[4];
    String longitude = fields[5];   
    String lonDirection = fields[6];

    if (status == "A") { 
      int utcHour = time.substring(0, 2).toInt();
      int utcMinute = time.substring(2, 4).toInt();
      int utcSecond = time.substring(4, 6).toInt();

      adjustToIST(utcHour, utcMinute, utcSecond);

      float latDecimal = convertToDecimal(latitude, latDirection);
      float lonDecimal = convertToDecimal(longitude, lonDirection);

      Serial.print("Latitude: ");
      Serial.println(latDecimal, 4);
      Serial.print("Longitude: ");
      Serial.println(lonDecimal, 4);

      // Read sensor data
      float ph = phSensor();
      int turbidity = turbiditySensor();
      float temp = temperatureSensor();

      // Send data over LoRa
      sendLoRaData(latDecimal, lonDecimal, utcHour, utcMinute, utcSecond, ph, turbidity, temp);
    } else {
      Serial.println("GPS data invalid");
    }
  }
}

void loop() {
  static String nmeaSentence = "";

  while (gpsSerial.available() > 0) {
    char gpsData = gpsSerial.read();
    if (gpsData == '\n') {
      parseNMEA(nmeaSentence);
      nmeaSentence = "";
    } else if (gpsData != '\r') {
      nmeaSentence += gpsData; 
    }
  }
}