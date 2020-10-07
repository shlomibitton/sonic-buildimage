#include "usock.h"

#include <swss/logger.h>

#include <arpa/inet.h>
#include <cerrno>
#include <string>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <stdexcept>
#include <cassert>

constexpr auto MaxBacklogConnections = 1;

USockSeqPacket::USockSeqPacket()
{
    int err;
    fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (fd < 0)
    {
        SWSS_LOG_THROW("socket(): failed to create socket: %s",
                strerror(errno));
    }
    initSocketBufferSize();
}

USockSeqPacket::USockSeqPacket(const std::string& usockpath) :
    USockSeqPacket()
{
    int err;
    sockaddr_un addr{};
    unlink(usockpath.c_str());
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, usockpath.c_str(), usockpath.length());
    err = bind(fd, reinterpret_cast<const sockaddr*>(&addr),
            sizeof(addr));
    if (err)
    {
        SWSS_LOG_THROW("bind(): failed to bind socket to %s: %s",
                usockpath.c_str(), strerror(errno));
    }
    err = listen(fd, MaxBacklogConnections);
    if (err)
    {
        SWSS_LOG_THROW("listen(): failed to listen on socket: %s",
                strerror(errno));
    }
}

USockSeqPacket::USockSeqPacket(int fd) : fd(fd)
{
    initSocketBufferSize();
}

USockSeqPacket::~USockSeqPacket()
{
    int err;
    if (fd == -1)
    {
        return;
    }
    err = close(fd);
    if (err)
    {
        SWSS_LOG_THROW("close(): failed to close socket %s",
                strerror(errno));
    }
    fd = -1;
}

void USockSeqPacket::initSocketBufferSize()
{
    // prepare buffer size variables for send/recv
    unsigned int paramsize = sizeof(socksndbuffsize);
    int err;
    err = getsockopt(fd, SOL_SOCKET, SO_SNDBUF,
            reinterpret_cast<void*>(&socksndbuffsize), &paramsize);
    if (err)
    {
        SWSS_LOG_THROW("getsockopt(): failed to get buffer size, %s",
                strerror(errno));
    }
    err = getsockopt(fd, SOL_SOCKET, SO_RCVBUF,
            reinterpret_cast<void*>(&sockrcvbuffsize), &paramsize);
    if (err)
    {
        SWSS_LOG_THROW("getsockopt(): failed to get buffer size, %s",
                strerror(errno));
    }
    // From "man 7 socket":
    // O_SNDBUF
    //    Sets or gets the maximum socket send buffer in bytes.
    //    The kernel doubles this value (to allow space for bookkeeping overhead)
    //    when it is set using setsockopt(2), and this doubled  value
    //    is  returned  by  getsockopt(2).
    // This is why we are dividng the buffer size by two:
    socksndbuffsize /= 2;
    sockrcvbuffsize /= 2;
}

int USockSeqPacket::getFd()
{
    return fd;
}

uint64_t USockSeqPacket::readData()
{
    // don't let Select to read data
    return 0;
}

std::unique_ptr<Connection> USockSeqPacket::accept()
{
    sockaddr addr{};
    socklen_t len = sizeof(addr);

    int clientfd = ::accept(fd, &addr, &len);
    if (clientfd < 0)
    {
        SWSS_LOG_THROW("accept(): failed to accept connection: %s",
                strerror(errno));
    }
    return std::make_unique<USockSeqPacket>(clientfd);
}

void USockSeqPacket::connect(const std::string& path)
{
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path));
    int err = ::connect(fd, reinterpret_cast<struct sockaddr*>(&addr),
            sizeof(addr));
    if (err)
    {
        SWSS_LOG_THROW("connect() failed to connect to daemon: %s",
                strerror(errno));
    }
}

bool USockSeqPacket::send(const std::string& data)
{
    ssize_t sent;
    size_t totalsent = 0;
    while (totalsent < data.size())
    {
        sent = ::send(fd, data.data() + totalsent,
                std::min(data.size() - totalsent,
                   static_cast<size_t>(socksndbuffsize)), 0);
        if (sent < 0)
        {
            if (errno == EINTR)
            {
                SWSS_LOG_DEBUG("send(): interrupted by a signal, ignore and proceed");
                continue;
            }
            else if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // timeout, exit
                SWSS_LOG_NOTICE("send(): client connection timeout");
                return false;
            }
            else
            {
                SWSS_LOG_ERROR("send(): failed to sent data to client: %s",
                        strerror(errno));
                return false;
            }
        }
        else
        {
            totalsent += sent;
        }
    }
    return true;
}

bool USockSeqPacket::recv(std::string& msg)
{
    ssize_t rcv;
    std::vector<unsigned char> buffer(sockrcvbuffsize);
    do
    {
        rcv = ::recv(fd, buffer.data(), buffer.size(), 0);
        if (rcv < 0)
        {
            if (errno == EINTR)
            {
                SWSS_LOG_DEBUG("recv(): interrupted by a signal, ignore and proceed");
                continue;
            }
            else if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // timeout, exit
                SWSS_LOG_NOTICE("recv(): client connection timeout");
                return false;
            }
            SWSS_LOG_ERROR("recv(): failed to read data from socket: %s",
                    strerror(errno));
            return false;
        }
        else if (rcv == 0)
        {
            // shutdown
        }
        buffer.resize(rcv);
    }
    while(false);
    msg = std::string(std::begin(buffer), std::end(buffer));
    return true;
}

void USockSeqPacket::setTimeout(int timeout)
{
    struct timeval tv;
    tv.tv_sec = timeout;
    tv.tv_usec = 0;

    for (auto opt: {SO_SNDTIMEO, SO_RCVTIMEO})
    {
        int err = setsockopt(fd, SOL_SOCKET, opt,
                reinterpret_cast<const char*>(&tv), sizeof tv);
        if (err == -1)
        {
            SWSS_LOG_THROW("setsockopt(): failed to set socket timeout: %s",
                    strerror(errno));
        }
    }
}
