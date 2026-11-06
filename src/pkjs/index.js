var Clay = require('@rebble/clay');
var clayConfig = require('./config');
var customClay = require('./custom-clay');
var weather = require('./weather');

// Settings are converted here rather than by Clay so the watch always gets
// one int32 per key, whatever type the component produced (select values are
// strings, toggles booleans, colors numbers or hex strings).
var clay = new Clay(clayConfig, customClay, { autoHandleEvents: false });

var COLOR_KEYS = { CustomEmber: true, CustomFlame: true, CustomHot: true };

function toInt(key, value) {
  if (typeof value === 'boolean') return value ? 1 : 0;
  if (COLOR_KEYS[key]) {
    if (typeof value === 'number') return value;
    return parseInt(String(value).replace(/^#|^0x/, ''), 16) || 0;
  }
  return parseInt(value, 10) || 0;
}

// Clay keeps the last saved values in localStorage.
function weatherEnabled() {
  try {
    var saved = JSON.parse(localStorage.getItem('clay-settings')) || {};
    return !!saved.Weather;
  } catch (e) {
    return false;
  }
}

Pebble.addEventListener('ready', function() {
  if (weatherEnabled()) weather.fetchAndSend();
});

// The watch asks every half hour while the Weather setting is on.
Pebble.addEventListener('appmessage', function(e) {
  if (e.payload && e.payload.WeatherRequest !== undefined && weatherEnabled()) {
    weather.fetchAndSend();
  }
});

Pebble.addEventListener('showConfiguration', function() {
  Pebble.openURL(clay.generateUrl());
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (!e || !e.response) return;
  var raw = clay.getSettings(e.response, false);
  var message = {};
  Object.keys(raw).forEach(function(key) {
    var entry = raw[key];
    var value = entry !== null && typeof entry === 'object' ? entry.value : entry;
    if (value === undefined || value === null || value === '') return;
    message[key] = toInt(key, value);
  });
  Pebble.sendAppMessage(message, function() {
    console.log('Settings sent');
    if (message.Weather) weather.fetchAndSend();
  }, function(err) {
    console.log('Settings failed: ' + JSON.stringify(err));
  });
});
