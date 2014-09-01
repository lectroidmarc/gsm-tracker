/**
 *
 */

var map;
var marker;
var track;
var elevator;

const PHANT_URL = 'https://data.sparkfun.com';


function initMap() {
  var map_div = document.getElementById('map');
  map_div.style.height = window.innerHeight - 240 + 'px';

  map = new google.maps.Map(map_div, {
    center: new google.maps.LatLng(38.74, -121.12),
    zoom: 18
  });

  marker = new google.maps.Marker({
    draggable: true,
    map: map
  });

  track = new MapTrack({
    map: map
  });

  elevator = new google.maps.ElevationService();

  google.maps.event.addDomListener(window, 'resize', function () {
    var center = map.getCenter();
    map_div.style.height = window.innerHeight - 240 + 'px';
    google.maps.event.trigger(map, 'resize');
    map.setCenter(center);
  });
}

function initButtons () {
  document.getElementById('send').addEventListener('click', sendMarkerLocation);
  document.getElementById('clear').addEventListener('click', clearData);

  document.querySelector('#data_link a').href = PHANT_URL + '/streams/' + public_key;
}

function initMOWS () {
  if (typeof mows !== 'undefined') {
    var client = mows.createClient('wss://data.sparkfun.com/');
    client.subscribe('output/' + public_key);

    client.on('message', function (topic, data) {
      //console.log(JSON.parse(data));
      updateMap(JSON.parse(data));
    });
  }
}

function initFetch() {
  fetch();
}


function fetch() {
  var params = new Parameters({
    page: 1
  });

  var xhr = new XMLHttpRequest();
  xhr.open('GET', PHANT_URL + '/output/' + public_key + '.json?' + params.serialize());
  xhr.responseType = 'json';
  xhr.onload = function (e) {
    //console.log(this.response);
    if (Array.isArray(this.response)) {
      updateMap(this.response);
    } else {
      resetMap();
    }
  };
  xhr.send();
}

function updateMap (new_map_data) {
  if (map && marker) {
    var marker_latlng = (Array.isArray(new_map_data)) ?
      new google.maps.LatLng(new_map_data[0].latitude, new_map_data[0].longitude) :
      new google.maps.LatLng(new_map_data.latitude, new_map_data.longitude);

    marker.setPosition(marker_latlng);

    if (Array.isArray(new_map_data)) {
      track.setPath(new_map_data);
    } else if (new_map_data) {
      track.addPoint(new_map_data);
    }

    if (track.count() < 3) {
      map.panTo(marker_latlng);
    } else {
      map.fitBounds(track.getBounds());
    }

    getLocationFromLatLng (marker_latlng, function (location) {
      var reported_altitude =  (Array.isArray(new_map_data)) ? new_map_data[0].altitude : new_map_data.altitude;
      var agl = (reported_altitude - location.altitude) * 3.28084;

      var agl_element = document.getElementById('agl');
      if (agl_element) agl_element.innerHTML = agl.toFixed(1) + "'";
    });
  }
}

function resetMap (point) {
  if (point) {
    var center = new google.maps.LatLng(point.latitude, point.longitude);
    marker.setPosition(center);
    map.setCenter(center);
  } else {
    navigator.geolocation.getCurrentPosition(updateGeoPosition, locationError);
  }
  track.clear();
  map.setZoom(18);

  var agl_element = document.getElementById('agl');
  if (agl_element) agl_element.innerHTML = "0'";

  function updateGeoPosition (position) {
    resetMap(position.coords);
  }

  function locationError (positionError) {
    console.warn(positionError);

    getLocationFromLatLng(map.getCenter(), function (location) {
      resetMap(location);
    });
  }
}

function getLocationFromLatLng (latlng, callback) {
  var location = {
    latitude: latlng.lat().toFixed(6),
    longitude: latlng.lng().toFixed(6),
    altitude: 0
  }

  var positionalRequest = {
    'locations': [latlng]
  }

  elevator.getElevationForLocations(positionalRequest, function (results, status) {
    if (status == google.maps.ElevationStatus.OK) {
      if (results[0]) {
        location.altitude = results[0].elevation.toFixed(2);
      } else {
        console.log('No elevation results found');
      }
    } else {
      console.warn('Elevation service failed due to: ' + status);
    }

    callback(location);
  });
}

function sendMarkerLocation () {
  document.getElementById('send').disabled = true;

  getLocationFromLatLng (marker.getPosition(), function (location) {
    var params = new Parameters({
      latitude: location.latitude,
      longitude: location.longitude,
      hdop: 1,
      altitude: location.altitude
    });

    var xhr = new XMLHttpRequest();
    xhr.open('POST', PHANT_URL + '/input/' + public_key + '.json');
    xhr.setRequestHeader('Phant-Private-Key', private_key);
    xhr.responseType = 'json';
    xhr.onload = function (e) {
      //console.log(this.response);
      if (!this.response.success) {
        console.warn(this.response.message);
      }
      document.getElementById('send').disabled = false;
      if (typeof mows === 'undefined') fetch();
    };
    xhr.send(params.formData());
  });
}

function clearData () {
  document.getElementById('clear').disabled = true;

  var xhr = new XMLHttpRequest();
  xhr.open('DELETE', PHANT_URL + '/input/' + public_key + '.json');
  xhr.setRequestHeader('Phant-Private-Key', private_key);
  xhr.responseType = 'json';
  xhr.onload = function (e) {
    //console.log(this.response);
    if (!this.response.success) {
      console.warn(this.response.message);
    }
    document.getElementById('clear').disabled = false;
    resetMap();
  };
  xhr.send();
}

function getStats () {
  var xhr = new XMLHttpRequest();
  xhr.open('GET', PHANT_URL + '/output/' + public_key + '/stats.json');
  xhr.responseType = 'json';
  xhr.onload = function (e) {
    console.log(this.response);
    if (this.status === 200) {
      var use_percent = 100 * (this.response.used / this.response.cap);

      var data_used_div = document.createElement('div');
      data_used_div.style.width = use_percent.toFixed(0) + '%';

      var stats_div = document.getElementById('stats');
      stats_div.style.display = 'block';
      stats_div.title = use_percent.toFixed(0) + '% used';
      stats_div.appendChild(data_used_div);
    } else {
      console.warn(this.response);
    }
  };
  xhr.send();
}
