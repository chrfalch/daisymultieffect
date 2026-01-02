require 'json'

Pod::Spec.new do |s|
  s.name           = 'DaisyMultiFX'
  s.version        = '1.0.0'
  s.summary        = 'Expo module for DaisyMultiFX MIDI control'
  s.description    = 'Native module for controlling DaisyMultiFX effects via MIDI'
  s.author         = 'Christian Falch'
  s.homepage       = 'https://github.com/chrfalch/daisyseed-multi-effect'
  s.platforms      = { :ios => '15.1' }
  s.source         = { git: '' }
  s.static_framework = true

  s.dependency 'ExpoModulesCore'

  s.source_files = '*.{h,m,mm,swift}'
  s.frameworks = 'CoreMIDI'

  s.swift_version = '5.4'
end
