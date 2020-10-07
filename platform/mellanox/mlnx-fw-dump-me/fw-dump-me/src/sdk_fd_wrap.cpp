#include "sdk_fd_wrap.h"

HealthFD::HealthFD(int fd)
{
    this->fd = fd;
}

int HealthFD::getFd()
{
    return this->fd;
}

uint64_t HealthFD::readData()
{
    // don't let Select to read data
    return 0;
}
