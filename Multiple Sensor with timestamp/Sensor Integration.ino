#include <SPI.h>
#include <LoRa.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define NSS 5
#define RST 14
#define DIO0 26
#define MOSI 23
#define MISO 19
#define SCK 18

#define RXD2 16  // GPS TX pin
#define TXD2 17  // GPS RX pin

#define ONE_WIRE_BUS 13
#define GPS_BAUD 9600
#define phPin 12
#define TurPin 35

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
HardwareSerial gpsSerial(2);  // Using UART2 for GPS

// Global variables to store the latest valid GPS data
String latestTime = "00:00:00";
String latestLatitude = "0.000000";
String latestLongitude = "0.000000";

void setup() {
    Serial.begin(115200);
    gpsSerial.begin(GPS_BAUD, SERIAL_8N1, RXD2, TXD2);
    Serial.println("GPS Serial started at 9600 baud");
    
    sensors.begin();

    LoRa.setPins(NSS, RST, DIO0);
    if (!LoRa.begin(433E6)) {
        Serial.println("LoRa initialization failed!");
        while (1);
    }
    Serial.println("LoRa initialized successfully");
}

void loop() {
    String time, latitude, longitude;
    // Try to update the GPS data
    if (readGPS(time, latitude, longitude)) {
        // If new valid GPS data is received, update the global variables
        latestTime = time;
        latestLatitude = latitude;
        latestLongitude = longitude;
    }
  
    // Read sensor data
    float temperature = readTemperature();
    float turbidity = readTurbidity();
    float pH = readPH();

    // Build payload using the latest GPS data (even if not updated this iteration)
    String payload = latestTime + "," + latestLatitude + "," + latestLongitude + "," +
                     String(temperature) + "," + String(turbidity) + "," + String(pH, 2);

    // Print payload to Serial and send it via LoRa
    Serial.println("Sending: " + payload);
    sendLoRa(payload);

    delay(1000);  // Adjust as needed
}

/* Function to Read GPS Data */
bool readGPS(String &time, String &latitude, String &longitude) {
    static String nmeaSentence = "";

    while (gpsSerial.available() > 0) {
        char gpsData = gpsSerial.read();
        if (gpsData == '\n') {
            // We received a complete NMEA sentence
            if (parseNMEA(nmeaSentence, time, latitude, longitude)) {
                nmeaSentence = "";
                return true;
            }
            nmeaSentence = "";
        } else if (gpsData != '\r') {
            nmeaSentence += gpsData;
        }
    }
    return false;
}

/* Parse a GPRMC sentence and extract time, latitude, and longitude */
bool parseNMEA(String nmea, String &time, String &latitude, String &longitude) {
    if (!nmea.startsWith("$GPRMC")) return false;

    String fields[12];
    int fieldIndex = 0, startIndex = 0;
    for (int i = 0; i < nmea.length(); i++) {
        if (nmea[i] == ',' || i == nmea.length() - 1) {
            fields[fieldIndex++] = nmea.substring(startIndex, i);
            startIndex = i + 1;
            if (fieldIndex >= 12) break;
        }
    }

    // Check if GPS data is valid (status field is "A")
    if (fields[2] != "A") {
        Serial.println("GPS data invalid");
        return false;
    }

    // Extract time from fields[1] (format hhmmss.sss)
    if (fields[1].length() < 6) return false;
    int utcHour = fields[1].substring(0, 2).toInt();
    int utcMinute = fields[1].substring(2, 4).toInt();
    int utcSecond = fields[1].substring(4, 6).toInt();
    adjustToIST(utcHour, utcMinute, utcSecond);
    time = String(utcHour) + ":" + String(utcMinute) + ":" + String(utcSecond);

    // Convert latitude and longitude to decimal degrees
    latitude = String(convertToDecimal(fields[3], fields[4]), 6);
    longitude = String(convertToDecimal(fields[5], fields[6]), 6);

    return true;
}

/* Adjust UTC time to IST (UTC+5:30) */
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
}

/* Convert coordinate from NMEA format to decimal degrees */
float convertToDecimal(String coordinate, String direction) {
    if (coordinate.length() < 4) return 0.0;
    float degrees = coordinate.substring(0, coordinate.indexOf('.') - 2).toFloat();
    float minutes = coordinate.substring(coordinate.indexOf('.') - 2).toFloat();
    float decimal = degrees + (minutes / 60.0);
    if (direction == "S" || direction == "W") decimal = -decimal;
    return decimal;
}

/* Function to Read Temperature from DS18B20 */
float readTemperature() {
    sensors.requestTemperatures();
    float tempC = sensors.getTempCByIndex(0);
    return tempC;
}

/* Function to Read Turbidity */
float readTurbidity() {
    int sensorValue = analogRead(TurPin);
    // Adjust mapping based on your sensor's characteristics
    int turbidity = map(sensorValue, 0, 1800, 100, 0);
    return turbidity;
}

/* Function to Read pH */
float readPH() {
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
    return phValue;
}

/* Function to Send Data via LoRa */
void sendLoRa(String data) {
    LoRa.beginPacket();
    LoRa.print(data);
    LoRa.endPacket();
}