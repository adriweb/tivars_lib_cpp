#import <AppKit/AppKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface TIVarsQuickLookSupport : NSObject

+ (NSData * _Nullable)previewHTMLDataForFileURL:(NSURL *)fileURL
                                          title:(NSString * _Nullable * _Nullable)title
                                          error:(NSError * _Nullable * _Nullable)error;
+ (BOOL)drawThumbnailForFileURL:(NSURL *)fileURL
                    contextSize:(CGSize)contextSize
                          badge:(NSString * _Nullable * _Nullable)badge
                          error:(NSError * _Nullable * _Nullable)error;

@end

NS_ASSUME_NONNULL_END
