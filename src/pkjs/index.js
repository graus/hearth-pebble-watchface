var Clay = require('@rebble/clay');
var clayConfig = require('./config');
var customClay = require('./custom-clay');

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
