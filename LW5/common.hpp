#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <mutex>

#pragma comment(lib, "Ws2_32.lib")

const uint32_t STUDENT_ID = 95;
const uint32_t SEED = 1000 + STUDENT_ID * 97; const uint16_t BASE_PORT = 20000 + STUDENT_ID;     
inline void LogMessage(const std::string& role, int id, const std::string& msg) {
    static std::mutex logMtx;
    std::lock_guard<std::mutex> lock(logMtx);

    SYSTEMTIME st;
    GetLocalTime(&st);
    printf("[%02d:%02d:%02d.%03d] %s", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, role.c_str());
    if (id >= 0) printf(" id=%d", id);
    printf(" | %s\n", msg.c_str());
}

inline void PrintWSAError(const std::string& funcName, const std::string& role, int id = -1) {
    int errCode = WSAGetLastError();
    char* msgBuf = nullptr;

    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, errCode, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
        (LPSTR)&msgBuf, 0, NULL
    );

    std::string errText = msgBuf ? msgBuf : "Unknown Winsock error";
    while (!errText.empty() && (errText.back() == '\n' || errText.back() == '\r')) {
        errText.pop_back();
    }

    LogMessage(role, id, "[ERROR] " + funcName + " failed | WSAGetLastError=" + std::to_string(errCode) + " (" + errText + ")");
    if (msgBuf) LocalFree(msgBuf);
}

inline bool SendAll(SOCKET sock, const char* data, int len, const std::string& role, int id) {
    int totalSent = 0;
    while (totalSent < len) {
        int sent = send(sock, data + totalSent, len - totalSent, 0);
        if (sent == SOCKET_ERROR) {
            PrintWSAError("send", role, id);
            return false;
        }
        if (sent == 0) {
            LogMessage(role, id, "Connection closed gracefully during send");
            return false;
        }
        totalSent += sent;
    }
    return true;
}

inline int RecvAll(SOCKET sock, char* buffer, int len, const std::string& role, int id) {
    int totalRecv = 0;
    while (totalRecv < len) {
        int received = recv(sock, buffer + totalRecv, len - totalRecv, 0);
        if (received == SOCKET_ERROR) {
            int err = WSAGetLastError();
            if (err == WSAETIMEDOUT) {
                LogMessage(role, id, "[TIMEOUT] recv timed out");
            }
            else {
                PrintWSAError("recv", role, id);
            }
            return -1;
        }
        if (received == 0) {
                        return (totalRecv == 0) ? 0 : -1;
        }
        totalRecv += received;
    }
    return 1;
}