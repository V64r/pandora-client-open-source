#include <winsock2.h>
#include <windows.h>
#include <vector>
#include <mutex>
#include <atomic>
#include <limits>
#include "../hooks.hpp"

#pragma comment(lib, "ws2_32.lib")

namespace features::latency::blink { extern std::atomic_bool is_blinking; }
namespace network_hooks {
    struct BufferedPacket {
        SOCKET sock = INVALID_SOCKET;
        std::vector<char> data;
        DWORD flags = 0;
    };

    static std::vector<BufferedPacket> blink_vault;
    static std::mutex vault_mutex;

    typedef int(WSAAPI* WSASend_t)(SOCKET, LPWSABUF, DWORD, LPDWORD, DWORD, LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE);
    WSASend_t original_WSASend = nullptr;

    static void drain_packets()
    {
        std::vector<BufferedPacket> packets;
        {
            std::lock_guard<std::mutex> lock(vault_mutex);
            packets.swap(blink_vault);
        }
        if (!original_WSASend) return;

        for (auto& packet : packets) {
            size_t offset = 0;
            while (offset < packet.data.size()) {
                const size_t remaining = packet.data.size() - offset;
                WSABUF buffer{};
                buffer.buf = packet.data.data() + offset;
                buffer.len = static_cast<ULONG>((std::min)(remaining,
                    static_cast<size_t>((std::numeric_limits<ULONG>::max)())));
                DWORD sent = 0;
                if (original_WSASend(packet.sock, &buffer, 1, &sent,
                    packet.flags, nullptr, nullptr) == SOCKET_ERROR || sent == 0)
                    break;
                offset += sent;
            }
        }
    }

    void flush_blink_packets() { drain_packets(); }

    int WSAAPI hooked_WSASend(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesSent, DWORD dwFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
    {
        hooks::callback_guard callback;
        if (original_WSASend == nullptr) {
            WSASetLastError(WSAENETDOWN);
            return SOCKET_ERROR;
        }

        // 1. Blink
        if (features::latency::blink::is_blinking.load(std::memory_order_acquire)) {
            if (lpOverlapped == nullptr && lpCompletionRoutine == nullptr) {
                std::lock_guard<std::mutex> lock(vault_mutex);
                BufferedPacket packet;
                packet.sock = s;
                packet.flags = dwFlags;
                size_t total = 0;
                for (DWORD i = 0; i < dwBufferCount; ++i) total += lpBuffers[i].len;
                packet.data.reserve(total);
                for (DWORD i = 0; i < dwBufferCount; ++i)
                    packet.data.insert(packet.data.end(), lpBuffers[i].buf,
                        lpBuffers[i].buf + lpBuffers[i].len);
                blink_vault.push_back(std::move(packet));
                if (lpNumberOfBytesSent != nullptr) {
                    *lpNumberOfBytesSent = static_cast<DWORD>((std::min)(total,
                        static_cast<size_t>((std::numeric_limits<DWORD>::max)())));
                }
                return 0;
            }
        }
        else {
            flush_blink_packets();
        }

        return original_WSASend(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpOverlapped, lpCompletionRoutine);
    }
}
