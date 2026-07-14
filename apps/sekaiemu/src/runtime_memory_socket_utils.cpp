#include "runtime_memory_socket_utils.hpp"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef DrawText
#undef DrawText
#endif
using NativeSocket = SOCKET;
#else
#include <cerrno>
#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>
using NativeSocket = int;
#endif

namespace sekaiemu::spike {

bool EnsureSocketRuntime() {
#if defined(_WIN32)
  static bool initialized = [] {
    WSADATA data{};
    return WSAStartup(MAKEWORD(2, 2), &data) == 0;
  }();
  return initialized;
#else
  static bool initialized = [] {
#if defined(SIGPIPE)
    signal(SIGPIPE, SIG_IGN);
#endif
    return true;
  }();
  (void)initialized;
  return true;
#endif
}

bool WouldBlock() {
#if defined(_WIN32)
  const int error = WSAGetLastError();
  return error == WSAEWOULDBLOCK;
#else
  return errno == EAGAIN || errno == EWOULDBLOCK;
#endif
}

void CloseFd(std::intptr_t fd) {
  if (fd >= 0) {
#if defined(_WIN32)
    closesocket(static_cast<SOCKET>(fd));
#else
    close(fd);
#endif
  }
}

bool SetNonBlocking(std::intptr_t fd) {
#if defined(_WIN32)
  u_long mode = 1;
  return ioctlsocket(static_cast<SOCKET>(fd), FIONBIO, &mode) == 0;
#else
  const int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0) {
    return false;
  }
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}

bool SendAll(std::intptr_t fd, const std::string& payload) {
  std::size_t sent = 0;
  while (sent < payload.size()) {
    const auto remaining = payload.size() - sent;
#if defined(_WIN32)
    constexpr int flags = 0;
#elif defined(MSG_NOSIGNAL)
    constexpr int flags = MSG_NOSIGNAL;
#else
    constexpr int flags = 0;
#endif
    const auto written = send(static_cast<NativeSocket>(fd),
                              payload.data() + sent,
                              static_cast<int>(remaining),
                              flags);
    if (written <= 0) {
      return false;
    }
    sent += static_cast<std::size_t>(written);
  }
  return true;
}

}  // namespace sekaiemu::spike
