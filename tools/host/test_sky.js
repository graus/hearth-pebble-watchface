// Unit tests for the phone-side weather mapping: node tools/host/test_weather.js
var assert = require('assert');
var w = require('../../src/pkjs/weather');

assert.strictEqual(w.flameHeight(-25), 10);
assert.strictEqual(w.flameHeight(-10), 10);
assert.strictEqual(w.flameHeight(10), 7);   // 10 - 20 * 7 / 40 = 6.5 -> 7
assert.strictEqual(w.flameHeight(30), 3);
assert.strictEqual(w.flameHeight(40), 3);

// Direction is where the wind comes FROM: a west wind (270) blows east (right).
assert.strictEqual(w.windSetting(30, 270), 2);
assert.strictEqual(w.windSetting(10, 270), 1);
assert.strictEqual(w.windSetting(30, 90), -2);   // east wind blows left
assert.strictEqual(w.windSetting(30, 0), 0);     // north wind: no east-west part
assert.strictEqual(w.windSetting(3, 270), 0);    // calm

// A response in Open-Meteo's documented shape.
var sample = {
  latitude: 52.37, longitude: 4.89,
  current_units: { temperature_2m: '°C', wind_speed_10m: 'km/h', wind_direction_10m: '°' },
  current: { time: '2026-11-06T18:00', interval: 900, temperature_2m: 6.4,
             wind_speed_10m: 22.3, wind_direction_10m: 236 }
};
// 6.4 degC -> 10 - 16.4 * 7 / 40 = 7.1 -> 7; 22.3 km/h from 236 deg -> 18.5 km/h eastward -> 1.
assert.deepStrictEqual(w.toMessage(sample), { WeatherWind: 1, WeatherHeight: 7 });
assert.strictEqual(w.toMessage({ error: true, reason: 'limit' }), null);
assert.strictEqual(w.toMessage(null), null);
console.log('weather mapping: all tests passed');
