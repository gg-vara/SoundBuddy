// src/net/HttpClient, against public test services (httpbin.org, stream.wikimedia.org).
// Labelled "network" : left out of the default test runs, run with ctest -L network

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

#include "Check.h"
#include "src/net/HttpClient.h"
#include "src/net/Multipart.h"
#include "src/net/SseParser.h"

using namespace std::chrono_literals;

namespace {

long long millisecondsSince(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
}

}  // namespace

int main() {
    net::HttpClient client("SoundBuddy-network-test/1.0");

    // GET
    {
        const auto response = client.send({ .method = "GET", .url = "https://httpbin.org/get?x=1", .headers = { { .name = "Accept-Language", .value = "fr" } } });
        CHECK(response.ok() && response.status == 200, "GET : status 200");
        CHECK(response.body.find("\"x\": \"1\"") != std::string::npos, "query string sent");
        CHECK(response.body.find("SoundBuddy-network-test/1.0") != std::string::npos, "user agent sent");
        CHECK(response.body.find("\"Accept-Language\": \"fr\"") != std::string::npos, "custom header sent");
        CHECK(response.header("CONTENT-TYPE").value_or("") == "application/json", "case-insensitive response header");
        if (!response.ok()) std::printf("    error : %s\n", response.error.c_str());
    }

    // POST multipart
    {
        net::Request request { .method = "POST", .url = "https://httpbin.org/post" };
        net::Multipart form;
        form.addFile("shout_file", "shout_file.json", "application/json", "{\"name\":\"\xC3\xA9t\xC3\xA9\"}");
        form.addField("password", "not-a-real-password");
        form.addField("headless", "1");
        form.applyTo(request);

        const auto response = client.send(request);
        CHECK(response.ok(), "POST multipart : status 2xx");
        CHECK(response.body.find("\"headless\": \"1\"") != std::string::npos && response.body.find("\"password\": \"not-a-real-password\"") != std::string::npos,
            "form fields received");
        CHECK(response.body.find("\"shout_file\":") != std::string::npos, "file part received");
    }

    // errors
    {
        auto response = client.send({ .method = "GET", .url = "https://httpbin.org/status/403" });
        CHECK(response.error.empty() && response.status == 403 && !response.ok(), "HTTP 403 is a response, not a transport error");

        response = client.send({ .method = "GET", .url = "https://nonexistent.invalid/" });
        CHECK(!response.error.empty() && response.status == 0, "unresolvable host");

        net::Request slow { .method = "GET", .url = "https://httpbin.org/delay/5" };
        slow.timeout = 1500ms;
        const auto start = std::chrono::steady_clock::now();
        response = client.send(slow);
        CHECK(!response.error.empty() && millisecondsSince(start) < 4000, "timeout");
    }

    // cancel from another thread
    {
        std::thread canceller([&client] {
            std::this_thread::sleep_for(800ms);
            client.cancel();
        });
        const auto start = std::chrono::steady_clock::now();
        auto response = client.send({ .method = "GET", .url = "https://httpbin.org/delay/5" });
        const auto elapsed = millisecondsSince(start);
        canceller.join();
        CHECK(response.error == "Request cancelled" && elapsed < 3000, "cancelled quickly");

        response = client.send({ .method = "GET", .url = "https://httpbin.org/get" });
        CHECK(response.ok(), "client reusable after cancel");
    }

    // streamed body
    {
        std::string all;
        const auto response = client.stream({ .method = "GET", .url = "https://httpbin.org/stream/5" }, [&all](std::string_view chunk) {
            all += chunk;
            return true;
        });

        int lines = 0;
        for (const char c : all) lines += static_cast<int>(c == '\n');
        CHECK(response.ok() && response.body.empty() && lines == 5, "5 JSON lines streamed, body not buffered");
    }

    // Server-Sent Events
    {
        int events = 0;
        net::SseParser parser([&events](const net::SseEvent&) { ++events; });

        net::Request request { .method = "GET", .url = "https://stream.wikimedia.org/v2/stream/recentchange", .headers = { { .name = "Accept", .value = "text/event-stream" } } };
        request.timeout = 0ms;  // no limit for a stream

        const auto start = std::chrono::steady_clock::now();
        const auto response = client.stream(request, [&](std::string_view chunk) {
            parser.feed(chunk);
            return events < 3 && std::chrono::steady_clock::now() - start < 20s;
        });

        CHECK(response.status == 200 && events >= 3, "3 events received, then stopped by the callback");
    }

    return check::summary();
}
