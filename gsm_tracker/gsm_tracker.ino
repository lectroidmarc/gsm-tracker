
#include <SoftwareSerial.h>
#include <Adafruit_GPS.h>
#include <Adafruit_FONA.h>

#include "Location.h"
#include "keys.h"

#define FONA_RST 4
#define FONA_PS 5
#define FONA_KEY 6
#define FONA_RI 2    // <-- must be connected to external interrupt pin
#define FONA_RX 8
#define FONA_TX 9

#define GPS_RX 10
#define GPS_TX 11

#define BUZZER_PIN 12
#define FIX_LED 13

#define GPS_INTERVAL_SECONDS 10


SoftwareSerial gpsSS = SoftwareSerial(GPS_TX, GPS_RX);
Adafruit_GPS gps = Adafruit_GPS(&gpsSS);

SoftwareSerial fonaSS = SoftwareSerial(FONA_TX, FONA_RX);
Adafruit_FONA fona = Adafruit_FONA(&fonaSS, FONA_RST);

Location current_location = Location();

unsigned long timer = 0;
volatile boolean ringing = false;


// Interrupt is called once a millisecond, looks for any new GPS data, and stores it
SIGNAL(TIMER0_COMPA_vect) {
  char c = gps.read();
  //if (c) UDR0 = c;
}


void ringInterrupt () {
  ringing = true;
}


void setup() {
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, HIGH);
  delay(500);
  digitalWrite(BUZZER_PIN, LOW);

  Serial.begin(115200);
  Serial.println(F("GPS Tracker."));

  pinMode(FONA_KEY, OUTPUT);
  digitalWrite(FONA_KEY, HIGH);

  // Power up the FONA if it needs it
  if (digitalRead(FONA_PS) == LOW) {
    Serial.print(F("Powering FONA on..."));
    while (digitalRead(FONA_PS) == LOW) {
      digitalWrite(FONA_KEY, LOW);
      delay(500);
    }
    digitalWrite(FONA_KEY, HIGH);
    Serial.println(F(" done."));
    delay(500);
  }

  // Start the FONA
  Serial.print(F("Initializing FONA..."));
  while (! fona.begin(4800)) {
    Serial.print(F(" waiting..."));
    delay (1000);
  }
  Serial.println(F(" done."));

  // wait for a valid network, nothing works w/o that
  Serial.print(F("Waiting for GSM network..."));
  while (1) {
    uint8_t network_status = fona.getNetworkStatus();
    if (network_status == 1 || network_status == 5) break;
    delay(250);
  }
  Serial.println(F(" done."));

  // Attach the RI interrupt
  attachInterrupt(0, ringInterrupt, FALLING);


  // We need to start the GPS second... By default, the last intialized port is listening.
  Serial.print(F("Starting GPS..."));
  gps.begin(9600);
  gps.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
  gps.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);   // 1 Hz update rate
  pinMode(FIX_LED, OUTPUT);
  Serial.println(F(" done."));


  // Timer0 is already used for millis() - we'll just interrupt somewhere
  // in the middle and call the "Compare A" function above
  OCR0A = 0xAF;
  TIMSK0 |= _BV(OCIE0A);
}


void loop() {
  if (ringing && digitalRead(FONA_PS) == HIGH) handleRing();

  if (gps.newNMEAreceived() && gps.parse(gps.lastNMEA())) {
    boolean isValidFix = gps.fix && gps.HDOP < 5 && gps.HDOP != 0;  // HDOP == 0 is an error case that comes up on occasion.

    digitalWrite(FIX_LED, isValidFix ? HIGH : LOW);

    if (isValidFix) current_location.set(gps);

    if (millis() - timer > GPS_INTERVAL_SECONDS * 1000) {
      Serial.print(F("Satellites: ")); Serial.println((int)gps.satellites);

      if (isValidFix) {
        Serial.print(F("Location: "));
        Serial.print(current_location.latitude, 6);
        Serial.print(F(", "));
        Serial.print(current_location.longitude, 6);
        Serial.print(F(", "));
        Serial.println(current_location.altitude, 2);

        Serial.print(F("HDOP: ")); Serial.println(gps.HDOP);

        //if (digitalRead(FONA_PS) == HIGH && !current_location.isEqual(last_location)) {
        if (digitalRead(FONA_PS) == HIGH) {
          sendLocation();
        //  last_location.set(gps);
        }
      } else {
        Serial.println(F("No valid fix."));
      }
      Serial.println();

      timer = millis(); // reset the timer
    }
  }

  // if millis() wraps around, reset the timer
  if (timer > millis()) timer = millis();
}


void sendLocation () {
  char url[200];
  uint16_t statuscode;
  int16_t length;

  sprintf (url, "http://data.sparkfun.com/input/%s?private_key=%s&latitude=%s&longitude=%s&altitude=%s",
    SPARKFUN_PUBLIC_KEY, SPARKFUN_PRIVATE_KEY, current_location.latitude_c, current_location.longitude_c, current_location.altitude_c);

  Serial.print(F("Sending: ")); Serial.println(url);

  // Make the FONA listen, we kinda need that...
  fonaSS.listen();

  uint8_t rssi = fona.getRSSI();

  if (rssi > 5) {
    if (fona.enableGPRS(true)) {
      if (fona.HTTP_GET_start(url, &statuscode, (uint16_t *)&length)) {

        // Successful transmission, handle and check response data...
        if (statuscode != 200) {
          Serial.print(F("Error sending data: "));

          while (length > 0) {
            while (fona.available()) {
              char c = fona.read();

              // Serial.write is too slow, we'll write directly to Serial register!
              loop_until_bit_is_set(UCSR0A, UDRE0); /* Wait until data register empty. */
              UDR0 = c;

              length--;
              if (! length) break;
            }
          }
        } else {
          // give time to recieve the datas
          delay (50);
        }

      } else {
        Serial.println(F("Failed to send GPRS data!"));
      }

      if (!fona.enableGPRS(false)) {
        Serial.println(F("Failed to turn GPRS off!"));
      }
    } else {
      Serial.println(F("Failed to turn GPRS on!"));
    }
  } else {
    Serial.println(F("Can't transmit, network signal strength is crap!"));
  }

  // Put the GPS back into listen mode
  gpsSS.listen();
}


void handleRing () {
  char sms_buffer[140];
  uint16_t smslen;

  Serial.println(F("Ring ring, Neo."));

  fonaSS.listen();

  int8_t sms_num = fona.getNumSMS();

  if (sms_num == -1) {
    // This is an error case
    ringing = true;
    gpsSS.listen();
    return;
  }

  Serial.print(sms_num); Serial.println(" messages waiting.");

  // Read any SMS message we may have...
  for (int8_t sms_index = 1; sms_index <= sms_num; sms_index++) {
    Serial.print(F("  SMS #")); Serial.print(sms_index); Serial.print(F(": "));

    if (fona.readSMS(sms_index, sms_buffer, 250, &smslen)) {
      // if the length is zero, its a special case where the index number is higher
      // so increase the max we'll look at!
      if (smslen == 0) {
        Serial.println(F("[empty slot]"));
        sms_num++;
        continue;
      }

      Serial.println(sms_buffer);

      // If it matches our pre-defined command string...
      if (strcmp(sms_buffer, "Location") == 0) {
        Serial.println(F("  Responding with location... "));

        char sms_response[52];

        if (current_location.isValid) {
          sprintf (sms_response, "https://maps.google.com?q=%s,%s", current_location.latitude_c, current_location.longitude_c);
        } else {
          sprintf (sms_response, "I'm lost!");
        }

        // reply...
        if (fona.sendSMS(MY_PHONE_NUMBER, sms_response)) {
          Serial.println(F("reply sent!"));
        } else {
          Serial.println(F("reply failed"));
        }

        // delete this SMS
        fona.deleteSMS(sms_index);
      } else if (strcmp(sms_buffer, "Status") == 0) {
        Serial.println(F("  Responding with status... "));

        char sms_response[8];
        uint8_t rssi = fona.getRSSI();
        sprintf (sms_response, "%d bars.", barsFromRSSI(rssi));

        // reply...
        if (fona.sendSMS(MY_PHONE_NUMBER, sms_response)) {
          Serial.println(F("reply sent!"));
        } else {
          Serial.println(F("reply failed"));
        }

        // delete this SMS
        fona.deleteSMS(sms_index);
      }
    } else {
      Serial.println(F("Failed to read SMS messages!"));
    }
  }

  gpsSS.listen();

  ringing = false;
}


uint8_t barsFromRSSI (uint8_t rssi) {
  // https://en.wikipedia.org/wiki/Mobile_phone_signal#ASU
  //
  // In GSM networks, ASU maps to RSSI (received signal strength indicator, see TS 27.007[1] sub clause 8.5).
  //   dBm = 2 × ASU - 113, ASU in the range of 0..31 and 99 (for not known or not detectable).

  int8_t dbm = 2 * rssi - 113;

  if (rssi == 99 || rssi == 0) {
    return 0;
  } else if (dbm < -107) {
    return 1;
  } else if (dbm < -98) {
    return 2;
  } else if (dbm < -87) {
    return 3;
  } else if (dbm < -76) {
    return 4;
  }

  return 5;
}
