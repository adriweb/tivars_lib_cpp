#import "ThumbnailProvider.h"
#import "TIVarsQuickLookSupport.h"

#import <QuickLookThumbnailing/QuickLookThumbnailing.h>

@implementation ThumbnailProvider

- (void)provideThumbnailForFileRequest:(QLFileThumbnailRequest *)request
                     completionHandler:(void (^)(QLThumbnailReply * _Nullable reply, NSError * _Nullable error))handler
{
    QLThumbnailReply* reply = [QLThumbnailReply replyWithContextSize:request.maximumSize
                                          currentContextDrawingBlock:^BOOL{
        NSError* thumbnailError = nil;
        NSString* badge = nil;
        [request.fileURL startAccessingSecurityScopedResource];
        const BOOL success = [TIVarsQuickLookSupport drawThumbnailForFileURL:request.fileURL
                                                                 contextSize:request.maximumSize
                                                                       badge:&badge
                                                                       error:&thumbnailError];
        [request.fileURL stopAccessingSecurityScopedResource];
        if (success && badge.length > 0)
        {
            reply.extensionBadge = badge;
        }
        if (!success)
        {
            NSLog(@"[TIVarsQL] provideThumbnailForFileRequest error=%@", thumbnailError);
        }
        return success;
    }];

    handler(reply, nil);
}

@end
