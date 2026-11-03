// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// NSURLSession transport of HttpClient (compiled with ARC)
// TODO(macOS) : not compiled nor tested yet, written alongside the WinHTTP transport

#import <Foundation/Foundation.h>

#include "HttpClient.h"

#include <mutex>

// receives a single task events, on the session delegate queue
@interface SBTaskDelegate : NSObject <NSURLSessionDataDelegate>
@property(nonatomic, assign) net::Response* response;
@property(nonatomic, assign) const net::OnBodyChunk* onChunk;
@property(nonatomic, strong) dispatch_semaphore_t done;
@property(nonatomic, assign) BOOL stoppedByCallback;
@end

@implementation SBTaskDelegate

- (void)URLSession:(NSURLSession*)session
              dataTask:(NSURLSessionDataTask*)dataTask
    didReceiveResponse:(NSURLResponse*)response
     completionHandler:(void (^)(NSURLSessionResponseDisposition))completionHandler {
    auto http = (NSHTTPURLResponse*)response;
    self.response->status = static_cast<int>(http.statusCode);

    [http.allHeaderFields enumerateKeysAndObjectsUsingBlock:^(id key, id value, BOOL*) {
      self.response->headers.push_back({ [[key description] UTF8String], [[value description] UTF8String] });
    }];

    completionHandler(NSURLSessionResponseAllow);
}

- (void)URLSession:(NSURLSession*)session dataTask:(NSURLSessionDataTask*)dataTask didReceiveData:(NSData*)data {
    [data enumerateByteRangesUsingBlock:^(const void* bytes, NSRange range, BOOL* stop) {
      const std::string_view chunk { static_cast<const char*>(bytes), range.length };

      if (self.onChunk) {
          if (!(*self.onChunk)(chunk)) {
              *stop = YES;
              self.stoppedByCallback = YES;
              [dataTask cancel];
          }
      } else {
          self.response->body += chunk;
      }
    }];
}

- (void)URLSession:(NSURLSession*)session task:(NSURLSessionTask*)task didCompleteWithError:(NSError*)error {
    // a task cancelled because the chunk callback asked to stop is not an error
    if (error && !(error.code == NSURLErrorCancelled && self.stoppedByCallback)) {
        self.response->error = error.code == NSURLErrorCancelled
                                   ? std::string { "Request cancelled" }
                                   : std::string { error.localizedDescription.UTF8String } + " (" + std::to_string(error.code) + ")";
    }

    dispatch_semaphore_signal(self.done);
}

@end

namespace net {

class HttpClient::Impl {
 public:
    explicit Impl(const std::string& userAgent) :
        _userAgent(userAgent) {}

    Response perform(const Request& request, const OnBodyChunk* onChunk, const std::stop_token& stop) {
        Response response;

        if (stop.stop_requested()) {
            response.error = "Request cancelled";
            return response;
        }

        NSURL* url = [NSURL URLWithString:@(request.url.c_str())];
        if (!url) {
            response.error = "Invalid URL";
            return response;
        }

        // zero means no limit
        const NSTimeInterval timeout = request.timeout.count() ? request.timeout.count() / 1000.0 : 365 * 24 * 3600.0;

        NSMutableURLRequest* urlRequest = [NSMutableURLRequest requestWithURL:url
                                                                  cachePolicy:NSURLRequestReloadIgnoringLocalCacheData
                                                              timeoutInterval:timeout];
        urlRequest.HTTPMethod = @(request.method.c_str());
        [urlRequest setValue:@(_userAgent.c_str()) forHTTPHeaderField:@"User-Agent"];

        for (const auto& header : request.headers) {
            [urlRequest setValue:@(header.value.c_str()) forHTTPHeaderField:@(header.name.c_str())];
        }

        if (!request.body.empty()) {
            urlRequest.HTTPBody = [NSData dataWithBytes:request.body.data() length:request.body.size()];
        }

        // one session per request : its delegate is the task's one
        SBTaskDelegate* delegate = [SBTaskDelegate new];
        delegate.response = &response;
        delegate.onChunk = onChunk;
        delegate.done = dispatch_semaphore_create(0);

        NSURLSession* session = [NSURLSession sessionWithConfiguration:NSURLSessionConfiguration.ephemeralSessionConfiguration
                                                              delegate:delegate
                                                         delegateQueue:nil];
        NSURLSessionDataTask* task = [session dataTaskWithRequest:urlRequest];

        {
            std::lock_guard lock { _mutex };
            _task = task;
        }

        {
            // once the task is registered : a stop already requested cancels it right away
            std::stop_callback cancelOnStop(stop, [this] { cancel(); });
            [task resume];
            dispatch_semaphore_wait(delegate.done, DISPATCH_TIME_FOREVER);
        }

        {
            std::lock_guard lock { _mutex };
            _task = nil;
        }

        [session finishTasksAndInvalidate];
        return response;
    }

    void cancel() {
        std::lock_guard lock { _mutex };
        [_task cancel];
    }

 private:
    std::string _userAgent;

    std::mutex _mutex;
    NSURLSessionDataTask* _task = nil;
};

HttpClient::HttpClient(const std::string& userAgent) :
    _impl(std::make_unique<Impl>(userAgent)) {}
HttpClient::~HttpClient() = default;

Response HttpClient::send(const Request& request, const std::stop_token& stop) {
    return _impl->perform(request, nullptr, stop);
}

Response HttpClient::stream(const Request& request, const OnBodyChunk& onChunk, const std::stop_token& stop) {
    return _impl->perform(request, &onChunk, stop);
}

void HttpClient::cancel() {
    _impl->cancel();
}

}  // namespace net
