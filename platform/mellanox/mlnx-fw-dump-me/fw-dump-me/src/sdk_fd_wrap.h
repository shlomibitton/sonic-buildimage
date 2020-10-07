#include <swss/selectable.h>

class HealthFD : virtual public swss::Selectable
{
public:
    HealthFD(int fd);
    int getFd() override;
    uint64_t readData() override;
    ~HealthFD() = default;
private:
    int fd;
};
