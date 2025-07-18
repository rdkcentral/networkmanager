#include <iostream>
#include <string>
#include <cstring>
#include <thread>
#include <netinet/in.h>
#include <unistd.h>
#include <vector>

class SimpleHttpServer {
public:
    SimpleHttpServer(unsigned short port = 8080)
        : m_port(port), m_running(false), m_httpResponseCode(204), m_httpMessage(""), m_timeoutMs(0) {}

    ~SimpleHttpServer() {
        stop();
    }

    void setHttpResponseCode(int code) {
        m_httpResponseCode = code;
    }

    void setHttpMessage(const std::string& message) {
        m_httpMessage = message;
    }
    
    void setTimeout(unsigned int timeoutMs) {
        m_timeoutMs = timeoutMs;
    }

    void start() {
        m_running = true;
        m_serverThread = std::thread([this]() { run(); });
    }

    void stop() {
        m_running = false;
        if (m_serverThread.joinable()) {
            m_serverThread.join();
        }
    }

private:
    void run() {
        int server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd == -1) {
            std::cerr << "Failed to create socket" << std::endl;
            return;
        }

        // Set SO_REUSEADDR to allow immediate reuse of the port
        int opt = 1;
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            std::cerr << "Failed to set socket options" << std::endl;
            close(server_fd);
            return;
        }

        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(m_port);

        if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            std::cerr << "Failed to bind socket" << std::endl;
            close(server_fd);
            return;
        }

        if (listen(server_fd, 5) < 0) {
            std::cerr << "Failed to listen on socket" << std::endl;
            close(server_fd);
            return;
        }

        std::cout << "Server running on http://localhost:" << m_port << "/generate_204" << std::endl;

        while (m_running) {
            sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);
            int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
            if (client_fd < 0) {
                std::cerr << "Failed to accept connection" << std::endl;
                continue;
            }

            handleRequest(client_fd);
            close(client_fd);
        }

        close(server_fd);
    }

    void handleRequest(int client_fd) {
        // If timeout is set, simulate a timeout by sleeping
        if (m_timeoutMs > 0) {
            // Break timeout into 100ms chunks so we can check for stop conditions
            const unsigned int chunkSize = 100; // 100ms chunks
            unsigned int remaining = m_timeoutMs;
            
            while (m_running && remaining > 0) {
                // Sleep for chunkSize or remaining time, whichever is smaller
                unsigned int sleepTime = std::min(chunkSize, remaining);
                std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));
                remaining -= sleepTime;
                
                // If stop() was called during the sleep, exit immediately
                if (!m_running) {
                    break;
                }
            }
            
            // Return without sending a response to simulate timeout
            return;
        }

        char buffer[1024] = {0};
        read(client_fd, buffer, sizeof(buffer));

        std::string response;

        switch (m_httpResponseCode) {
            case 204: // No Content
                response = "HTTP/1.1 204 No Content\r\n";
                response += "Content-Length: 0\r\n";
                response += "Date: " + getCurrentDate() + "\r\n";
                response += "\r\n";
                break;

            case 302: // Found (Redirect)
                response = "HTTP/1.1 302 Found\r\n";
                response += "Content-Length: " + std::to_string(m_httpMessage.size()) + "\r\n";
                response += "Location: " + m_httpMessage + "\r\n";
                response += "Date: " + getCurrentDate() + "\r\n";
                response += "\r\n";
                response += m_httpMessage; // Add the URI as the body
                break;

            case 200: // OK
                response = "HTTP/1.1 200 OK\r\n";
                response += "Content-Length: 0\r\n";
                response += "Date: " + getCurrentDate() + "\r\n";
                response += "\r\n";
                response += m_httpMessage;
                break;

            case 404: // Not Found
                response = "HTTP/1.1 404 Not Found\r\n";
                response += "Content-Length: 0\r\n";
                response += "Date: " + getCurrentDate() + "\r\n";
                response += "\r\n";
                break;

            case 511: // Authentication Required (Redirect as 302)
                response = "HTTP/1.1 302 Found\r\n";
                response += "Content-Length: " + std::to_string(m_httpMessage.size()) + "\r\n";
                response += "Location: " + m_httpMessage + "\r\n";
                response += "Date: " + getCurrentDate() + "\r\n";
                response += "\r\n";
                response += m_httpMessage; // Add the URI as the body
                break;

            default: // Default case for unsupported codes
                response = "HTTP/1.1 500 Internal Server Error\r\n";
                response += "Content-Length: 0\r\n";
                response += "Date: " + getCurrentDate() + "\r\n";
                response += "\r\n";
                break;
        }

        send(client_fd, response.c_str(), response.size(), 0);
    }

    std::string getCurrentDate() {
        std::time_t now = std::time(nullptr);
        char buf[100];
        std::strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", std::gmtime(&now));
        return std::string(buf);
    }

    unsigned short m_port;
    std::thread m_serverThread;
    bool m_running;
    int m_httpResponseCode;
    std::string m_httpMessage;
    unsigned int m_timeoutMs;
};

// int main() {
//     SimpleHttpServer server(8080);

//     // Test all possible response codes
//     std::vector<std::pair<int, std::string>> testCases = {
//         {204, ""}, // No Content
//         {302, "http://example.com/redirect"}, // Found (Redirect)
//         {200, "OK"}, // OK
//         {404, ""}, // Not Found
//         {511, "http://captive.portal/login"} // Authentication Required (Redirect as 302)
//     };

//     for (const auto& testCase : testCases) {
//         server.setHttpResponseCode(testCase.first);
//         server.setHttpMessage(testCase.second);

//         std::cout << "Testing HTTP response code: " << testCase.first << std::endl;
//         server.start();

//         std::this_thread::sleep_for(std::chrono::seconds(5)); // Wait for 5 seconds to test
//         server.stop();
//         std::this_thread::sleep_for(std::chrono::seconds(1));
//     }

//     // Test timeout functionality
//     std::cout << "Testing timeout functionality..." << std::endl;
//     server.setTimeout(5000); // Set a 5000ms timeout
//     server.start();
//     std::this_thread::sleep_for(std::chrono::seconds(10)); // Wait longer than the timeout
//     server.stop();

//     return 0;
// }