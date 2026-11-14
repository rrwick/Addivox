#include "host_view_ios.h"

#import <UIKit/UIKit.h>

namespace addivox_ios {

void CenterViewInSuperview(void* platformView) {
  UIView* view = (__bridge UIView*)platformView;
  UIView* superview = view.superview;
  if (superview == nil) return;

  view.center = CGPointMake(CGRectGetMidX(superview.bounds), CGRectGetMidY(superview.bounds));
}

} // namespace addivox_ios
