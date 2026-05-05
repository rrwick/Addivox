/*
 ==============================================================================

 This file is based on iPlug2's AUv3 iOS standalone player, with CoreMIDI input
 routing added for Addivox's standalone iPad app.

 ==============================================================================
*/

#import "IPlugAUPlayer.h"

#import <CoreMIDI/CoreMIDI.h>
#import <UIKit/UIKit.h>

#include "audio_midi_settings_ios.h"
#include "config.h"
#include "../settings/params.h"

#include <cmath>

#if !__has_feature(objc_arc)
#error This file must be compiled with Arc. Use -fobjc-arc flag
#endif

static bool isInstrument()
{
#if PLUG_TYPE == 1
  return YES;
#else
  return NO;
#endif
}

@interface IPlugAUPlayer ()
- (void)applyStandaloneParameterDefaults;
- (void)connectMIDISources;
- (void)finishAudioUnitInstantiationWithError:(NSError*)error completion:(void (^)(void))completionBlock;
- (void)handleMIDIPacketList:(const MIDIPacketList*)packetList;
- (BOOL)restoreStandaloneState;
- (BOOL)saveStandaloneState;
- (void)syncParameterTreeToRestoredState;
- (void)onAudioSettingsChanged:(NSNotification*)notification;
- (void)onAppStateShouldSave:(NSNotification*)notification;
@end

namespace {
NSString* const kAudioSettingsChangedNotification = @"AddivoxAudioSettingsChanged";
NSString* const kStandaloneAUStateKey = @"auState";
NSString* const kStandaloneStateFileName = @"ios_standalone_state.plist";
constexpr AUValue kPluginDefaultReverb = 0.f;
constexpr AUValue kIOSStandaloneDefaultReverb = 50.f;
}

static void MIDIReadCallback(const MIDIPacketList* packetList, void* readProcRefCon, void* srcConnRefCon)
{
  IPlugAUPlayer* player = (__bridge IPlugAUPlayer*) readProcRefCon;
  [player handleMIDIPacketList:packetList];
}

static void MIDIStateChangedCallback(const MIDINotification* message, void* refCon)
{
  IPlugAUPlayer* player = (__bridge IPlugAUPlayer*) refCon;
  dispatch_async(dispatch_get_main_queue(), ^{
    [player connectMIDISources];
  });
}

@implementation IPlugAUPlayer
{
  AVAudioEngine* engine;
  AVAudioUnit* avAudioUnit;
  UInt32 componentType;
  MIDIClientRef midiClient;
  MIDIPortRef midiInputPort;
  NSMutableArray<NSNumber*>* connectedMIDISources;
}

- (instancetype)initWithComponentType:(UInt32)unitComponentType
{
  self = [super init];

  if (self)
  {
    engine = [[AVAudioEngine alloc] init];
    componentType = unitComponentType;
    midiClient = 0;
    midiInputPort = 0;
    connectedMIDISources = [[NSMutableArray alloc] init];
  }

  return self;
}

- (void)loadAudioUnitWithComponentDescription:(AudioComponentDescription)desc completion:(void (^)(void))completionBlock
{
  [AVAudioUnit instantiateWithComponentDescription:desc
                                          options:0
                                completionHandler:^(AVAudioUnit* __nullable audioUnit, NSError* __nullable error) {
                                  [self onAudioUnitInstantiated:audioUnit error:error completion:completionBlock];
                                }];
}

- (void)onAudioUnitInstantiated:(AVAudioUnit* __nullable)audioUnit error:(NSError* __nullable)error completion:(void (^)(void))completionBlock
{
  if (audioUnit == nil)
    return;

  avAudioUnit = audioUnit;

  [engine attachNode:avAudioUnit];

  self.currentAudioUnit = avAudioUnit.AUAudioUnit;

  dispatch_async(dispatch_get_main_queue(), ^{
    [self finishAudioUnitInstantiationWithError:error completion:completionBlock];
  });
}

- (void)finishAudioUnitInstantiationWithError:(NSError*)error completion:(void (^)(void))completionBlock
{
  const BOOL restoredState = [self restoreStandaloneState];
  if (!restoredState)
    [self applyStandaloneParameterDefaults];
  [self startMIDIInput];

  [self setupSession];

#ifdef _DEBUG
  [self printEngineInfo];
  [self printSessionInfo];
#endif

  [self makeEngineConnections];
  [self addNotifications];

  AVAudioSession* session = [AVAudioSession sharedInstance];

  if (![session setActive:TRUE error:&error])
  {
    NSLog(@"Error setting session active: %@", [error localizedDescription]);
  }

  if (![engine startAndReturnError:&error])
  {
    NSLog(@"engine failed to start: %@", error);
  }

  completionBlock();
}

- (void)applyStandaloneParameterDefaults
{
  AUParameterTree* parameterTree = self.currentAudioUnit.parameterTree;
  if (parameterTree == nil)
    return;

  AUParameter* reverbParameter = [parameterTree parameterWithAddress:kParamEffectsReverb];
  if (reverbParameter == nil)
    return;

  if (std::fabs(reverbParameter.value - kPluginDefaultReverb) <= 0.001f)
    [reverbParameter setValue:kIOSStandaloneDefaultReverb originator:nil];
}

- (void)dealloc
{
  [self saveStandaloneState];
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [self stopMIDIInput];
}

- (NSURL*)standaloneStateURLCreatingDirectory:(BOOL)createDirectory
{
  NSFileManager* fileManager = NSFileManager.defaultManager;
  NSError* error = nil;
  NSURL* appSupportURL = [fileManager URLForDirectory:NSApplicationSupportDirectory
                                             inDomain:NSUserDomainMask
                                    appropriateForURL:nil
                                               create:createDirectory
                                                error:&error];
  if (appSupportURL == nil)
  {
    if (error != nil)
      NSLog(@"Could not locate Application Support directory: %@", error.localizedDescription);
    return nil;
  }

  NSString* bundleName = [NSString stringWithUTF8String:BUNDLE_NAME];
  NSURL* stateDirectoryURL = [appSupportURL URLByAppendingPathComponent:bundleName isDirectory:YES];
  if (createDirectory && ![fileManager createDirectoryAtURL:stateDirectoryURL withIntermediateDirectories:YES attributes:nil error:&error])
  {
    NSLog(@"Could not create Addivox state directory: %@", error.localizedDescription);
    return nil;
  }

  return [stateDirectoryURL URLByAppendingPathComponent:kStandaloneStateFileName isDirectory:NO];
}

- (BOOL)restoreStandaloneState
{
  if (self.currentAudioUnit == nil)
    return NO;

  NSURL* stateURL = [self standaloneStateURLCreatingDirectory:NO];
  if (stateURL == nil)
    return NO;

  NSError* error = nil;
  NSData* stateData = [NSData dataWithContentsOfURL:stateURL options:0 error:&error];
  if (stateData == nil)
    return NO;

  id propertyList = [NSPropertyListSerialization propertyListWithData:stateData options:NSPropertyListImmutable format:nil error:&error];
  if (![propertyList isKindOfClass:NSDictionary.class])
  {
    if (error != nil)
      NSLog(@"Could not read Addivox standalone state: %@", error.localizedDescription);
    return NO;
  }

  NSDictionary<NSString*, id>* storedState = (NSDictionary<NSString*, id>*)propertyList;
  NSDictionary<NSString*, id>* auState = storedState[kStandaloneAUStateKey];
  if (![auState isKindOfClass:NSDictionary.class])
    auState = storedState; // Legacy state file from earlier iOS persistence builds.

  self.currentAudioUnit.fullState = auState;
  [self syncParameterTreeToRestoredState];
  return YES;
}

- (BOOL)saveStandaloneState
{
  if (self.currentAudioUnit == nil)
    return NO;

  NSDictionary<NSString*, id>* state = self.currentAudioUnit.fullState;
  if (state.count == 0)
    return NO;

  NSMutableDictionary<NSString*, id>* storedState = [[NSMutableDictionary alloc] init];
  storedState[kStandaloneAUStateKey] = state;

  NSError* error = nil;
  NSData* stateData = [NSPropertyListSerialization dataWithPropertyList:storedState format:NSPropertyListBinaryFormat_v1_0 options:0 error:&error];
  if (stateData == nil)
  {
    NSLog(@"Could not serialize Addivox standalone state: %@", error.localizedDescription);
    return NO;
  }

  NSURL* stateURL = [self standaloneStateURLCreatingDirectory:YES];
  if (stateURL == nil)
    return NO;

  if (![stateData writeToURL:stateURL options:NSDataWritingAtomic error:&error])
  {
    NSLog(@"Could not save Addivox standalone state: %@", error.localizedDescription);
    return NO;
  }

  return YES;
}

- (void)syncParameterTreeToRestoredState
{
  AUParameterTree* parameterTree = self.currentAudioUnit.parameterTree;
  if (parameterTree == nil)
    return;

  for (AUParameter* parameter in parameterTree.allParameters)
  {
    AUValue restoredValue = parameter.value;
    if (parameterTree.implementorValueProvider != nil)
      restoredValue = parameterTree.implementorValueProvider(parameter);

    [parameter setValue:restoredValue originator:nil];
  }
}

- (void)startMIDIInput
{
  if (midiClient != 0 || midiInputPort != 0)
    return;

  OSStatus result = MIDIClientCreate(CFSTR("Addivox MIDI Client"), MIDIStateChangedCallback, (__bridge void*) self, &midiClient);
  if (result != noErr)
  {
    NSLog(@"Could not create MIDI client: %d", static_cast<int>(result));
    midiClient = 0;
    return;
  }

  result = MIDIInputPortCreate(midiClient, CFSTR("Addivox MIDI Input"), MIDIReadCallback, (__bridge void*) self, &midiInputPort);
  if (result != noErr)
  {
    NSLog(@"Could not create MIDI input port: %d", static_cast<int>(result));
    MIDIClientDispose(midiClient);
    midiClient = 0;
    midiInputPort = 0;
    return;
  }

  [self connectMIDISources];
}

- (void)stopMIDIInput
{
  for (NSNumber* sourceNumber in connectedMIDISources)
  {
    MIDIPortDisconnectSource(midiInputPort, static_cast<MIDIEndpointRef>(sourceNumber.unsignedIntValue));
  }
  [connectedMIDISources removeAllObjects];

  if (midiInputPort != 0)
  {
    MIDIPortDispose(midiInputPort);
    midiInputPort = 0;
  }

  if (midiClient != 0)
  {
    MIDIClientDispose(midiClient);
    midiClient = 0;
  }
}

- (void)connectMIDISources
{
  if (midiInputPort == 0)
    return;

  for (NSNumber* sourceNumber in connectedMIDISources)
  {
    MIDIPortDisconnectSource(midiInputPort, static_cast<MIDIEndpointRef>(sourceNumber.unsignedIntValue));
  }
  [connectedMIDISources removeAllObjects];

  const ItemCount numSources = MIDIGetNumberOfSources();
  for (ItemCount i = 0; i < numSources; ++i)
  {
    MIDIEndpointRef source = MIDIGetSource(i);
    if (source == 0)
      continue;

    OSStatus result = MIDIPortConnectSource(midiInputPort, source, nullptr);
    if (result == noErr)
    {
      [connectedMIDISources addObject:@(source)];
      CFStringRef sourceName = nullptr;
      if (MIDIObjectGetStringProperty(source, kMIDIPropertyDisplayName, &sourceName) == noErr && sourceName != nullptr)
      {
        NSLog(@"Connected MIDI source: %@", (__bridge NSString*) sourceName);
        CFRelease(sourceName);
      }
    }
    else
    {
      NSLog(@"Could not connect MIDI source %u: %d", static_cast<unsigned>(source), static_cast<int>(result));
    }
  }
}

- (void)scheduleMIDIMessage:(const UInt8*)message length:(UInt8)length
{
  if (self.currentAudioUnit == nil || length == 0)
    return;

  AUScheduleMIDIEventBlock scheduleMIDIEventBlock = self.currentAudioUnit.scheduleMIDIEventBlock;
  if (scheduleMIDIEventBlock == nil)
    return;

  scheduleMIDIEventBlock(AUEventSampleTimeImmediate, 0, length, message);
}

- (void)handleMIDIBytes:(const UInt8*)data length:(UInt16)length
{
  UInt8 runningStatus = 0;
  UInt16 position = 0;

  while (position < length)
  {
    UInt8 status = data[position];

    if ((status & 0x80) != 0)
    {
      ++position;
      if (status >= 0xF0)
        continue;
      runningStatus = status;
    }
    else
    {
      status = runningStatus;
      if (status == 0)
      {
        ++position;
        continue;
      }
    }

    const UInt8 statusType = status & 0xF0;
    const UInt8 dataLength = (statusType == 0xC0 || statusType == 0xD0) ? 1 : 2;
    if (position + dataLength > length)
      break;

    UInt8 message[3] = {status, data[position], static_cast<UInt8>(dataLength == 2 ? data[position + 1] : 0)};
    [self scheduleMIDIMessage:message length:static_cast<UInt8>(dataLength + 1)];
    position += dataLength;
  }
}

- (void)handleMIDIPacketList:(const MIDIPacketList*)packetList
{
  const MIDIPacket* packet = &packetList->packet[0];
  for (UInt32 i = 0; i < packetList->numPackets; ++i)
  {
    [self handleMIDIBytes:packet->data length:packet->length];
    packet = MIDIPacketNext(packet);
  }
}

- (void)restartAudioEngine
{
  [engine stop];

  NSError* error = nil;

  if (![engine startAndReturnError:&error])
  {
    NSLog(@"Error re-starting audio engine: %@", error);
  }
  else
  {
    [self printSessionInfo];
  }
}

- (void)setupSession
{
  AVAudioSession* session = [AVAudioSession sharedInstance];

  NSError* error = nil;
  const AVAudioSessionCategory category = isInstrument() ? AVAudioSessionCategoryPlayback : AVAudioSessionCategoryPlayAndRecord;
  const AVAudioSessionCategoryOptions options = isInstrument() ? 0 : (AVAudioSessionCategoryOptionDefaultToSpeaker | AVAudioSessionCategoryOptionAllowBluetoothHFP);
  if (![session setCategory:category withOptions:options error:&error])
  {
    NSLog(@"Error setting category: %@", error);
  }

  const int preferredSampleRate = addivox_standalone::GetPreferredSampleRate();
  error = nil;
  if (![session setPreferredSampleRate:preferredSampleRate error:&error])
  {
    NSLog(@"Error setting samplerate: %@", error);
  }

  const int preferredBufferSize = addivox_standalone::GetPreferredBufferSize();
  error = nil;
  if (![session setPreferredIOBufferDuration:static_cast<double>(preferredBufferSize) / static_cast<double>(preferredSampleRate) error:&error])
  {
    NSLog(@"Error setting io buffer duration: %@", error);
  }
}

- (void)makeEngineConnections
{
  if (!isInstrument())
  {
    AVAudioNode* inputNode = [engine inputNode];
    AVAudioFormat* inputNodeFormat = [inputNode inputFormatForBus:0];

    @autoreleasepool
    {
      @try
      {
        [engine connect:inputNode to:avAudioUnit format:inputNodeFormat];
      }
      @catch (NSException* exception)
      {
        NSLog(@"NSException when trying to connect input node: %@, Reason: %@", exception.name, exception.reason);
      }
    }
  }

  auto numOutputBuses = [avAudioUnit numberOfOutputs];
  AVAudioMixerNode* mainMixer = [engine mainMixerNode];
  AVAudioFormat* pluginOutputFormat = [avAudioUnit outputFormatForBus:0];
  AVAudioNode* outputNode = [engine outputNode];

  if (numOutputBuses > 1)
  {
    for (int busIdx = 0; busIdx < numOutputBuses; busIdx++)
    {
      [engine connect:avAudioUnit to:mainMixer fromBus:busIdx toBus:[mainMixer nextAvailableInputBus] format:pluginOutputFormat];
    }
  }
  else
  {
    [engine connect:avAudioUnit to:outputNode format:pluginOutputFormat];
  }
}

- (void)printEngineInfo
{
  if (!isInstrument())
  {
    AVAudioFormat* inputNodeFormat = [[engine inputNode] inputFormatForBus:0];
    AVAudioFormat* pluginInputFormat = [avAudioUnit inputFormatForBus:0];
    NSLog(@"Input Node SR: %i", int(inputNodeFormat.sampleRate));
    NSLog(@"Input Node Chans: %i", inputNodeFormat.channelCount);
    NSLog(@"Plugin Input SR: %i", int(pluginInputFormat.sampleRate));
    NSLog(@"Plugin Input Chans: %i", pluginInputFormat.channelCount);
  }

  AVAudioFormat* pluginOutputFormat = [avAudioUnit outputFormatForBus:0];
  AVAudioFormat* outputNodeFormat = [[engine outputNode] inputFormatForBus:0];

  NSLog(@"Plugin Output SR: %i", int(pluginOutputFormat.sampleRate));
  NSLog(@"Plugin Output Chans: %i", pluginOutputFormat.channelCount);
  NSLog(@"Output Node SR: %i", int(outputNodeFormat.sampleRate));
  NSLog(@"Output Node Chans: %i", outputNodeFormat.channelCount);
}

- (void)printSessionInfo
{
  AVAudioSession* session = [AVAudioSession sharedInstance];
  NSLog(@"Session SR: %i", int(session.sampleRate));
  NSLog(@"Session IO Buffer: %i", int((session.IOBufferDuration * session.sampleRate) + 0.5));
  if (!isInstrument())
    NSLog(@"Session Input Chans: %i", int(session.inputNumberOfChannels));
  NSLog(@"Session Output Chans: %i", int(session.outputNumberOfChannels));
  if (!isInstrument())
    NSLog(@"Session Input Latency: %f ms", session.inputLatency * 1000.0f);
  NSLog(@"Session Output Latency: %f ms", session.outputLatency * 1000.0f);
  AVAudioSessionRouteDescription* currentRoute = [session currentRoute];
  for (AVAudioSessionPortDescription* input in currentRoute.inputs)
  {
    NSLog(@"Input Port Name: %@", input.portName);
  }

  for (AVAudioSessionPortDescription* output in currentRoute.outputs)
  {
    NSLog(@"Output Port Name: %@", output.portName);
  }
}

- (void)addNotifications
{
  NSNotificationCenter* notifCtr = [NSNotificationCenter defaultCenter];

  [notifCtr addObserver:self selector:@selector(onEngineConfigurationChange:) name:AVAudioEngineConfigurationChangeNotification object:engine];
  [notifCtr addObserver:self selector:@selector(onAudioSettingsChanged:) name:kAudioSettingsChangedNotification object:nil];
  [notifCtr addObserver:self selector:@selector(onAppStateShouldSave:) name:UIApplicationWillResignActiveNotification object:nil];
  [notifCtr addObserver:self selector:@selector(onAppStateShouldSave:) name:UIApplicationDidEnterBackgroundNotification object:nil];
  [notifCtr addObserver:self selector:@selector(onAppStateShouldSave:) name:UIApplicationWillTerminateNotification object:nil];
}

#pragma mark Notifications
- (void)onEngineConfigurationChange:(NSNotification*)notification
{
  [self restartAudioEngine];
}

- (void)onAudioSettingsChanged:(NSNotification*)notification
{
  [self setupSession];
  [self restartAudioEngine];
}

- (void)onAppStateShouldSave:(NSNotification*)notification
{
  [self saveStandaloneState];
}

@end
