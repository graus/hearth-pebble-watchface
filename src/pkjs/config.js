// Clay configuration page. Sections appear only on watches that support them
// (`capabilities`); every watch gets Flame height and Animation, so every
// watch has a Save button.
module.exports = [
  {
    type: 'heading',
    defaultValue: 'Hearth'
  },
  {
    type: 'text',
    defaultValue: 'The time, burning.'
  },
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Flame height' },
      {
        type: 'text',
        defaultValue: 'How far the flames climb up the digits. Low flames leave the top digits glowing dark red; high flames fill them with fire.'
      },
      {
        type: 'select',
        messageKey: 'HeightSource',
        label: 'Set by',
        defaultValue: '0',
        description: 'Outside temperature: the colder it is, the higher the flames. Time of day: low by day, high at night, rising at sunset and falling at sunrise.',
        options: [
          { label: 'Me (slider below)', value: '0' },
          { label: 'Outside temperature', value: '1' },
          { label: 'Time of day', value: '2' }
        ]
      },
      {
        type: 'slider',
        messageKey: 'FlameHeight',
        label: 'Flame height',
        description: '1: low, glowing embers. 10: flames reach the top of the digits.',
        defaultValue: 5,
        min: 1,
        max: 10,
        step: 1
      }
    ]
  },
  {
    type: 'section',
    capabilities: ['COLOR'],
    items: [
      { type: 'heading', defaultValue: 'Wind' },
      {
        type: 'text',
        defaultValue: 'Wind makes the flames lean sideways, as if a draft blows through the fire. It does not change how high they burn.'
      },
      {
        type: 'select',
        messageKey: 'WindFollowsWeather',
        label: 'Set by',
        defaultValue: '0',
        description: 'Local wind: the flames lean with the wind outside, as on a map: wind blowing east leans them right.',
        options: [
          { label: 'Me (slider below)', value: '0' },
          { label: 'Local wind', value: '1' }
        ]
      },
      {
        type: 'slider',
        messageKey: 'Wind',
        label: 'Wind',
        defaultValue: 0,
        min: -2,
        max: 2,
        step: 1,
        description: 'Below 0 the flames lean left, above 0 right; 2 is a strong wind. At 0 they lean just slightly left, as in the original Doom fire.'
      },
      {
        type: 'toggle',
        messageKey: 'TiltWind',
        label: 'Tilt the watch to blow',
        defaultValue: false,
        description: 'Tilting your wrist left or right adds wind that way, on top of the wind above.'
      }
    ]
  },
  {
    type: 'section',
    capabilities: ['COLOR'],
    items: [
      { type: 'heading', defaultValue: 'Colors' },
      {
        type: 'select',
        messageKey: 'Theme',
        label: 'Color theme',
        defaultValue: '0',
        options: [
          { label: 'Classic', value: '0' },
          { label: 'Ember', value: '1' },
          { label: 'Blue', value: '2' },
          { label: 'Green', value: '3' },
          { label: 'Purple', value: '4' },
          { label: 'Custom', value: '5' }
        ]
      },
      { type: 'color', messageKey: 'CustomEmber', label: 'Embers', defaultValue: 'FF0000', sunlight: false },
      { type: 'color', messageKey: 'CustomFlame', label: 'Flames', defaultValue: 'FF5500', sunlight: false },
      { type: 'color', messageKey: 'CustomHot', label: 'Hottest', defaultValue: 'FFFF00', sunlight: false },
      { type: 'text', id: 'palettePreview', defaultValue: '' }
    ]
  },
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Animation' },
      {
        type: 'radiogroup',
        messageKey: 'Animate',
        label: 'Flames move',
        defaultValue: '0',
        description: '"After a tap" freezes the fire 30 seconds after the last tap, and a tap brings it back to life. A tap is a touch on the screen (Pebble Time 2, Round 2) or a sharp flick of the wrist. It saves by far the most battery.',
        options: [
          { label: 'Always', value: '0' },
          { label: 'For 30 seconds after a tap', value: '1' }
        ]
      },
      {
        type: 'toggle',
        messageKey: 'Stoke',
        label: 'Tap to stoke',
        defaultValue: true,
        description: 'Touch the screen (Pebble Time 2, Round 2) or flick your wrist sharply, and the flames leap up, then settle back over a few seconds.'
      },
      {
        type: 'toggle',
        messageKey: 'Flare',
        label: 'Flare at each new minute',
        defaultValue: false,
        description: 'When the minute changes, the flames leap up for a moment, so each new time arrives with a burst of fire.'
      },
      {
        type: 'select',
        messageKey: 'FrameRate',
        label: 'Frame rate',
        defaultValue: '10',
        description: 'How many times a second the fire moves. Higher is smoother and uses more battery.',
        options: [
          { label: '5 fps', value: '5' },
          { label: '8 fps', value: '8' },
          { label: '10 fps', value: '10' },
          { label: '15 fps', value: '15' },
          { label: '20 fps', value: '20' }
        ]
      },
      {
        type: 'select',
        messageKey: 'Resolution',
        label: 'Detail',
        defaultValue: '1',
        capabilities: ['COLOR'],
        description: 'The size of each speck of fire. Fine looks smoothest and uses a little more battery.',
        options: [
          { label: 'Fine (1 px)', value: '0' },
          { label: 'Normal (2 px)', value: '1' },
          { label: 'Chunky (4 px)', value: '2' }
        ]
      }
    ]
  },
  {
    type: 'section',
    items: [
      {
        type: 'text',
        defaultValue: 'Weather and the time of day use your location every 30 minutes, only while one of them is chosen above. Sunrise and sunset are worked out on your phone; weather data by <a href="https://open-meteo.com/">Open-Meteo.com</a> (CC BY 4.0).'
      }
    ]
  },
  {
    type: 'section',
    capabilities: ['PLATFORM_EMERY'],
    items: [
      { type: 'heading', defaultValue: 'Backlight' },
      {
        type: 'toggle',
        messageKey: 'Backlight',
        label: 'Flickering backlight',
        defaultValue: false,
        description: 'Keeps the light on while the flames move. Overrides the backlight timeout and ambient sensor, and drains the battery.'
      },
      {
        type: 'slider',
        messageKey: 'Flicker',
        label: 'Flicker amount (%)',
        defaultValue: 100,
        min: 0,
        max: 100,
        step: 10
      }
    ]
  },
  {
    type: 'section',
    capabilities: ['SPEAKER'],
    items: [
      { type: 'heading', defaultValue: 'Sound' },
      {
        type: 'toggle',
        messageKey: 'Crackle',
        label: 'Crackling sound',
        defaultValue: true,
        description: 'Silent while the watch speaker is muted (on the watch: Settings, Sounds) or Quiet Time is on.'
      },
      {
        type: 'slider',
        messageKey: 'Volume',
        label: 'Crackle volume',
        description: 'Saving a new volume plays one crackle so you can hear it.',
        defaultValue: 5,
        min: 1,
        max: 10,
        step: 1
      }
    ]
  },
  {
    type: 'button',
    id: 'resetDefaults',
    defaultValue: 'Reset to defaults'
  },
  {
    type: 'submit',
    defaultValue: 'Save'
  }
];
