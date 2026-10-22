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
#include "NetworkManagerLogger.h"

#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <net/if.h>
#include <poll.h>
#include <sstream>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace std;

namespace WPEFramework
{
    namespace Plugin
    {
        namespace Diagnostics
        {
            namespace
            {
                constexpr size_t MaxOutputBytes = 1024 * 1024;
                constexpr int PollSliceMs = 100;
                constexpr int64_t DeadlineMarginMs = 5000;
                constexpr int64_t DeadlineCapMs = 120000;
                // traceroute -w 3 -m 6: at most 3 s per probe, 6 hops
                constexpr int64_t TraceMsPerQuery = 3000 * 6;

                bool IsHostnameChar(const char c)
                {
                    return ((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')) || ((c >= '0') && (c <= '9')) || (c == '-');
                }

                bool IsValidHostname(const string& name)
                {
                    size_t labelLength = 0;
                    char previous = '.';
                    for (const char c : name)
                    {
                        if (c == '.')
                        {
                            if ((labelLength == 0) || (previous == '-'))
                                return false;
                            labelLength = 0;
                        }
                        else if (IsHostnameChar(c))
                        {
                            if ((c == '-') && (labelLength == 0))
                                return false;
                            if (++labelLength > 63)
                                return false;
                        }
                        else
                        {
                            return false;
                        }
                        previous = c;
                    }
                    return (labelLength > 0) && (previous != '-');
                }

                bool IsValidIPv6WithZone(const string& endpoint)
                {
                    const size_t percent = endpoint.find('%');
                    if ((percent == string::npos) || (percent + 1 >= endpoint.size()) || (endpoint.size() - percent - 1 >= IF_NAMESIZE))
                        return false;

                    struct in6_addr address;
                    if (inet_pton(AF_INET6, endpoint.substr(0, percent).c_str(), &address) != 1)
                        return false;

                    for (size_t i = percent + 1; i < endpoint.size(); ++i)
                    {
                        const char c = endpoint[i];
                        if (!IsHostnameChar(c) && (c != '_') && (c != '.'))
                            return false;
                    }
                    return true;
                }

                std::chrono::milliseconds Capped(const int64_t ms)
                {
                    return std::chrono::milliseconds(std::min(ms, DeadlineCapMs));
                }

                void CloseInheritedFds()
                {
#ifdef SYS_close_range
                    if (::syscall(SYS_close_range, STDERR_FILENO + 1, ~0U, 0) == 0)
                        return;
#endif
                    long maxFd = ::sysconf(_SC_OPEN_MAX);
                    if ((maxFd < 0) || (maxFd > 65536))
                        maxFd = 65536;
                    for (long fd = STDERR_FILENO + 1; fd < maxFd; ++fd)
                        ::close(static_cast<int>(fd));
                }
            }

            bool ProcessResult::Succeeded() const
            {
                return started && !timedOut && WIFEXITED(status) && (WEXITSTATUS(status) == 0);
            }

            ProcessResult ProcessRunner::Run(const std::vector<string>& argv, const std::chrono::milliseconds deadline, const std::atomic<bool>& cancel)
            {
                ProcessResult result;
                if (argv.empty())
                    return result;

                // Built before fork(): the child must not allocate.
                std::vector<char*> args;
                for (const string& arg : argv)
                    args.push_back(const_cast<char*>(arg.c_str()));
                args.push_back(nullptr);

                int fds[2];
                if (::pipe2(fds, O_CLOEXEC) != 0)
                {
                    NMLOG_ERROR("pipe2 failed: %s", strerror(errno));
                    return result;
                }

                const pid_t pid = ::fork();
                if (pid < 0)
                {
                    NMLOG_ERROR("fork failed: %s", strerror(errno));
                    ::close(fds[0]);
                    ::close(fds[1]);
                    return result;
                }

                if (pid == 0)
                {
                    // Child: async-signal-safe calls only.
                    ::setpgid(0, 0);
                    ::dup2(fds[1], STDOUT_FILENO);
                    ::dup2(fds[1], STDERR_FILENO);
                    CloseInheritedFds();
                    ::execvp(args[0], args.data());
                    ::_exit(127);
                }

                // Also set from the parent so kill(-pid) works even if the child has not run yet.
                ::setpgid(pid, pid);
                ::close(fds[1]);
                result.started = true;

                const auto end = std::chrono::steady_clock::now() + deadline;
                int readFd = fds[0];
                bool exited = false;
                int status = -1;
                char buffer[4096];

                while (!exited)
                {
                    if (cancel.load())
                        break;
                    const auto now = std::chrono::steady_clock::now();
                    if (now >= end)
                    {
                        result.timedOut = true;
                        break;
                    }
                    const int remaining = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(end - now).count());
                    const int slice = std::max(1, std::min(remaining, PollSliceMs));

                    if (readFd >= 0)
                    {
                        struct pollfd entry = { readFd, POLLIN, 0 };
                        const int ready = ::poll(&entry, 1, slice);
                        if (ready > 0)
                        {
                            const ssize_t length = ::read(readFd, buffer, sizeof(buffer));
                            if (length > 0)
                            {
                                const size_t room = MaxOutputBytes - std::min(MaxOutputBytes, result.output.size());
                                result.output.append(buffer, std::min(static_cast<size_t>(length), room));
                                continue;
                            }
                            if ((length < 0) && (errno == EINTR))
                                continue;
                            // EOF: the child closed its output; fall through to reap it.
                            ::close(readFd);
                            readFd = -1;
                        }
                        else if ((ready < 0) && (errno != EINTR))
                        {
                            ::close(readFd);
                            readFd = -1;
                        }
                    }
                    else
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(std::min(slice, 10)));
                    }

                    const pid_t reaped = ::waitpid(pid, &status, WNOHANG);
                    if ((reaped == pid) || ((reaped < 0) && (errno != EINTR)))
                        exited = true;
                }

                if (!exited)
                {
                    ::kill(-pid, SIGKILL);
                    ::kill(pid, SIGKILL);
                    while ((::waitpid(pid, &status, 0) < 0) && (errno == EINTR))
                    {
                    }
                }
                else if (readFd >= 0)
                {
                    // Drain whatever the child wrote just before exiting.
                    ssize_t length;
                    while ((length = ::read(readFd, buffer, sizeof(buffer))) > 0)
                    {
                        const size_t room = MaxOutputBytes - std::min(MaxOutputBytes, result.output.size());
                        result.output.append(buffer, std::min(static_cast<size_t>(length), room));
                    }
                }
                if (readFd >= 0)
                    ::close(readFd);

                result.status = status;
                return result;
            }

            bool IsValidEndpoint(const string& endpoint)
            {
                if (endpoint.empty() || (endpoint.size() > 253) || (endpoint[0] == '-'))
                    return false;

                struct in_addr v4;
                struct in6_addr v6;
                if ((inet_pton(AF_INET, endpoint.c_str(), &v4) == 1) || (inet_pton(AF_INET6, endpoint.c_str(), &v6) == 1))
                    return true;

                if (endpoint.find('%') != string::npos)
                    return IsValidIPv6WithZone(endpoint);

                return IsValidHostname(endpoint);
            }

            // "--" ends option parsing (busybox 1.35 ping/traceroute honour it); IsValidEndpoint() already rejects a leading '-'.
            std::vector<string> PingArgs(const bool ipv6, const uint32_t count, const uint16_t timeout, const string& endpoint)
            {
                return { (ipv6 ? "ping6" : "ping"), "-c", std::to_string(count), "-W", std::to_string(timeout), "-i", "0.002", "--", endpoint };
            }

            std::vector<string> TraceArgs(const bool ipv6, const uint32_t nqueries, const string& endpoint)
            {
                return { (ipv6 ? "traceroute6" : "traceroute"), "-w", "3", "-m", "6", "-q", std::to_string(nqueries), "--", endpoint, (ipv6 ? "64" : "52") };
            }

            std::chrono::milliseconds PingDeadline(const uint32_t count, const uint16_t timeout)
            {
                return Capped(static_cast<int64_t>(count) * timeout * 1000 + DeadlineMarginMs);
            }

            std::chrono::milliseconds TraceDeadline(const uint32_t nqueries)
            {
                return Capped(static_cast<int64_t>(nqueries) * TraceMsPerQuery + DeadlineMarginMs);
            }

            string PingResponse(const ProcessResult& result, const string& endpoint)
            {
                JsonObject pingResult;
                std::istringstream output(result.output);
                string line;
                while (std::getline(output, line))
                {
                    if( line.find( "packet" ) != string::npos )
                    {
                        //Example: 10 packets transmitted, 10 packets received, 0% packet loss
                        stringstream ss( line );
                        int transCount;
                        ss >> transCount;
                        pingResult["packetsTransmitted"] = transCount;

                        string token;
                        getline( ss, token, ',' );
                        getline( ss, token, ',' );
                        stringstream ss2( token );
                        int rxCount;
                        ss2 >> rxCount;
                        pingResult["packetsReceived"] = rxCount;

                        getline( ss, token, ',' );
                        string prefix = token.substr(0, token.find("%"));
                        pingResult["packetLoss"] = prefix.c_str();

                    }
                    else if( line.find( "min/avg/max" ) != string::npos )
                    {
                        //Example: round-trip min/avg/max = 17.038/18.310/20.197 ms
                        stringstream ss( line );
                        string fullpath;
                        getline( ss, fullpath, '=' );
                        getline( ss, fullpath, '=' );

                        string prefix;
                        int index = fullpath.find("/");
                        if (index >= 0)
                        {
                            prefix = fullpath.substr(0, fullpath.find("/"));
                            pingResult["tripMin"] = prefix.c_str();
                        }

                        index = fullpath.find("/");
                        if (index >= 0)
                        {
                            fullpath = fullpath.substr(index + 1, fullpath.length());
                            prefix = fullpath.substr(0, fullpath.find("/"));
                            pingResult["tripAvg"] = prefix.c_str();
                        }

                        index = fullpath.find("/");
                        if (index >= 0)
                        {
                            fullpath = fullpath.substr(index + 1, fullpath.length());
                            prefix = fullpath.substr(0, fullpath.find("/"));
                            pingResult["tripMax"] = prefix.c_str();
                        }

                        index = fullpath.find("/");
                        if (index >= 0)
                        {
                            fullpath = fullpath.substr(index + 1, fullpath.length());
                            pingResult["tripStdDev"] = fullpath.c_str();
                        }
                    }
                    else if( line.find( "bad" ) != string::npos )
                    {
                        pingResult["success"] = false;
                        pingResult["error"] = "Bad Address";
                    }
                }

                if (result.Succeeded()) {
                    pingResult["success"] = true;
                } else {
                    pingResult["success"] = false;
                    pingResult["error"] = "Could not ping endpoint";
                }
                pingResult["endpoint"] = endpoint;

                string response;
                pingResult.ToString(response);
                return response;
            }

            string TraceResponse(const ProcessResult& result, const string& endpoint)
            {
                // The output contains newlines, so it is returned as an array of lines; each complete line keeps a trailing space where its newline was.
                JsonArray list;
                size_t start = 0;
                while (start < result.output.size())
                {
                    const size_t newline = result.output.find('\n', start);
                    if (newline == string::npos)
                    {
                        list.Add(result.output.substr(start));
                        break;
                    }
                    list.Add(result.output.substr(start, newline - start) + " ");
                    start = newline + 1;
                }

                string lines;
                list.ToString(lines);

                JsonObject trace;
                trace["endpoint"] = endpoint;
                trace["results"] = lines;

                string response;
                trace.ToString(response);
                return response;
            }

            Engine::Engine(std::shared_ptr<IProcessRunner> runner)
                : _runner(std::move(runner))
                , _stop(false)
            {
                for (Lane& lane : _lanes)
                    lane.worker = std::thread(&Engine::Work, this, std::ref(lane));
            }

            Engine::~Engine()
            {
                Stop();
            }

            uint32_t Engine::Submit(const Kind kind, Job&& job)
            {
                Lane& lane = _lanes[static_cast<uint8_t>(kind)];
                std::lock_guard<std::mutex> guard(lane.lock);
                if (_stop.load())
                    return Core::ERROR_UNAVAILABLE;
                if (lane.queue.size() >= MaxQueued)
                    return Core::ERROR_INPROGRESS;
                lane.queue.push_back(std::move(job));
                lane.signal.notify_one();
                return Core::ERROR_NONE;
            }

            void Engine::Stop()
            {
                _stop.store(true);
                for (Lane& lane : _lanes)
                {
                    std::lock_guard<std::mutex> guard(lane.lock);
                    lane.signal.notify_all();
                }

                std::deque<Job> dropped;
                for (Lane& lane : _lanes)
                {
                    if (lane.worker.joinable())
                        lane.worker.join();
                    std::lock_guard<std::mutex> guard(lane.lock);
                    for (Job& job : lane.queue)
                        dropped.push_back(std::move(job));
                    lane.queue.clear();
                }
                for (Job& job : dropped)
                {
                    if (job.onDrop)
                        job.onDrop();
                }
            }

            void Engine::Work(Lane& lane)
            {
                while (true)
                {
                    Job job;
                    {
                        std::unique_lock<std::mutex> guard(lane.lock);
                        lane.signal.wait(guard, [&] { return _stop.load() || !lane.queue.empty(); });
                        if (_stop.load())
                            return;
                        job = std::move(lane.queue.front());
                        lane.queue.pop_front();
                    }

                    const ProcessResult result = _runner->Run(job.argv, job.deadline, _stop);
                    if (_stop.load())
                    {
                        if (job.onDrop)
                            job.onDrop();
                        return;
                    }
                    if (result.timedOut)
                        NMLOG_WARNING("%s killed after %lld ms deadline", job.argv[0].c_str(), static_cast<long long>(job.deadline.count()));
                    job.onDone(result);
                }
            }
        }
    }
}
