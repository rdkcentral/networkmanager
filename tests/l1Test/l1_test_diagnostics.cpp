/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2026 RDK Management
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
**/

#include "NetworkManagerDiagnostics.h"
#include "NetworkManagerDiagnosticsJson.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <algorithm>
#include <condition_variable>
#include <future>
#include <mutex>
#include <sys/wait.h>

using namespace std;
using namespace WPEFramework;
using namespace WPEFramework::Plugin::Diagnostics;

namespace {

    int ExitStatus(const int code)
    {
        return code << 8;
    }

    ProcessResult Finished(const string& output, const int exitCode)
    {
        ProcessResult result;
        result.started = true;
        result.status = ExitStatus(exitCode);
        result.output = output;
        return result;
    }

    JsonObject Parse(const string& json)
    {
        JsonObject object;
        object.FromString(json);
        return object;
    }

    // The JSON the sync INetworkManager::Ping/Trace return for a finished child.
    string PingResponse(const ProcessResult& result, const string& endpoint)
    {
        string json;
        PingJson(ParsePing(result), endpoint).ToString(json);
        return json;
    }

    string TraceResponse(const ProcessResult& result, const string& endpoint)
    {
        string json;
        TraceJson(result.output, endpoint).ToString(json);
        return json;
    }

    JsonArray TraceLines(const string& response)
    {
        JsonArray lines;
        lines.FromString(Parse(response)["results"].String());
        return lines;
    }

    // Each Run() blocks until released (or cancelled) so tests can observe what runs concurrently.
    class GatedRunner : public IProcessRunner {
    public:
        ProcessResult Run(const std::vector<string>& argv, const std::chrono::milliseconds, const std::atomic<bool>& cancel) override
        {
            std::unique_lock<std::mutex> guard(_lock);
            _started.push_back(argv.back());
            ++_running;
            _maxRunning = std::max(_maxRunning, _running);
            _signal.notify_all();
            while (!cancel.load() && (_releases == 0))
                _signal.wait_for(guard, std::chrono::milliseconds(10));
            if (_releases > 0)
                --_releases;
            --_running;
            return Finished("done\n", 0);
        }

        void Release(const int count = 1)
        {
            std::lock_guard<std::mutex> guard(_lock);
            _releases += count;
            _signal.notify_all();
        }

        bool WaitStarted(const size_t count)
        {
            std::unique_lock<std::mutex> guard(_lock);
            return _signal.wait_for(guard, std::chrono::seconds(5), [&] { return _started.size() >= count; });
        }

        std::vector<string> Started()
        {
            std::lock_guard<std::mutex> guard(_lock);
            return _started;
        }

        int MaxRunning()
        {
            std::lock_guard<std::mutex> guard(_lock);
            return _maxRunning;
        }

    private:
        std::mutex _lock;
        std::condition_variable _signal;
        std::vector<string> _started;
        int _running = 0;
        int _maxRunning = 0;
        int _releases = 0;
    };

    class Outcomes {
    public:
        Engine::Job Job(const string& name)
        {
            Engine::Job job;
            job.argv = { "fake", name };
            job.deadline = std::chrono::seconds(5);
            job.onDone = [this, name](const ProcessResult&) { Record(_done, name); };
            job.onDrop = [this, name]() { Record(_dropped, name); };
            return job;
        }

        bool WaitDone(const size_t count)
        {
            std::unique_lock<std::mutex> guard(_lock);
            return _signal.wait_for(guard, std::chrono::seconds(5), [&] { return _done.size() >= count; });
        }

        std::vector<string> Done() { std::lock_guard<std::mutex> guard(_lock); return _done; }
        std::vector<string> Dropped() { std::lock_guard<std::mutex> guard(_lock); return _dropped; }

    private:
        void Record(std::vector<string>& list, const string& name)
        {
            std::lock_guard<std::mutex> guard(_lock);
            list.push_back(name);
            _signal.notify_all();
        }

        std::mutex _lock;
        std::condition_variable _signal;
        std::vector<string> _done;
        std::vector<string> _dropped;
    };
}

// ---- Endpoint validation ----

TEST(DiagnosticsEndpoint, AcceptsAddressesAndHostnames)
{
    for (const string endpoint : { "10.0.0.1", "8.8.8.8", "2001:4860:4860::8888", "::1", "fe80::1%eth0",
                                   "cnn.com", "gateway", "my-router.local", "xn--bcher-kva.example", "a1.b2.c3", "example.com." }) {
        EXPECT_TRUE(IsValidEndpoint(endpoint)) << endpoint;
    }
}

TEST(DiagnosticsEndpoint, RejectsShellMetacharactersAndSpaces)
{
    for (const string endpoint : { "8.8.8.8; reboot", "8.8.8.8 $(id)", "`id`", "a|b", "a&b", "a>b", "host name",
                                   "8.8.8.8\n", "'8.8.8.8'", "a/b", "fe80::1%eth0;id" }) {
        EXPECT_FALSE(IsValidEndpoint(endpoint)) << endpoint;
    }
}

TEST(DiagnosticsEndpoint, RejectsEmbeddedNul)
{
    EXPECT_FALSE(IsValidEndpoint(string("8.8.8.8\0;invalid", 16)));
    EXPECT_FALSE(IsValidEndpoint(string("example.com\0", 12)));
}

TEST(DiagnosticsEndpoint, RejectsOptionInjection)
{
    for (const string endpoint : { "-f", "-i0", "--help", "-c100000" }) {
        EXPECT_FALSE(IsValidEndpoint(endpoint)) << endpoint;
    }
}

TEST(DiagnosticsEndpoint, RejectsMalformedHostnames)
{
    EXPECT_FALSE(IsValidEndpoint(""));
    EXPECT_FALSE(IsValidEndpoint(string(64, 'a') + ".com"));
    EXPECT_FALSE(IsValidEndpoint(string(254, 'a')));
    EXPECT_FALSE(IsValidEndpoint("a..b"));
    EXPECT_FALSE(IsValidEndpoint(".a"));
    EXPECT_FALSE(IsValidEndpoint("."));
    EXPECT_FALSE(IsValidEndpoint("a.."));
    EXPECT_FALSE(IsValidEndpoint("a-."));
    EXPECT_FALSE(IsValidEndpoint("a-.b"));
    EXPECT_FALSE(IsValidEndpoint("a.-b"));
    EXPECT_TRUE(IsValidEndpoint(string(63, 'a') + ".com"));
}

// ---- Argument vectors and deadlines ----

TEST(DiagnosticsArgs, EndpointIsOneArgumentAfterOptionTerminator)
{
    const string endpoint = "2001:4860:4860::8888";
    for (const auto& argv : { PingArgs(true, 3, 2, endpoint), TraceArgs(true, 3, endpoint) }) {
        EXPECT_EQ(1, std::count(argv.begin(), argv.end(), endpoint));
        const auto at = std::find(argv.begin(), argv.end(), endpoint);
        ASSERT_NE(argv.begin(), at);
        EXPECT_EQ("--", *(at - 1));
    }
    EXPECT_EQ("ping", PingArgs(false, 1, 1, "x")[0]);
    EXPECT_EQ("ping6", PingArgs(true, 1, 1, "x")[0]);
    EXPECT_EQ("traceroute", TraceArgs(false, 1, "x")[0]);
    EXPECT_EQ("traceroute6", TraceArgs(true, 1, "x")[0]);
}

TEST(DiagnosticsArgs, PingIsQuietSoLargeCountsCannotPushTheSummaryPastTheOutputCap)
{
    for (const bool ipv6 : { false, true }) {
        const auto argv = PingArgs(ipv6, 20000, 1, "x");
        EXPECT_EQ(1, std::count(argv.begin(), argv.end(), "-q"));
    }
}

TEST(DiagnosticsArgs, DeadlinesGrowWithRequestAndAreCapped)
{
    EXPECT_LT(PingDeadline(1, 1), PingDeadline(5, 2));
    EXPECT_GE(PingDeadline(5, 2), std::chrono::seconds(10));
    EXPECT_GT(PingDeadline(0, 1), std::chrono::milliseconds(0));
    // timeout 0: the probes alone take count x 2 ms at "-i 0.002"
    EXPECT_GE(PingDeadline(10000, 0), std::chrono::seconds(20));
    EXPECT_LT(TraceDeadline(1), TraceDeadline(2));
    EXPECT_EQ(std::chrono::seconds(120), PingDeadline(1000000, 60));
    EXPECT_EQ(std::chrono::seconds(120), TraceDeadline(100));
}

// ---- Output parsing ----

TEST(DiagnosticsPingResponse, ParsesSummary)
{
    const string output =
        "PING 8.8.8.8 (8.8.8.8): 56 data bytes\n"
        "64 bytes from 8.8.8.8: seq=0 ttl=119 time=23.363 ms\n"
        "\n"
        "--- 8.8.8.8 ping statistics ---\n"
        "3 packets transmitted, 3 packets received, 0% packet loss\n"
        "round-trip min/avg/max/mdev = 23.363/23.395/23.440/0.179 ms\n";
    JsonObject response = Parse(PingResponse(Finished(output, 0), "8.8.8.8"));

    EXPECT_TRUE(response["success"].Boolean());
    EXPECT_FALSE(response.HasLabel("error"));
    EXPECT_EQ("8.8.8.8", response["endpoint"].String());
    EXPECT_EQ(3, response["packetsTransmitted"].Number());
    EXPECT_EQ(3, response["packetsReceived"].Number());
    EXPECT_EQ("23.395", response["tripAvg"].String());
    EXPECT_EQ("23.440", response["tripMax"].String());
}

TEST(DiagnosticsPingResponse, NonZeroExitIsFailure)
{
    JsonObject response = Parse(PingResponse(Finished("3 packets transmitted, 0 packets received, 100% packet loss\n", 1), "192.0.0.1"));
    EXPECT_FALSE(response["success"].Boolean());
    EXPECT_EQ("Could not ping endpoint", response["error"].String());
    EXPECT_EQ(0, response["packetsReceived"].Number());
}

TEST(DiagnosticsPingResponse, NotStartedOrTimedOutIsFailure)
{
    ProcessResult notStarted;
    EXPECT_FALSE(Parse(PingResponse(notStarted, "8.8.8.8"))["success"].Boolean());

    ProcessResult timedOut = Finished("", 0);
    timedOut.timedOut = true;
    EXPECT_FALSE(Parse(PingResponse(timedOut, "8.8.8.8"))["success"].Boolean());
}

TEST(DiagnosticsTraceResponse, OneElementPerLineIncludingStderrText)
{
    const string output =
        "traceroute to 8.8.8.8 (8.8.8.8), 6 hops max, 52 byte packets\n"
        "1  gateway (10.46.5.1)  0.448 ms\n"
        "traceroute6: bad address '8.8.8.8'";
    JsonObject response = Parse(TraceResponse(Finished(output, 0), "8.8.8.8"));
    JsonArray lines = TraceLines(TraceResponse(Finished(output, 0), "8.8.8.8"));

    EXPECT_EQ("8.8.8.8", response["endpoint"].String());
    ASSERT_EQ(3, lines.Length());
    EXPECT_THAT(lines[0].String(), ::testing::HasSubstr("6 hops max"));
    EXPECT_THAT(lines[2].String(), ::testing::HasSubstr("bad address '8.8.8.8'"));
}

TEST(DiagnosticsTraceResponse, LongLineKeptWholeWithNoExtraBytes)
{
    const string longLine = "1  " + string(3000, 'x');
    JsonArray lines = TraceLines(TraceResponse(Finished(longLine + "\n2  *\n", 0), "8.8.8.8"));

    ASSERT_EQ(2, lines.Length());
    const string first = lines[0].String();
    EXPECT_EQ(0u, first.find(longLine));
    EXPECT_LE(first.size(), longLine.size() + 1);
    EXPECT_EQ(string::npos, first.find_first_not_of(longLine + " "));
}

TEST(DiagnosticsTraceResponse, EmptyOutputGivesEmptyList)
{
    EXPECT_EQ(0, TraceLines(TraceResponse(Finished("", 0), "8.8.8.8")).Length());
}

// ---- Exact bytes of the sync INetworkManager::Ping/Trace JSON (unchanged since the shell-based implementation) ----

namespace {
    string SyncPingJson(const ProcessResult& result, const string& endpoint)
    {
        return PingResponse(result, endpoint);
    }

    string SyncTraceJson(const ProcessResult& result, const string& endpoint)
    {
        return TraceResponse(result, endpoint);
    }
}

TEST(DiagnosticsGolden, PingFullStatistics)
{
    const string output =
        "PING 8.8.8.8 (8.8.8.8): 56 data bytes\n"
        "64 bytes from 8.8.8.8: seq=0 ttl=119 time=23.363 ms\n"
        "\n"
        "--- 8.8.8.8 ping statistics ---\n"
        "3 packets transmitted, 3 packets received, 0% packet loss\n"
        "round-trip min/avg/max/mdev = 23.363/23.395/23.440/0.179 ms\n";
    EXPECT_EQ(R"({"packetsTransmitted":3,"packetsReceived":3,"packetLoss":" 0","tripMin":" 23.363","tripAvg":"23.395","tripMax":"23.440","tripStdDev":"0.179 ms","success":true,"endpoint":"8.8.8.8"})", SyncPingJson(Finished(output, 0), "8.8.8.8"));
}

TEST(DiagnosticsGolden, PingBusyboxStatistics)
{
    const string output =
        "PING 8.8.8.8 (8.8.8.8): 56 data bytes\n"
        "\n"
        "--- 8.8.8.8 ping statistics ---\n"
        "5 packets transmitted, 4 packets received, 20% packet loss\n"
        "round-trip min/avg/max = 17.038/18.310/20.197 ms\n";
    EXPECT_EQ(R"({"packetsTransmitted":5,"packetsReceived":4,"packetLoss":" 20","tripMin":" 17.038","tripAvg":"18.310","tripMax":"20.197 ms","success":true,"endpoint":"8.8.8.8"})", SyncPingJson(Finished(output, 0), "8.8.8.8"));
}

TEST(DiagnosticsGolden, PingNonZeroExit)
{
    EXPECT_EQ(R"({"packetsTransmitted":3,"packetsReceived":0,"packetLoss":" 100","success":false,"error":"Could not ping endpoint","endpoint":"192.0.0.1"})", SyncPingJson(Finished("3 packets transmitted, 0 packets received, 100% packet loss\n", 1), "192.0.0.1"));
}

TEST(DiagnosticsGolden, PingBadAddress)
{
    EXPECT_EQ(R"({"success":false,"error":"Could not ping endpoint","endpoint":"nowhere.invalid"})", SyncPingJson(Finished("ping: bad address 'nowhere.invalid'\n", 1), "nowhere.invalid"));
}

TEST(DiagnosticsGolden, PingBadAddressButExitZero)
{
    EXPECT_EQ(R"({"success":true,"error":"Bad Address","endpoint":"nowhere.invalid"})", SyncPingJson(Finished("ping: bad address 'nowhere.invalid'\n", 0), "nowhere.invalid"));
}

TEST(DiagnosticsGolden, PingNotStarted)
{
    EXPECT_EQ(R"({"success":false,"error":"Could not ping endpoint","endpoint":"8.8.8.8"})", SyncPingJson(ProcessResult(), "8.8.8.8"));
}

TEST(DiagnosticsGolden, TraceLinesEndingInNewline)
{
    const string output =
        "traceroute to 8.8.8.8 (8.8.8.8), 6 hops max, 52 byte packets\n"
        "1  gateway (10.46.5.1)  0.448 ms\n"
        "2  *\n";
    EXPECT_EQ(R"({"endpoint":"8.8.8.8","results":"[\"traceroute to 8.8.8.8 (8.8.8.8), 6 hops max, 52 byte packets \",\"1  gateway (10.46.5.1)  0.448 ms \",\"2  * \"]"})", SyncTraceJson(Finished(output, 0), "8.8.8.8"));
}

TEST(DiagnosticsGolden, TraceLastLineWithoutNewline)
{
    EXPECT_EQ(R"({"endpoint":"8.8.8.8","results":"[\"1  \\\"quoted\\\"\\\\path \",\"2  *\"]"})", SyncTraceJson(Finished("1  \"quoted\"\\path\n2  *", 0), "8.8.8.8"));
}

TEST(DiagnosticsGolden, TraceEmptyOutput)
{
    EXPECT_EQ(R"({"endpoint":"8.8.8.8","results":"[]"})", SyncTraceJson(Finished("", 0), "8.8.8.8"));
}

TEST(DiagnosticsParsePing, KeepsPingTextAndReportsWhatWasPrinted)
{
    const auto ping = ParsePing(Finished(
        "5 packets transmitted, 4 packets received, 20% packet loss\n"
        "round-trip min/avg/max = 17.038/18.310/20.197 ms\n", 0));

    EXPECT_TRUE(ping.success);
    EXPECT_TRUE(ping.error.empty());
    EXPECT_TRUE(ping.countsReported);
    EXPECT_EQ(5, ping.packetsTransmitted);
    EXPECT_EQ(4, ping.packetsReceived);
    EXPECT_EQ(" 20", ping.packetLoss);
    EXPECT_EQ(" 17.038", ping.tripMin);
    EXPECT_EQ("18.310", ping.tripAvg);
    EXPECT_EQ("20.197 ms", ping.tripMax);
    EXPECT_TRUE(ping.tripStdDev.empty());
}

TEST(DiagnosticsParsePing, NothingPrintedMeansNothingReported)
{
    const auto ping = ParsePing(ProcessResult());

    EXPECT_FALSE(ping.success);
    EXPECT_EQ("Could not ping endpoint", ping.error);
    EXPECT_FALSE(ping.countsReported);
    EXPECT_TRUE(ping.tripMin.empty() && ping.tripAvg.empty() && ping.tripMax.empty() && ping.tripStdDev.empty());
}

// ---- Engine ----

TEST(DiagnosticsEngine, PingAndTraceRunTogether)
{
    auto runner = std::make_shared<GatedRunner>();
    Outcomes outcomes;
    Engine engine(runner);

    EXPECT_EQ(Core::ERROR_NONE, engine.Submit(Engine::Kind::TRACE, outcomes.Job("trace")));
    EXPECT_EQ(Core::ERROR_NONE, engine.Submit(Engine::Kind::PING, outcomes.Job("ping")));
    ASSERT_TRUE(runner->WaitStarted(2));
    EXPECT_EQ(2, runner->MaxRunning());

    runner->Release(2);
    ASSERT_TRUE(outcomes.WaitDone(2));
}

TEST(DiagnosticsEngine, SameKindIsQueuedAndRunsInOrder)
{
    auto runner = std::make_shared<GatedRunner>();
    Outcomes outcomes;
    Engine engine(runner);

    EXPECT_EQ(Core::ERROR_NONE, engine.Submit(Engine::Kind::PING, outcomes.Job("tester")));
    ASSERT_TRUE(runner->WaitStarted(1));
    EXPECT_EQ(Core::ERROR_NONE, engine.Submit(Engine::Kind::PING, outcomes.Job("as")));

    runner->Release(2);
    ASSERT_TRUE(outcomes.WaitDone(2));
    EXPECT_EQ((std::vector<string>{ "tester", "as" }), outcomes.Done());
    EXPECT_EQ(1, runner->MaxRunning());
}

// A COM-RPC client whose Completed() calls the synchronous Ping: the sync request must not wait for the callback.
TEST(DiagnosticsEngine, DispatchedCallbackCanWaitForInlineJobOfSameKind)
{
    auto runner = std::make_shared<GatedRunner>();
    runner->Release(2);
    Engine engine(runner);
    std::promise<bool> innerFinished;

    Engine::Job outer;
    outer.argv = { "fake", "outer" };
    outer.deadline = std::chrono::seconds(5);
    outer.onDone = [&](const ProcessResult&) {
        auto inner = std::make_shared<std::promise<void>>();
        auto innerDone = inner->get_future();
        Engine::Job job;
        job.argv = { "fake", "inner" };
        job.deadline = std::chrono::seconds(5);
        job.notify = Engine::Notify::INLINE;
        job.onDone = [inner](const ProcessResult&) { inner->set_value(); };
        EXPECT_EQ(Core::ERROR_NONE, engine.Submit(Engine::Kind::PING, std::move(job)));
        innerFinished.set_value(innerDone.wait_for(std::chrono::seconds(3)) == std::future_status::ready);
    };
    EXPECT_EQ(Core::ERROR_NONE, engine.Submit(Engine::Kind::PING, std::move(outer)));

    auto finished = innerFinished.get_future();
    ASSERT_EQ(std::future_status::ready, finished.wait_for(std::chrono::seconds(10)));
    EXPECT_TRUE(finished.get());
}

TEST(DiagnosticsEngine, RejectsWhenQueueFull)
{
    auto runner = std::make_shared<GatedRunner>();
    Outcomes outcomes;
    Engine engine(runner);

    EXPECT_EQ(Core::ERROR_NONE, engine.Submit(Engine::Kind::PING, outcomes.Job("running")));
    ASSERT_TRUE(runner->WaitStarted(1));
    for (size_t i = 0; i < Engine::MaxQueued; ++i)
        EXPECT_EQ(Core::ERROR_NONE, engine.Submit(Engine::Kind::PING, outcomes.Job("queued")));

    EXPECT_EQ(Core::ERROR_INPROGRESS, engine.Submit(Engine::Kind::PING, outcomes.Job("rejected")));
    EXPECT_EQ(Core::ERROR_NONE, engine.Submit(Engine::Kind::TRACE, outcomes.Job("trace")));

    runner->Release(static_cast<int>(Engine::MaxQueued) + 2);
    ASSERT_TRUE(outcomes.WaitDone(Engine::MaxQueued + 2));
    const auto done = outcomes.Done();
    EXPECT_EQ(0, std::count(done.begin(), done.end(), "rejected"));
}

TEST(DiagnosticsEngine, StopDropsRunningAndQueuedJobsWithoutCompleting)
{
    auto runner = std::make_shared<GatedRunner>();
    Outcomes outcomes;
    Engine engine(runner);

    EXPECT_EQ(Core::ERROR_NONE, engine.Submit(Engine::Kind::TRACE, outcomes.Job("running")));
    ASSERT_TRUE(runner->WaitStarted(1));
    EXPECT_EQ(Core::ERROR_NONE, engine.Submit(Engine::Kind::TRACE, outcomes.Job("queued")));

    engine.Stop();

    EXPECT_TRUE(outcomes.Done().empty());
    auto dropped = outcomes.Dropped();
    std::sort(dropped.begin(), dropped.end());
    EXPECT_EQ((std::vector<string>{ "queued", "running" }), dropped);
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, engine.Submit(Engine::Kind::PING, outcomes.Job("late")));
}

// ---- Real process runner ----

TEST(DiagnosticsProcessRunner, CapturesStdoutAndStderrAndExitCode)
{
    ProcessRunner runner;
    std::atomic<bool> cancel(false);
    const ProcessResult result = runner.Run({ "sh", "-c", "echo out; echo err >&2; exit 3" }, std::chrono::seconds(5), cancel);

    EXPECT_TRUE(result.started);
    EXPECT_FALSE(result.timedOut);
    EXPECT_THAT(result.output, ::testing::HasSubstr("out"));
    EXPECT_THAT(result.output, ::testing::HasSubstr("err"));
    ASSERT_TRUE(WIFEXITED(result.status));
    EXPECT_EQ(3, WEXITSTATUS(result.status));
    EXPECT_FALSE(result.Succeeded());
}

TEST(DiagnosticsProcessRunner, ArgumentsAreNotInterpretedByAShell)
{
    ProcessRunner runner;
    std::atomic<bool> cancel(false);
    const ProcessResult result = runner.Run({ "echo", "a; echo injected" }, std::chrono::seconds(5), cancel);

    EXPECT_TRUE(result.Succeeded());
    EXPECT_EQ("a; echo injected\n", result.output);
}

TEST(DiagnosticsProcessRunner, DeadlineKillsWholeProcessGroup)
{
    ProcessRunner runner;
    std::atomic<bool> cancel(false);
    const auto start = std::chrono::steady_clock::now();
    // The background sleep keeps the pipe open; only a process-group kill ends it.
    const ProcessResult result = runner.Run({ "sh", "-c", "sleep 30 & sleep 30" }, std::chrono::milliseconds(300), cancel);

    EXPECT_TRUE(result.timedOut);
    EXPECT_FALSE(result.Succeeded());
    EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds(5));
}

TEST(DiagnosticsProcessRunner, LeaderExitedButBackgroundChildHoldsOutput_DeadlineStillApplies)
{
    ProcessRunner runner;
    std::atomic<bool> cancel(false);
    const auto start = std::chrono::steady_clock::now();
    const ProcessResult result = runner.Run({ "sh", "-c", "echo started; sleep 30 &" }, std::chrono::milliseconds(300), cancel);

    EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds(5));
    EXPECT_TRUE(result.timedOut);
    EXPECT_THAT(result.output, ::testing::HasSubstr("started"));
}

TEST(DiagnosticsProcessRunner, LeaderExitedButBackgroundChildHoldsOutput_CancelStillApplies)
{
    ProcessRunner runner;
    std::atomic<bool> cancel(false);
    auto pending = std::async(std::launch::async, [&] {
        return runner.Run({ "sh", "-c", "sleep 30 &" }, std::chrono::seconds(60), cancel);
    });
    cancel.store(true);

    ASSERT_EQ(std::future_status::ready, pending.wait_for(std::chrono::seconds(5)));
}

TEST(DiagnosticsProcessRunner, OutputWrittenAfterLeaderExitsIsKept)
{
    ProcessRunner runner;
    std::atomic<bool> cancel(false);
    const ProcessResult result = runner.Run({ "sh", "-c", "echo first; (sleep 0.2; echo second) &" }, std::chrono::seconds(5), cancel);

    EXPECT_FALSE(result.timedOut);
    EXPECT_TRUE(result.Succeeded());
    EXPECT_THAT(result.output, ::testing::HasSubstr("first"));
    EXPECT_THAT(result.output, ::testing::HasSubstr("second"));
}

TEST(DiagnosticsProcessRunner, CancelStopsChild)
{
    ProcessRunner runner;
    std::atomic<bool> cancel(false);
    auto pending = std::async(std::launch::async, [&] {
        return runner.Run({ "sleep", "30" }, std::chrono::seconds(60), cancel);
    });
    cancel.store(true);

    ASSERT_EQ(std::future_status::ready, pending.wait_for(std::chrono::seconds(5)));
    EXPECT_FALSE(pending.get().Succeeded());
}

TEST(DiagnosticsProcessRunner, MissingProgramFails)
{
    ProcessRunner runner;
    std::atomic<bool> cancel(false);
    EXPECT_FALSE(runner.Run({ "/nonexistent/ping" }, std::chrono::seconds(5), cancel).Succeeded());
}
