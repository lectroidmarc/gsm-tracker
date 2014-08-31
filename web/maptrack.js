/**
 *
 */

var MapTrack = function (opts) {
  this.map = opts.map;
  this.polyLine = new google.maps.Polyline({
    map: this.map,
    strokeColor: '#999',
    strokeOpacity: 1.0,
    strokeWeight: 1
  });
  this.markers = [];
};

MapTrack.prototype.setPath = function (points) {
  if (Array.isArray(points)) {
    this.clear();

    var path = [];

    for (var x = 0; x < points.length; x++) {
      var point = points[x];
      var location = new google.maps.LatLng(point.latitude, point.longitude);
      var date = new Date(point.timestamp);

      var marker = new google.maps.Marker({
        map: this.map,
        position: location,
        icon: {
          path: google.maps.SymbolPath.CIRCLE,
          scale: 2,
          strokeOpacity: 1
        },
        title: 'at ' + date.toString()
      });

      path.push(location);
      this.markers.push(marker);
    }

    this.polyLine.setPath(path);
  }
};

MapTrack.prototype.addPoint = function (point) {
  var location = new google.maps.LatLng(point.latitude, point.longitude);
  var date = new Date(point.timestamp);

  var marker = new google.maps.Marker({
    map: this.map,
    position: location,
    icon: {
      path: google.maps.SymbolPath.CIRCLE,
      scale: 2,
      strokeOpacity: 1
    },
    title: 'at ' + date.toString()
  });

  this.polyLine.getPath().insertAt(0, location);
  this.markers.unshift(marker);
};

MapTrack.prototype.clear = function () {
  this.polyLine.setPath([]);

  for (var x = 0; x < this.markers.length; x++) {
    this.markers[x].setMap(null);
  }

  this.markers = [];
};

MapTrack.prototype.count = function () {
  return this.polyLine.getPath().length;
};

MapTrack.prototype.getBounds = function () {
  var bounds = new google.maps.LatLngBounds();
  this.polyLine.getPath().forEach(function(latLng) {
    bounds.extend(latLng);
  });
  return bounds;
};
