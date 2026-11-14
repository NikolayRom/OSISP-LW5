#include "../common/common.hpp"
#include <random>
#include <vector>
#include <cstring>
#include <algorithm>

struct ExperimentResult {
    double totalTimeMs;
    double avgRttMs;
    double throughputMBps;
    uint64_t totalBytes;
    int errors;
};

ExperimentResult RunEchoSession(const std::string& host, uint16_t port, int clientId,
    int blockSize, int cycles, int timeoutMs, bool chunkedSendTest) {
    ExperimentResult res = { 0, 0, 0, 0, 0 };

        SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        PrintWSAError("socket", "CLIENT", clientId);
        res.errors++;
        return res;
    }

        DWORD tv = (DWORD)timeoutMs;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));

        BOOL nodelay = TRUE;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&nodelay, sizeof(nodelay));

        sockaddr_in serverAddr = { 0 };
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &serverAddr.sin_addr) <= 0) {
        LogMessage("CLIENT", clientId, "[ERROR] Invalid IP: " + host);
        closesocket(sock);
        res.errors++;
        return res;
    }

    if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        PrintWSAError("connect", "CLIENT", clientId);
        closesocket(sock);
        res.errors++;
        return res;
    }

    LogMessage("CLIENT", clientId, "CONNECTED to " + host + ":" + std::to_string(port) +
        " | block=" + std::to_string(blockSize) + "B | cycles=" + std::to_string(cycles));

        std::vector<char> sendBuf(blockSize);
    std::vector<char> recvBuf(blockSize);
    std::mt19937 rng(SEED + clientId);
    std::uniform_int_distribution<int> byteDist(0, 255);

    for (int i = 0; i < blockSize; ++i) {
        sendBuf[i] = static_cast<char>(byteDist(rng));
    }
        if (blockSize >= 10) {
        sendBuf[0] = '\0';
        sendBuf[blockSize / 2] = '\0';
        sendBuf[blockSize - 1] = '\0';
    }

    LARGE_INTEGER freq, startTotal, endTotal;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&startTotal);

        for (int c = 1; c <= cycles; ++c) {
                sendBuf[1 % blockSize] = static_cast<char>(c & 0xFF);

        if (chunkedSendTest && blockSize >= 512) {
                        int sent = 0;
            while (sent < blockSize) {
                int chunk = std::min(333, blockSize - sent);
                if (!SendAll(sock, sendBuf.data() + sent, chunk, "CLIENT", clientId)) {
                    res.errors++;
                    break;
                }
                sent += chunk;
            }
        }
        else {
            if (!SendAll(sock, sendBuf.data(), blockSize, "CLIENT", clientId)) {
                res.errors++;
                break;
            }
        }

        if (res.errors > 0) break;

                int recvStatus = RecvAll(sock, recvBuf.data(), blockSize, "CLIENT", clientId);
        if (recvStatus <= 0) {
            res.errors++;
            break;
        }

                if (std::memcmp(sendBuf.data(), recvBuf.data(), blockSize) != 0) {
            LogMessage("CLIENT", clientId, "[ERROR] Data corruption detected at cycle #" + std::to_string(c));
            res.errors++;
            break;
        }

        res.totalBytes += blockSize;
    }

    QueryPerformanceCounter(&endTotal);

        shutdown(sock, SD_SEND);
    closesocket(sock);

    res.totalTimeMs = (double)(endTotal.QuadPart - startTotal.QuadPart) * 1000.0 / freq.QuadPart;
    res.avgRttMs = res.totalTimeMs / cycles;
        double totalMB = (res.totalBytes * 2.0) / (1024.0 * 1024.0);
    res.throughputMBps = totalMB / (res.totalTimeMs / 1000.0);

    LogMessage("CLIENT", clientId, "DISCONNECTED | Verified cycles=" + std::to_string(cycles) +
        " | Errors=" + std::to_string(res.errors) +
        " | Time=" + std::to_string(res.totalTimeMs) + " ms");

    return res;
}

int main(int argc, char* argv[]) {
    std::string host = "127.0.0.1";
    uint16_t port = BASE_PORT;     int clientId = 1;
    int blockSize = 64;
    int cycles = 100;
    int timeoutMs = 5000;
    bool runBenchmark = false;
    bool testChunked = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--host" && i + 1 < argc) host = argv[++i];
        else if (arg == "--port" && i + 1 < argc) port = (uint16_t)std::stoi(argv[++i]);
        else if (arg == "--id" && i + 1 < argc) clientId = std::stoi(argv[++i]);
        else if (arg == "--size" && i + 1 < argc) blockSize = std::stoi(argv[++i]);
        else if (arg == "--cycles" && i + 1 < argc) cycles = std::stoi(argv[++i]);
        else if (arg == "--timeout" && i + 1 < argc) timeoutMs = std::stoi(argv[++i]);
        else if (arg == "--benchmark") runBenchmark = true;
        else if (arg == "--chunked") testChunked = true;
    }

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed!\n";
        return 1;
    }

    if (!runBenchmark) {
                ExperimentResult r = RunEchoSession(host, port, clientId, blockSize, cycles, timeoutMs, testChunked);
        printf("\n=== SUMMARY (Client ID=%d) ===\n", clientId);
        printf("Total Time:   %.2f ms\n", r.totalTimeMs);
        printf("Avg RTT:      %.4f ms\n", r.avgRttMs);
        printf("Throughput:   %.2f MB/s\n", r.throughputMBps);
        printf("Data Errors:  %d\n", r.errors);
    }
    else {
                struct Config { int size; int cyc; std::string name; };
        std::vector<Config> configs = {
            { 64,   100, "A (64B, 100 cyc)"   },
            { 64,   500, "B (64B, 500 cyc)"   },
            { 4096, 100, "C (4096B, 100 cyc)" },
            { 4096, 500, "D (4096B, 500 cyc)" }
        };

        printf("\n===========================================================================================\n");
        printf("  BENCHMARK SUITE (RFC 862 TCP Echo | STUDENT_ID=%d | SEED=%d | PORT=%d)\n", STUDENT_ID, SEED, port);
        printf("===========================================================================================\n");
        printf("%-20s | %-8s | %-8s | %-8s | %-10s | %-10s | %-10s\n",
            "Configuration", "Run 1", "Run 2", "Run 3", "Avg (ms)", "RTT (ms)", "MB/s");
        printf("-------------------------------------------------------------------------------------------\n");

        for (const auto& cfg : configs) {
            std::vector<double> times(3);
            double sumRtt = 0, sumSpeed = 0;
            int totalErrors = 0;

            for (int r = 0; r < 3; ++r) {
                ExperimentResult res = RunEchoSession(host, port, clientId, cfg.size, cfg.cyc, timeoutMs, true);
                times[r] = res.totalTimeMs;
                sumRtt += res.avgRttMs;
                sumSpeed += res.throughputMBps;
                totalErrors += res.errors;
            }

            double avgT = (times[0] + times[1] + times[2]) / 3.0;
            printf("%-20s | %-8.2f | %-8.2f | %-8.2f | %-10.2f | %-10.4f | %-10.2f (Err=%d)\n",
                cfg.name.c_str(), times[0], times[1], times[2], avgT, sumRtt / 3.0, sumSpeed / 3.0, totalErrors);
        }
        printf("===========================================================================================\n");
    }

    WSACleanup();
    return 0;
}