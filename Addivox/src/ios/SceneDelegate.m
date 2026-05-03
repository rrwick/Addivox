#import "SceneDelegate.h"

#include "config.h"

@implementation SceneDelegate

- (void)scene:(UIScene*)scene willConnectToSession:(UISceneSession*)session options:(UISceneConnectionOptions*)connectionOptions
{
  if (![scene isKindOfClass:UIWindowScene.class])
    return;

  UIWindowScene* windowScene = (UIWindowScene*)scene;
  UISceneSizeRestrictions* sizeRestrictions = windowScene.sizeRestrictions;
  if (sizeRestrictions)
  {
    sizeRestrictions.minimumSize = CGSizeMake(500., 375.);
    sizeRestrictions.maximumSize = CGSizeMake((CGFloat)PLUG_WIDTH * 2., (CGFloat)PLUG_HEIGHT * 2.);
  }
}

- (void)sceneDidBecomeActive:(UIScene*)scene
{
}

- (void)sceneWillResignActive:(UIScene*)scene
{
}

- (void)sceneWillEnterForeground:(UIScene*)scene
{
}

- (void)sceneDidEnterBackground:(UIScene*)scene
{
}

@end
