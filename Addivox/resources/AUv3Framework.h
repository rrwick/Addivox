
#include <TargetConditionals.h>
#if TARGET_OS_IOS == 1 || TARGET_OS_VISION == 1
#import <UIKit/UIKit.h>
#else
#import <Cocoa/Cocoa.h>
#endif

//! Project version number for AUv3Framework.
FOUNDATION_EXPORT double AUv3FrameworkVersionNumber;

//! Project version string for AUv3Framework.
FOUNDATION_EXPORT const unsigned char AUv3FrameworkVersionString[];

// In this header, you should import all the public headers of your framework using statements like #import <AUv3Framework/PublicHeader.h>

// The view controller class name carries OBJC_PREFIX (vAddivox or vAddivoxDemo)
// so the demo and full frameworks can coexist in one process.
#ifndef OBJC_PREFIX
#define OBJC_PREFIX vAddivox
#endif
#define AUV3_CONCAT_(a,b) a##b
#define AUV3_CONCAT(a,b) AUV3_CONCAT_(a,b)
@class AUV3_CONCAT(IPlugAUViewController_, OBJC_PREFIX);
