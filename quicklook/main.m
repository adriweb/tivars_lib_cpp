#import <AppKit/AppKit.h>

#import "AppDelegate.h"

int main(int argc, const char * argv[])
{
    @autoreleasepool
    {
        AppDelegate* delegate = [[AppDelegate alloc] init];
        NSApplication.sharedApplication.delegate = delegate;
        return NSApplicationMain(argc, argv);
    }
}
