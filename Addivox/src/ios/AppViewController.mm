#import "AppViewController.h"
#import "IPlugAUPlayer.h"
#import "IPlugAUAudioUnit.h"

#include "config.h"

#import "IPlugAUViewController.h"
#import <CoreAudioKit/CoreAudioKit.h>

#if !__has_feature(objc_arc)
#error This file must be compiled with Arc. Use -fobjc-arc flag
#endif

@interface AppViewController ()
{
  IPlugAUPlayer* player;
  IPLUG_AUVIEWCONTROLLER* pluginVC;
  IBOutlet UIView* auView;
}
@end

@implementation AppViewController

- (BOOL)prefersStatusBarHidden
{
  return YES;
}

- (UIInterfaceOrientationMask)supportedInterfaceOrientations
{
  return UIInterfaceOrientationMaskAll;
}

- (UIInterfaceOrientation)preferredInterfaceOrientationForPresentation
{
  return UIInterfaceOrientationLandscapeRight;
}

- (UIRectEdge)preferredScreenEdgesDeferringSystemGestures
{
  return UIRectEdgeAll;
}

#if __IPHONE_OS_VERSION_MAX_ALLOWED >= 260000
- (BOOL)prefersInterfaceOrientationLocked API_AVAILABLE(ios(26.0))
{
  return YES;
}
#endif

- (void)viewDidLoad
{
  [super viewDidLoad];

  self.view.backgroundColor = UIColor.blackColor;
  auView.backgroundColor = UIColor.blackColor;
  auView.clipsToBounds = YES;
  auView.translatesAutoresizingMaskIntoConstraints = YES;

#if PLUG_HAS_UI
  NSString* storyBoardName = [NSString stringWithFormat:@"%s-iOS-MainInterface", PLUG_NAME];
  UIStoryboard* storyboard = [UIStoryboard storyboardWithName:storyBoardName bundle:nil];
  pluginVC = [storyboard instantiateViewControllerWithIdentifier:@"main"];
  [self addChildViewController:pluginVC];
#endif

  AudioComponentDescription desc;

#if PLUG_TYPE == 0
#if PLUG_DOES_MIDI_IN
  desc.componentType = kAudioUnitType_MusicEffect;
#else
  desc.componentType = kAudioUnitType_Effect;
#endif
#elif PLUG_TYPE == 1
  desc.componentType = kAudioUnitType_MusicDevice;
#elif PLUG_TYPE == 2
  desc.componentType = 'aumi';
#endif

  desc.componentSubType = PLUG_UNIQUE_ID;
  desc.componentManufacturer = PLUG_MFR_ID;
  desc.componentFlags = 0;
  desc.componentFlagsMask = 0;

  [AUAudioUnit registerSubclass:IPLUG_AUAUDIOUNIT.class asComponentDescription:desc name:@"Local AUv3" version:UINT32_MAX];

  player = [[IPlugAUPlayer alloc] initWithComponentType:desc.componentType];

  [player loadAudioUnitWithComponentDescription:desc completion:^{
    self->pluginVC.audioUnit = (IPLUG_AUAUDIOUNIT*)self->player.currentAudioUnit;

    [self embedPlugInView];
  }];

  [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(receiveNotification:) name:@"LaunchBTMidiDialog" object:nil];
}

- (void)viewDidLayoutSubviews
{
  [super viewDidLayoutSubviews];
  [self layoutPlugInView];
}

- (void)receiveNotification:(NSNotification*)notification
{
  if ([notification.name isEqualToString:@"LaunchBTMidiDialog"])
  {
    NSDictionary* dict = notification.userInfo;
    NSNumber* x = (NSNumber*)dict[@"x"];
    NSNumber* y = (NSNumber*)dict[@"y"];

    CABTMIDICentralViewController* vc = [[CABTMIDICentralViewController alloc] init];
    UINavigationController* nc = [[UINavigationController alloc] initWithRootViewController:vc];
    nc.modalPresentationStyle = UIModalPresentationPopover;

    UIPopoverPresentationController* ppc = nc.popoverPresentationController;
    ppc.permittedArrowDirections = UIPopoverArrowDirectionAny;
    ppc.sourceView = self.view;
    ppc.sourceRect = CGRectMake([x floatValue], [y floatValue], 1., 1.);

    [self presentViewController:nc animated:YES completion:nil];
  }
}

- (void)embedPlugInView
{
#if PLUG_HAS_UI
  UIView* view = pluginVC.view;
  view.backgroundColor = UIColor.blackColor;
  view.clipsToBounds = YES;
  view.translatesAutoresizingMaskIntoConstraints = YES;
  [auView addSubview:view];
  [pluginVC didMoveToParentViewController:self];

#if TARGET_OS_VISION && defined(VISIONOS_TRANSPARENT_VC)
  self.view.opaque = false;
  self.view.backgroundColor = UIColor.clearColor;
#endif

  [self layoutPlugInView];
  dispatch_async(dispatch_get_main_queue(), ^{
    [self layoutPlugInView];
  });
#endif
}

- (void)layoutPlugInView
{
  const CGRect bounds = self.view.bounds;
  if (CGRectIsEmpty(bounds) || !auView)
    return;

  const CGFloat plugAspect = (CGFloat)PLUG_WIDTH / (CGFloat)PLUG_HEIGHT;
  CGFloat contentWidth = CGRectGetWidth(bounds);
  CGFloat contentHeight = contentWidth / plugAspect;

  if (contentHeight > CGRectGetHeight(bounds))
  {
    contentHeight = CGRectGetHeight(bounds);
    contentWidth = contentHeight * plugAspect;
  }

  auView.transform = CGAffineTransformIdentity;
  auView.frame = CGRectMake(CGRectGetMidX(bounds) - contentWidth * 0.5, CGRectGetMidY(bounds) - contentHeight * 0.5, contentWidth, contentHeight);

#if PLUG_HAS_UI
  UIView* view = pluginVC.view;
  if (!view.superview)
    return;

  view.transform = CGAffineTransformIdentity;
  view.frame = auView.bounds;

  UIView* graphicsView = pluginVC.view.subviews.firstObject;
  if (!graphicsView)
    return;

  CGSize naturalSize = graphicsView.bounds.size;
  if (naturalSize.width <= 0. || naturalSize.height <= 0.)
    return;

  const CGFloat scale = MIN(CGRectGetWidth(auView.bounds) / naturalSize.width, CGRectGetHeight(auView.bounds) / naturalSize.height);
  graphicsView.transform = CGAffineTransformIdentity;
  graphicsView.center = CGPointMake(CGRectGetMidX(auView.bounds), CGRectGetMidY(auView.bounds));
  graphicsView.transform = CGAffineTransformMakeScale(scale, scale);
#endif
}

#if TARGET_OS_VISION && defined(VISIONOS_TRANSPARENT_VC)
- (UIContainerBackgroundStyle)preferredContainerBackgroundStyle
{
  return UIContainerBackgroundStyleHidden;
}
#endif

@end
