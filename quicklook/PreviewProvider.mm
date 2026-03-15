#import "PreviewProvider.h"
#import "TIVarsQuickLookSupport.h"

#import <QuickLookUI/QuickLookUI.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

@implementation PreviewProvider

- (void)loadView
{
    self.view = [[NSView alloc] init];
}

- (void)providePreviewForFileRequest:(QLFilePreviewRequest *)request
                   completionHandler:(void (^)(QLPreviewReply * _Nullable reply, NSError * _Nullable error))handler
{
    [request.fileURL startAccessingSecurityScopedResource];
    NSError* previewError = nil;
    NSString* title = nil;
    NSData* htmlData = [TIVarsQuickLookSupport previewHTMLDataForFileURL:request.fileURL
                                                                   title:&title
                                                                   error:&previewError];
    [request.fileURL stopAccessingSecurityScopedResource];
    if (htmlData == nil)
    {
        handler(nil, previewError);
        return;
    }

    QLPreviewReply* reply = [[QLPreviewReply alloc] initWithDataOfContentType:UTTypeHTML
                                                                  contentSize:CGSizeMake(900.0, 900.0)
                                                            dataCreationBlock:^NSData * _Nullable(QLPreviewReply * _Nonnull replyToUpdate, NSError ** _Nonnull error) {
        replyToUpdate.stringEncoding = NSUTF8StringEncoding;
        replyToUpdate.title = title ?: request.fileURL.lastPathComponent;
        return htmlData;
    }];

    handler(reply, nil);
}

@end
