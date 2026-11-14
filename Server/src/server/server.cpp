#include "../common/common.hpp"
#include <atomic>
#include <thread>

std::atomic<bool> g_ServerRunning(true);
std::atomic<int>  g_ClientIdCounter(1);
SOCKET g_ListenSocket = INVALID_SOCKET;

BOOL WINAPI ConsoleHandler(DWORD ctrlType) {
    if (ctrlType == CTRL_C_EVENT || ctrlType == CTRL_CLOSE_EVENT) {
        LogMessage("SERVER", -1, "Shutdown signal received. Closing listen socket...");
        g_ServerRunning = false;
        if (g_ListenSocket != INVALID_SOCKET) {
            closesocket(g_ListenSocket);
            g_ListenSocket = INVALID_SOCKET;
        }
        return TRUE;
    }
    return FALSE;
}

void HandleClient(SOCKET clientSock, std::string clientIP, uint16_t clientPort, int clientId, int timeoutMs) {
    std::string addrStr = clientIP + ":" + std::to_string(clientPort);
    LogMessage("SERVER", clientId, "CLIENT_CONNECTED from " + addrStr);

        DWORD tv = (DWORD)timeoutMs;
    setsockopt(clientSock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    setsockopt(clientSock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));

    const int BUF_SIZE = 65536;
    std::vector<char> buffer(BUF_SIZE);
    uint64_t totalBytesEchoed = 0;
    uint64_t packetCount = 0;

    while (g_ServerRunning) {
        int bytesReceived = recv(clientSock, buffer.data(), BUF_SIZE, 0);

        if (bytesReceived > 0) {
            packetCount++;
            totalBytesEchoed += bytesReceived;

                        if (!SendAll(clientSock, buffer.data(), bytesReceived, "SERVER", clientId)) {
                break;
            }

                        if (packetCount <= 3 || packetCount % 100 == 0) {
                LogMessage("SERVER", clientId, "ECHO chunk #" + std::to_string(packetCount) +
                    " | size=" + std::to_string(bytesReceived) + " bytes | status=OK");
            }
        }
        else if (bytesReceived == 0) {
                        LogMessage("SERVER", clientId, "CLIENT_DISCONNECTED gracefully | Total echoed=" +
                std::to_string(totalBytesEchoed) + " bytes");
            break;
        }
        else {
            int err = WSAGetLastError();
            if (err == WSAETIMEDOUT) {
                LogMessage("SERVER", clientId, "Timeout waiting for client data. Closing connection.");
            }
            else if (err == WSAECONNRESET) {
                LogMessage("SERVER", clientId, "[WARNING] Connection reset forcibly by client (WSAECONNRESET)");
            }
            else if (g_ServerRunning) {
                PrintWSAError("recv", "SERVER", clientId);
            }
            break;
        }
    }

    shutdown(clientSock, SD_BOTH);
    closesocket(clientSock);
    LogMessage("SERVER", clientId, "Socket closed.");
}

int main(int argc, char* argv[]) {
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);

    std::string host = "127.0.0.1";
    uint16_t port = BASE_PORT;     int timeoutMs = 15000;     
        for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--host" && i + 1 < argc) host = argv[++i];
        else if (arg == "--port" && i + 1 < argc) port = (uint16_t)std::stoi(argv[++i]);
        else if (arg == "--timeout" && i + 1 < argc) timeoutMs = std::stoi(argv[++i]);
    }

        WSADATA wsaData;
    int wsaRes = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsaRes != 0) {
        std::cerr << "WSAStartup failed with error: " << wsaRes << std::endl;
        return 1;
    }

        g_ListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_ListenSocket == INVALID_SOCKET) {
        PrintWSAError("socket", "SERVER");
        WSACleanup();
        return 1;
    }

        BOOL opt = TRUE;
    setsockopt(g_ListenSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

        sockaddr_in serverAddr = { 0 };
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &serverAddr.sin_addr) <= 0) {
        LogMessage("SERVER", -1, "[ERROR] Invalid host IP address: " + host);
        closesocket(g_ListenSocket);
        WSACleanup();
        return 1;
    }

    if (bind(g_ListenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        PrintWSAError("bind", "SERVER");
        closesocket(g_ListenSocket);
        WSACleanup();
        return 1;
    }

        if (listen(g_ListenSocket, SOMAXCONN) == SOCKET_ERROR) {
        PrintWSAError("listen", "SERVER");
        closesocket(g_ListenSocket);
        WSACleanup();
        return 1;
    }

    LogMessage("SERVER", -1, "SERVER START (RFC 862 TCP Echo) | host=" + host +
        " | port=" + std::to_string(port) + " | timeout=" + std::to_string(timeoutMs) + "ms");
    LogMessage("SERVER", -1, "Press Ctrl+C to gracefully stop the server.");

    std::vector<std::thread> clientThreads;

        while (g_ServerRunning) {
        sockaddr_in clientAddr = { 0 };
        int addrLen = sizeof(clientAddr);

        SOCKET clientSock = accept(g_ListenSocket, (sockaddr*)&clientAddr, &addrLen);
        if (clientSock == INVALID_SOCKET) {
            if (g_ServerRunning) {
                PrintWSAError("accept", "SERVER");
            }
            break;
        }

        char ipStr[INET_ADDRSTRLEN] = { 0 };
        inet_ntop(AF_INET, &clientAddr.sin_addr, ipStr, INET_ADDRSTRLEN);
        uint16_t clientPort = ntohs(clientAddr.sin_port);
        int id = g_ClientIdCounter++;

                clientThreads.emplace_back(HandleClient, clientSock, std::string(ipStr), clientPort, id, timeoutMs);
    }

        for (auto& t : clientThreads) {
        if (t.joinable()) t.join();
    }

    if (g_ListenSocket != INVALID_SOCKET) closesocket(g_ListenSocket);
    WSACleanup();
    LogMessage("SERVER", -1, "SERVER STOPPED cleanly.");
    return 0;
}