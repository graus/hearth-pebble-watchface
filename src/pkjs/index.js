var Clay = require('@rebble/clay');
var clayConfig = require('./config');
var customClay = require('./custom-clay');
var sky = require('./sky');

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

// Clay keeps the last saved values in localStorage. Height source:
// 0 manual, 1 outside temperature, 2 sun.
function saved() {
  try {
    return JSON.parse(localStorage.getItem('clay-settings')) || {};
  } catch (e) {
    return {};
  }
}

function updateSky(settings) {
  var source = parseInt(settings.HeightSource, 10) || 0;
  sky.update(source === 2, source === 1 || !!settings.WindFollowsWeather);
}

Pebble.addEventListener('ready', function() {
  updateSky(saved());
});

// The watch asks every half hour while a setting uses the sky.
Pebble.addEventListener('appmessage', function(e) {
  if (e.payload && e.payload.SkyRequest !== undefined) {
    updateSky(saved());
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
    updateSky(saved());
  }, function(err) {
    console.log('Settings failed: ' + JSON.stringify(err));
  });
});
