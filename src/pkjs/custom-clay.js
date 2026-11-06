// Runs inside the Clay config page (serialized by Clay, so it must be
// self-contained: no require, no closures over module scope).
module.exports = function() {
  var clayConfig = this;

  // Must match the defaults in config.js and src/c/settings.c.
  var DEFAULTS = {
    FrameRate: '10', FlameHeight: 5, Resolution: '1', Wind: 0,
    Theme: '0', CustomEmber: 0xFF0000, CustomFlame: 0xFF5500, CustomHot: 0xFFFF00,
    Animate: '0', Flare: false, TiltWind: false, Backlight: true, Flicker: 100,
    Crackle: true, Volume: 3, Stoke: true, HeightSource: '0', WindFollowsWeather: false
  };

  // Same stops as src/c/flames.c, for the gradient preview.
  var THEMES = [
    [[0, 0x000000], [8, 0x550000], [15, 0xAA0000], [23, 0xFF5500], [30, 0xFFAA00], [34, 0xFFFF55], [36, 0xFFFFFF]],
    [[0, 0x000000], [8, 0x550000], [16, 0xAA0000], [26, 0xFF0000], [32, 0xFF5500], [36, 0xFFAA55]],
    [[0, 0x000000], [8, 0x000055], [15, 0x0000AA], [23, 0x0055FF], [30, 0x00AAFF], [34, 0x55FFFF], [36, 0xFFFFFF]],
    [[0, 0x000000], [8, 0x005500], [15, 0x00AA00], [23, 0x55AA00], [30, 0x55FF00], [34, 0xAAFF55], [36, 0xFFFFFF]],
    [[0, 0x000000], [8, 0x550055], [15, 0x5500AA], [23, 0xAA00FF], [30, 0xFF55FF], [34, 0xFFAAFF], [36, 0xFFFFFF]]
  ];

  function item(key) {
    return clayConfig.getItemByMessageKey(key);
  }

  function toNumber(color) {
    if (typeof color === 'number') return color;
    return parseInt(String(color || '0').replace(/^#|^0x/, ''), 16) || 0;
  }

  function hex(color) {
    var s = toNumber(color).toString(16);
    while (s.length < 6) s = '0' + s;
    return '#' + s;
  }

  function stops() {
    var theme = parseInt(item('Theme').get(), 10) || 0;
    if (theme === 5) {
      return [[0, 0x000000], [10, toNumber(item('CustomEmber').get())],
              [22, toNumber(item('CustomFlame').get())], [32, toNumber(item('CustomHot').get())],
              [36, 0xFFFFFF]];
    }
    return THEMES[theme] || THEMES[0];
  }

  function updatePreview() {
    var preview = clayConfig.getItemById('palettePreview');
    if (!preview) return;
    var css = stops().map(function(s) {
      return hex(s[1]) + ' ' + Math.round(s[0] * 100 / 36) + '%';
    }).join(', ');
    preview.set('<div style="height:24px;border-radius:4px;' +
                'background:linear-gradient(to right, ' + css + ')"></div>');
  }

  function updateVisibility() {
    var theme = item('Theme');
    if (theme) {
      var custom = theme.get() === '5';
      ['CustomEmber', 'CustomFlame', 'CustomHot'].forEach(function(key) {
        if (custom) item(key).show(); else item(key).hide();
      });
      updatePreview();
    }
    var source = item('HeightSource');
    if (source) {
      if (source.get() === '0') item('FlameHeight').show(); else item('FlameHeight').hide();
    }
    var backlight = item('Backlight');
    if (backlight) {
      if (backlight.get()) item('Flicker').show(); else item('Flicker').hide();
    }
  }

  clayConfig.on(clayConfig.EVENTS.AFTER_BUILD, function() {
    ['Theme', 'CustomEmber', 'CustomFlame', 'CustomHot', 'Backlight', 'HeightSource'].forEach(function(key) {
      var it = item(key);
      if (it) it.on('change', updateVisibility);
    });
    updateVisibility();

    var reset = clayConfig.getItemById('resetDefaults');
    if (reset) {
      reset.on('click', function() {
        Object.keys(DEFAULTS).forEach(function(key) {
          var it = item(key);
          if (it) it.set(DEFAULTS[key]);
        });
        updateVisibility();
      });
    }
  });

};
