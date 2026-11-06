// Clay configuration page. Sections appear only on watches that support them
// (`capabilities`); every watch gets frame rate and Animate, so every watch
// has a Save button.
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
      { type: 'heading', defaultValue: 'Flames' },
      {
        type: 'select',
        messageKey: 'FrameRate',
        label: 'Frame rate',
        defaultValue: '10',
        description: 'Higher is smoother and uses more battery.',
        options: [
          { label: '5 fps', value: '5' },
          { label: '8 fps', value: '8' },
          { label: '10 fps', value: '10' },
          { label: '15 fps', value: '15' },
          { label: '20 fps', value: '20' }
        ]
      },
      {
        type: 'slider',
        messageKey: 'FlameHeight',
        label: 'Flame height',
        defaultValue: 5,
        min: 1,
        max: 10,
        step: 1
      },
      {
        type: 'select',
        messageKey: 'Resolution',
        label: 'Resolution',
        defaultValue: '1',
        capabilities: ['COLOR'],
        description: 'Finer flames cost more battery.',
        options: [
          { label: 'Fine (1 px)', value: '0' },
          { label: 'Normal (2 px)', value: '1' },
          { label: 'Chunky (4 px)', value: '2' }
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
        capabilities: ['COLOR'],
        description: 'Negative blows left, positive blows right.'
      },
      {
        type: 'select',
        messageKey: 'Turbulence',
        label: 'Turbulence',
        defaultValue: '1',
        capabilities: ['COLOR'],
        options: [
          { label: 'Calm', value: '0' },
          { label: 'Normal', value: '1' },
          { label: 'Wild', value: '2' }
        ]
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
      { type: 'heading', defaultValue: 'Behaviour' },
      {
        type: 'radiogroup',
        messageKey: 'Animate',
        label: 'Animate',
        defaultValue: '0',
        description: 'On tap freezes the flames 30 seconds after you tap or shake the watch. It saves the most battery.',
        options: [
          { label: 'Always', value: '0' },
          { label: 'On tap', value: '1' }
        ]
      },
      {
        type: 'toggle',
        messageKey: 'Flare',
        label: 'Flare on the minute',
        defaultValue: false
      },
      {
        type: 'toggle',
        messageKey: 'TiltWind',
        label: 'Wind follows tilt',
        defaultValue: false,
        capabilities: ['COLOR']
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
        defaultValue: true,
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
        description: 'Silent when the speaker is muted or Quiet Time is on.'
      },
      {
        type: 'slider',
        messageKey: 'Volume',
        label: 'Crackle volume',
        defaultValue: 3,
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
