GPS Tracker
====

This is Arduino code to support a [Adafruit FONA](https://learn.adafruit.com/adafruit-fona-mini-gsm-gprs-cellular-phone-module/) module and a GPS.

Any serial serial GPS that the [Adafruit GPS library](https://github.com/adafruit/Adafruit-GPS-Library) can deal with will work.  Obviously the [Adafruit Ultimate GPS](https://www.adafruit.com/product/746) works pretty well.

What this code does
----
The upshot here is the Arduino gets the GPS location every `GPS_INTERVAL_SECONDS` seconds and then uses the FONA to update a web data store over a GPRS connection.  In this case the web data store is [data.sparkfun.com](https://data.sparkfun.com/).

For added amusement, the FONA module will also listen for SMS "commands".  Current commands are:

* "Status" - which will return the "bar" rating of the cellular connection.
* "Location" - which will return the module's location.

Visualizing the GPS data
----
Visualizing the saved data is done by opening the `web/index.html` page in a web browser.  It's hardcoded to *data.sparkfun.com* so YMMV.

A note on keys
----
The two files, `keys.h.sample` and `keys.js.sample` need to be copied to versions with the `.sample` suffix removed.  Then the various key data, like your public and private keys for *data.sparkfun.com*, need to be added there.
