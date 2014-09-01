
#include <Adafruit_GPS.h>

class Location {
  public:
    Location ();
    Location (Adafruit_GPS);

    boolean isValid;

    float latitude;
    float longitude;
    float hdop;
    float altitude;

    char latitude_c[12];
    char longitude_c[12];
    char hdop_c[5];
    char altitude_c[7];

    void set(Adafruit_GPS);
    boolean isEqual(Location);
  private:
    float toDecimal(float, char);
};

