#define RXD2 16
#define TXD2 17

#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS 15
#define GPS_BAUD 9600
#define phPin 12  
#define TurPin 13 

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

HardwareSerial gpsSerial(2);

void setup() {
  Serial.begin(115200);
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, RXD2, TXD2);
  Serial.println("Serial2 started at 9600 baud rate");
  sensors.begin();
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
    } else {
      Serial.println("GPS data invalid");
    }

    phSensor();
    turbiditySensor();
    temperatureSensor();
  }
}

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
}

void turbiditySensor() {
  int sensorValue = analogRead(TurPin);
  int turbidity = map(sensorValue, 100, 1679, 100, 0);

  Serial.print("Turbidity: ");
  Serial.println(turbidity);
}

void temperatureSensor() {
  sensors.requestTemperatures(); 
  float tempC = sensors.getTempCByIndex(0);
  Serial.print("Temperature: ");
  Serial.println(tempC);
}