
#include <SoftwareSerial.h>
#include <Adafruit_GPS.h>
#include <Adafruit_FONA.h>

#include "Location.h"
#include "keys.h"

#define FONA_RST A0
#define FONA_PS A1
#define FONA_KEY A2
#define FONA_RI 3    // <-- must be connected to external interrupt pin
#define FONA_RX A4
#define FONA_TX A5

#define GPS_RX 11
#define GPS_TX 12

#define BUZZER_PIN 4
#define FIX_LED 13

#define GPS_INTERVAL_SECONDS 10


SoftwareSerial gpsSerial = SoftwareSerial(GPS_TX, GPS_RX);
Adafruit_GPS gps = Adafruit_GPS(&gpsSerial);

SoftwareSerial fonaSerial = SoftwareSerial(FONA_TX, FONA_RX);
Adafruit_FONA fona = Adafruit_FONA(&fonaSerial, FONA_RST);

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


void setup () {
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
  while (1) {
    boolean fonaStarted = fona.begin(4800);
    if (fonaStarted) break;
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
  // Sometimes the FONA can't recieve SMSs until it sends one first.
  fonaSendStatusSMS(MY_PHONE_NUMBER);
  Serial.println(F(" done."));

  // Attach the RI interrupt
  attachInterrupt(1, ringInterrupt, FALLING);


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


void loop () {
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
  char url[160];
  uint16_t statuscode;
  int16_t length;

  sprintf (url, "http://data.sparkfun.com/input/%s?private_key=%s&latitude=%s&longitude=%s&hdop=%s&altitude=%s",
    SPARKFUN_PUBLIC_KEY, SPARKFUN_PRIVATE_KEY, current_location.latitude_c, current_location.longitude_c, current_location.hdop_c, current_location.altitude_c);

  Serial.print(F("Sending: ")); Serial.println(url);

  // Make the FONA listen, we kinda need that...
  fonaSerial.listen();

  uint8_t rssi = fona.getRSSI();

  if (rssi > 5) {
    // Make an attempt to turn GPRS mode on.  Sometimes the FONA gets freaked out and GPRS doesn't turn off.
    // When this happens you can't turn it on aagin, but you don't need to because it's on.  So don't sweat
    // the error case here -- GPRS could already be on -- just keep on keeping on and let HTTP_GET_start()
    // error if there's a problem with GPRS.
    if (!fona.enableGPRS(true)) {
      Serial.println(F("Failed to turn GPRS on!"));
    }

    if (fona.HTTP_GET_start(url, &statuscode, (uint16_t *)&length)) {
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
      fona.HTTP_GET_end();
    } else {
      Serial.println(F("Failed to send GPRS data!"));
    }

    if (!fona.enableGPRS(false)) {
      Serial.println(F("Failed to turn GPRS off!"));
    }
  } else {
    Serial.println(F("Can't transmit, network signal strength is crap!"));
  }

  // Put the GPS back into listen mode
  gpsSerial.listen();
}


void handleRing () {
  Serial.println(F("Ring ring, Neo."));

  fonaSerial.listen();
  int8_t sms_num = fona.getNumSMS();

  if (sms_num > -1) {
    Serial.print(sms_num); Serial.println(" messages waiting.");

    char sms_buffer[9];
    uint16_t smslen;

    // Read any SMS message we may have...
    for (int8_t sms_index = 1; sms_index <= sms_num; sms_index++) {
      Serial.print(F("  SMS #")); Serial.print(sms_index); Serial.print(F(": "));

      if (fona.readSMS(sms_index, sms_buffer, 8, &smslen)) {
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
          Serial.print(F("  Responding with location..."));

          if (fonaSendLocationSMS(MY_PHONE_NUMBER)) {
            Serial.println(F(" sent."));
          } else {
            Serial.println(F(" failed!"));
          }

          // delete this SMS
          fona.deleteSMS(sms_index);
        } else if (strcmp(sms_buffer, "Status") == 0) {
          Serial.print(F("  Responding with status..."));

          if (fonaSendStatusSMS(MY_PHONE_NUMBER)) {
            Serial.println(F(" sent."));
          } else {
            Serial.println(F(" failed!"));
          }

          // delete this SMS
          fona.deleteSMS(sms_index);
        }
      } else {
        Serial.println(F("Failed to read SMS messages!"));
      }
    }

    gpsSerial.listen();
    ringing = false;
  }
}


boolean fonaSendLocationSMS (char *recipient) {
  char sms_response[52];

  if (current_location.isValid) {
    sprintf (sms_response, "https://maps.google.com?q=%s,%s", current_location.latitude_c, current_location.longitude_c);
  } else {
    sprintf (sms_response, "I'm lost!");
  }

  return fona.sendSMS(recipient, sms_response);
}


boolean fonaSendStatusSMS (char *recipient) {
  uint8_t rssi = fona.getRSSI();
  uint16_t vbat;
  fona.getBattVoltage(&vbat);

  char sms_response[16];
  sprintf (sms_response, "%d bars, %d mV", barsFromRSSI(rssi), vbat);

  return fona.sendSMS(recipient, sms_response);
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
