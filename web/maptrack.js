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
  this.maxLength = opts.maxLength || 0;
  this.circle = circle = new google.maps.Circle({
    map: this.map,
    strokeColor: '#f00',
    strokeWeight: 1,
    fillColor: '#f00',
    fillOpacity: 0.3
  });
};

MapTrack.prototype.setPath = function (points) {
  if (Array.isArray(points)) {
    points.reverse();

    this.clear();

    for (var x = 0; x < points.length; x++) {
      this.addPoint(points[x]);
    }
  }
};

MapTrack.prototype.addPoint = function (point) {
  var location = new google.maps.LatLng(point.latitude, point.longitude);
  var date = new Date(point.timestamp);
  var hdop = point.hdop || 1;

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

  var path = this.polyLine.getPath();
  path.insertAt(0, location);
  this.markers.unshift(marker);

  this.circle.setCenter(location);
  this.circle.setRadius(hdop * 2);

  if (this.maxLength > 0 && path.getLength() > this.maxLength) {
    path.pop();
    this.markers.pop().setMap(null);
  }
};

MapTrack.prototype.clear = function () {
  this.polyLine.getPath().clear();

  while (this.markers.length > 0) {
    this.markers.pop().setMap(null);
  }

  this.circle.setCenter(null);
};

MapTrack.prototype.count = function () {
  return this.polyLine.getPath().getLength();
};

MapTrack.prototype.getBounds = function () {
  var bounds = new google.maps.LatLngBounds();
  this.polyLine.getPath().forEach(function(latLng) {
    bounds.extend(latLng);
  });
  return bounds;
};
