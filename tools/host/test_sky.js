// Unit tests for the phone-side sky values: TZ=Europe/Amsterdam node tools/host/test_sky.js
// (The sun checks use local times, so they need that time zone.)
var assert = require('assert');
var sky = require('../../src/pkjs/sky');

assert.strictEqual(sky.flameHeight(-25), 10);
assert.strictEqual(sky.flameHeight(-10), 10);
assert.strictEqual(sky.flameHeight(10), 7);   // 10 - 20 * 7 / 40 = 6.5 -> 7
assert.strictEqual(sky.flameHeight(30), 3);
assert.strictEqual(sky.flameHeight(40), 3);

// Direction is where the wind comes FROM: a west wind (270) blows east (right).
assert.strictEqual(sky.windSetting(30, 270), 2);
assert.strictEqual(sky.windSetting(10, 270), 1);
assert.strictEqual(sky.windSetting(30, 90), -2);   // east wind blows left
assert.strictEqual(sky.windSetting(30, 0), 0);     // north wind: no east-west part
assert.strictEqual(sky.windSetting(3, 270), 0);    // calm

// A response in Open-Meteo's documented shape.
var sample = {
  latitude: 52.37, longitude: 4.89,
  current_units: { temperature_2m: '°C', wind_speed_10m: 'km/h', wind_direction_10m: '°' },
  current: { time: '2026-11-06T18:00', interval: 900, temperature_2m: 6.4,
             wind_speed_10m: 22.3, wind_direction_10m: 236 }
};
// 6.4 degC -> 10 - 16.4 * 7 / 40 = 7.1 -> 7; 22.3 km/h from 236 deg -> 18.5 km/h eastward -> 1.
assert.deepStrictEqual(sky.weatherMessage(sample), { SkyWind: 1, SkyHeight: 7 });
assert.strictEqual(sky.weatherMessage({ error: true, reason: 'limit' }), null);
assert.strictEqual(sky.weatherMessage(null), null);

// Sun times, against published tables (timeanddate.com) for Amsterdam
// (52.37 N, 4.89 E), within 5 minutes.
function near(actual, hh, mm, what) {
  var expected = hh * 60 + mm;
  assert.ok(Math.abs(actual - expected) <= 5, what + ': ' + Math.floor(actual / 60) + ':' +
            ('0' + actual % 60).slice(-2) + ' vs ' + hh + ':' + ('0' + mm).slice(-2));
}
var june = sky.sunTimes(new Date(2026, 5, 21), 52.37, 4.89);
near(june.rise, 5, 18, 'Amsterdam June 21 sunrise');
near(june.set, 22, 6, 'Amsterdam June 21 sunset');
var dec = sky.sunTimes(new Date(2026, 11, 21), 52.37, 4.89);
near(dec.rise, 8, 48, 'Amsterdam December 21 sunrise');
near(dec.set, 16, 29, 'Amsterdam December 21 sunset');
// Tromso (69.65 N): polar night in December, midnight sun in June.
assert.deepStrictEqual(sky.sunTimes(new Date(2026, 11, 21), 69.65, 18.96), { rise: 0, set: 0 });
assert.deepStrictEqual(sky.sunTimes(new Date(2026, 5, 21), 69.65, 18.96), { rise: 0, set: 1440 });
// Reykjavik (64.15 N, 21.94 W) at midsummer: sunrise 02:55, sunset 00:03
// the next day. The sunset must stay after midnight (1443), not be cut to
// 1440, so the flames rise at the real dusk.
process.env.TZ = 'Atlantic/Reykjavik';
var rvk = sky.sunTimes(new Date(2026, 5, 21), 64.15, -21.94);
near(rvk.rise, 2, 55, 'Reykjavik June 21 sunrise');
near(rvk.set, 24, 3, 'Reykjavik June 21 sunset (after midnight)');
assert.ok(rvk.set > 1440, 'sunset after midnight is kept past 1440');
process.env.TZ = 'Europe/Amsterdam';
assert.deepStrictEqual(sky.sunMessage(new Date(2026, 5, 21), 52.37, 4.89),
                       { SunRise: june.rise, SunSet: june.set });
console.log('sky: all tests passed (June ' + june.rise + '-' + june.set + ', December ' +
            dec.rise + '-' + dec.set + ' minutes)');
