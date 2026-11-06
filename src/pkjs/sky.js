// What the sky outside says about the fire, worked out on the phone and sent
// to the watch in the face's own units:
//   - weather from Open-Meteo (https://open-meteo.com): free, no API key,
//     data under CC BY 4.0;
//   - sunrise and sunset, computed here from the location (no network).

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
function weatherMessage(json) {
  var c = json && json.current;
  if (!c || typeof c.temperature_2m !== 'number' || typeof c.wind_speed_10m !== 'number' ||
      typeof c.wind_direction_10m !== 'number') {
    return null;
  }
  return {
    SkyWind: windSetting(c.wind_speed_10m, c.wind_direction_10m),
    SkyHeight: flameHeight(c.temperature_2m)
  };
}

// Sunrise and sunset for the local day of `date`, in minutes after local
// midnight, from the sunrise equation (as used by NOAA's calculator;
// accurate to a few minutes). Polar night gives { rise: 0, set: 0 }, polar
// day { rise: 0, set: 1440 }. A sunset after midnight counts as midnight.
function sunTimes(date, latitude, longitude) {
  var rad = Math.PI / 180;
  var noon = new Date(date.getFullYear(), date.getMonth(), date.getDate(), 12);
  var julianNoon = noon.getTime() / 86400000 + 2440587.5;
  var n = Math.round(julianNoon - 2451545.0 + 0.0008 - longitude / 360);
  var meanNoon = n - longitude / 360;
  var m = (357.5291 + 0.98560028 * meanNoon) % 360;
  var c = 1.9148 * Math.sin(m * rad) + 0.02 * Math.sin(2 * m * rad) + 0.0003 * Math.sin(3 * m * rad);
  var lambda = (m + c + 180 + 102.9372) % 360;
  var transit = 2451545.0 + meanNoon + 0.0053 * Math.sin(m * rad) - 0.0069 * Math.sin(2 * lambda * rad);
  var sinDecl = Math.sin(lambda * rad) * Math.sin(23.4397 * rad);
  var cosDecl = Math.cos(Math.asin(sinDecl));
  var cosHour = (Math.sin(-0.833 * rad) - Math.sin(latitude * rad) * sinDecl) /
                (Math.cos(latitude * rad) * cosDecl);
  if (cosHour > 1) return { rise: 0, set: 0 };
  if (cosHour < -1) return { rise: 0, set: 1440 };
  var hour = Math.acos(cosHour) / rad / 360;
  function localMinutes(julian) {
    var d = new Date((julian - 2440587.5) * 86400000);
    var dayDiff = Math.round((new Date(d.getFullYear(), d.getMonth(), d.getDate()) -
                              new Date(date.getFullYear(), date.getMonth(), date.getDate())) / 86400000);
    return dayDiff * 1440 + d.getHours() * 60 + d.getMinutes();
  }
  var rise = Math.max(0, localMinutes(transit - hour));
  var set = Math.min(1440, localMinutes(transit + hour));
  return { rise: rise, set: set };
}

function sunMessage(date, latitude, longitude) {
  var t = sunTimes(date, latitude, longitude);
  return { SunRise: t.rise, SunSet: t.set };
}

function send(message, what) {
  Pebble.sendAppMessage(message, function() {
    console.log(what + ' sent: ' + JSON.stringify(message));
  }, function() {
    console.log(what + ': send failed');
  });
}

// Fetches what the settings need: sun times (no network) and/or weather.
function update(needSun, needWeather) {
  if (!needSun && !needWeather) return;
  navigator.geolocation.getCurrentPosition(function(pos) {
    var lat = pos.coords.latitude, lon = pos.coords.longitude;
    if (needSun) send(sunMessage(new Date(), lat, lon), 'Sun');
    if (!needWeather) return;
    var req = new XMLHttpRequest();
    req.onload = function() {
      var message = null;
      try {
        message = weatherMessage(JSON.parse(req.responseText));
      } catch (e) {
        message = null;
      }
      if (message) {
        send(message, 'Weather');
      } else {
        console.log('Weather: unexpected response ' + req.status);
      }
    };
    req.onerror = function() {
      console.log('Weather: request failed');
    };
    // Two decimals (about 1 km) is plenty for weather and shares less.
    req.open('GET', URL + '&latitude=' + lat.toFixed(2) + '&longitude=' + lon.toFixed(2));
    req.send();
  }, function(err) {
    console.log('Sky: no location (' + (err && err.message) + ')');
  }, { timeout: 15000, maximumAge: 30 * 60 * 1000 });
}

module.exports = {
  flameHeight: flameHeight,
  windSetting: windSetting,
  weatherMessage: weatherMessage,
  sunTimes: sunTimes,
  sunMessage: sunMessage,
  update: update
};
