#include "audio_midi_settings_ios.h"

#import <Foundation/Foundation.h>

#include <algorithm>
#include <iterator>

namespace {
NSString* const kAudioSettingsChangedNotification = @"AddivoxAudioSettingsChanged";
NSString* const kPreferredSampleRateKey = @"PreferredSampleRate";
NSString* const kPreferredBufferSizeKey = @"PreferredBufferSize";

bool IsValidOption(int value, const int* begin, const int* end) {
  return std::find(begin, end, value) != end;
}

bool IsValidSampleRate(int sampleRate) {
  return IsValidOption(sampleRate, std::begin(addivox_standalone::kIOSAudioSettingsSampleRates), std::end(addivox_standalone::kIOSAudioSettingsSampleRates));
}

bool IsValidBufferSize(int bufferSize) {
  return IsValidOption(bufferSize, std::begin(addivox_standalone::kIOSAudioSettingsBufferSizes), std::end(addivox_standalone::kIOSAudioSettingsBufferSizes));
}

void NotifyAudioSettingsChanged() {
  [[NSNotificationCenter defaultCenter] postNotificationName:kAudioSettingsChangedNotification object:nil];
}
} // namespace

namespace addivox_standalone {

bool IsAudioMidiSettingsMenuAvailable() {
  NSBundle* mainBundle = [NSBundle mainBundle];
  return [mainBundle objectForInfoDictionaryKey:@"NSExtension"] == nil && ![mainBundle.bundleURL.pathExtension isEqualToString:@"appex"];
}

int GetPreferredSampleRate() {
  const int sampleRate = static_cast<int>([[NSUserDefaults standardUserDefaults] integerForKey:kPreferredSampleRateKey]);
  return IsValidSampleRate(sampleRate) ? sampleRate : 48000;
}

int GetPreferredBufferSize() {
  const int bufferSize = static_cast<int>([[NSUserDefaults standardUserDefaults] integerForKey:kPreferredBufferSizeKey]);
  return IsValidBufferSize(bufferSize) ? bufferSize : 128;
}

bool SetPreferredSampleRate(int sampleRate) {
  if (!IsValidSampleRate(sampleRate)) return false;

  [[NSUserDefaults standardUserDefaults] setInteger:sampleRate forKey:kPreferredSampleRateKey];
  [[NSUserDefaults standardUserDefaults] synchronize];
  NotifyAudioSettingsChanged();
  return true;
}

bool SetPreferredBufferSize(int bufferSize) {
  if (!IsValidBufferSize(bufferSize)) return false;

  [[NSUserDefaults standardUserDefaults] setInteger:bufferSize forKey:kPreferredBufferSizeKey];
  [[NSUserDefaults standardUserDefaults] synchronize];
  NotifyAudioSettingsChanged();
  return true;
}

} // namespace addivox_standalone
