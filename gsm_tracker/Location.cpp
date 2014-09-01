
#include "Location.h"

Location::Location () {
  isValid = false;
};

Location::Location (Adafruit_GPS gps) {
  set(gps);
};

void Location::set (Adafruit_GPS gps) {
  isValid = (gps.fix == 0) ? false : true;

  latitude = toDecimal(gps.latitude, gps.lat);
  longitude = toDecimal(gps.longitude, gps.lon);
  altitude = gps.altitude;

  dtostrf(latitude, 8, 6, latitude_c);
  dtostrf(longitude, 8, 6, longitude_c);
  dtostrf(altitude, 4, 2, altitude_c);
};

boolean Location::isEqual(Location location) {
  if (isValid != location.isValid) return false;
  if (abs(latitude - location.latitude) > .001) return false;
  if (abs(longitude - location.longitude) > .001) return false;
  if (abs(altitude - location.altitude) > .1) return false;

  return true;
};

float Location::toDecimal (float nmeaCoord, char direction_letter) {
  uint16_t wholeDegrees = 0.01 * nmeaCoord;
  int direction_modifier = (direction_letter == 'S' || direction_letter == 'W') ? -1 : 1;
  return (wholeDegrees + (nmeaCoord - 100.0 * wholeDegrees) / 60.0) * direction_modifier;
};

