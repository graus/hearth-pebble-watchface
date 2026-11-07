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

// The watch asks for what its own settings need (bit 1: sun times, bit 2:
// weather): after a settings change, shortly after starting, and every half
// hour. Going by the watch rather than this phone's copy of the settings
// keeps working on a new phone or after the app's data is cleared.
// Requests that arrive close together (at launch and after a settings
// change) are merged into one location lookup and one round of messages.
var pendingNeeds = 0;
Pebble.addEventListener('appmessage', function(e) {
  var needs = e.payload && e.payload.SkyRequest;
  if (typeof needs !== 'number' || needs <= 0) return;
  if (!pendingNeeds) {
    setTimeout(function() {
      var n = pendingNeeds;
      pendingNeeds = 0;
      sky.update((n & 1) !== 0, (n & 2) !== 0);
    }, 1000);
  }
  pendingNeeds |= needs;
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
  }, function(err) {
    console.log('Settings failed: ' + JSON.stringify(err));
  });
});
