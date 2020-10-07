#pragma once

#include <swss/selectable.h>

#include <memory>
#include <string>
#include <iostream>

class Connection : virtual public swss::Selectable
{
public:
    virtual ~Connection() = default;
    virtual bool send(const std::string&) = 0;
    virtual bool recv(std::string&) = 0;
    virtual void setTimeout(int timeout) = 0;
};

class Listener : virtual public swss::Selectable
{
public:
    virtual ~Listener() = default;
    virtual std::unique_ptr<Connection> accept() = 0;
};

class USockSeqPacket : public Connection,
                       public Listener
{
public:
    // create SOCK_SEQPACKET socket
    USockSeqPacket();
    // create seqpacket socket and bind to usockpath
    USockSeqPacket(const std::string& usockpath);
    // create a client socket from fd
    USockSeqPacket(int fd);
    USockSeqPacket(const USockSeqPacket&) = delete;
    USockSeqPacket& operator=(const USockSeqPacket&) = delete;
    ~USockSeqPacket() override;

    int getFd() override;
    uint64_t readData() override;

    std::unique_ptr<Connection> accept() override;
    void connect(const std::string& path);
    bool send(const std::string&) override;
    bool recv(std::string&) override;
    void setTimeout(int timeout) override;
private:
    void initSocketBufferSize();

    int fd {-1};
    int socksndbuffsize {-1};
    int sockrcvbuffsize {-1};
};
