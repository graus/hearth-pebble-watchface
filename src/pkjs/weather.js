// Local weather for the fire, from Open-Meteo (https://open-meteo.com):
// free, no API key, data under CC BY 4.0. The mapping to the face's units
// happens here so it can change without a watch update.

var URL = 'https://api.open-meteo.com/v1/forecast?current=temperature_2m,wind_speed_10m,wind_direction_10m';

// Colder outside, bigger fire: -10 degC or below gives flame height 10,
// 30 degC or above gives 3, linear in between. Not lower than 3: below that
// the hours fade to scattered embers and the time is hard to read.
function flameHeight(celsius) {
  var h = Math.round(10 - (celsius + 10) * 7 / 40);
  return Math.max(3, Math.min(10, h));
}

// The watch has no compass, so wind is drawn as on a map: the east-west part
// of the wind, blowing right for a wind toward the east. Direction is where
// the wind comes from (meteorological convention), speed in km/h.
// Under 6 km/h of east-west wind is calm; 6-20 is a breeze; above is strong.
function windSetting(speedKmh, fromDegrees) {
  var towardEast = -speedKmh * Math.sin(fromDegrees * Math.PI / 180);
  var a = Math.abs(towardEast);
  var level = a < 6 ? 0 : (a < 20 ? 1 : 2);
  return towardEast < 0 ? -level : level;
}

// Turns an Open-Meteo response into the AppMessage for the watch, or null.
function toMessage(json) {
  var c = json && json.current;
  if (!c || typeof c.temperature_2m !== 'number' || typeof c.wind_speed_10m !== 'number' ||
      typeof c.wind_direction_10m !== 'number') {
    return null;
  }
  return {
    WeatherWind: windSetting(c.wind_speed_10m, c.wind_direction_10m),
    WeatherHeight: flameHeight(c.temperature_2m)
  };
}

function fetchAndSend() {
  navigator.geolocation.getCurrentPosition(function(pos) {
    var req = new XMLHttpRequest();
    req.onload = function() {
      var message = null;
      try {
        message = toMessage(JSON.parse(req.responseText));
      } catch (e) {
        message = null;
      }
      if (!message) {
        console.log('Weather: unexpected response ' + req.status);
        return;
      }
      Pebble.sendAppMessage(message, function() {
        console.log('Weather sent: ' + JSON.stringify(message));
      }, function() {
        console.log('Weather: send failed');
      });
    };
    req.onerror = function() {
      console.log('Weather: request failed');
    };
    // Two decimals (about 1 km) is plenty for weather and shares less.
    req.open('GET', URL + '&latitude=' + pos.coords.latitude.toFixed(2) +
             '&longitude=' + pos.coords.longitude.toFixed(2));
    req.send();
  }, function(err) {
    console.log('Weather: no location (' + (err && err.message) + ')');
  }, { timeout: 15000, maximumAge: 30 * 60 * 1000 });
}

module.exports = {
  flameHeight: flameHeight,
  windSetting: windSetting,
  toMessage: toMessage,
  fetchAndSend: fetchAndSend
};
