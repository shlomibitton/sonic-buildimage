#include "usock.h"

#include <unistd.h>
#include <swss/logger.h>
#include <swss/select.h>
#include <iostream>
#include <string>
#include <stdexcept>
#include <unistd.h>
#include <stdlib.h>

using namespace std;

static const auto DefaultSocketPath    = "/var/run/fw_dump_me/fw.sock";
static const auto DefaultTimeout       = 10; // s

int main(int argc, char** argv)
{
    /* Create UNIX socket and connect to daemon */
    USockSeqPacket sock{};
    try
    {
    sock.setTimeout(DefaultTimeout);
    sock.connect(DefaultSocketPath);
    }
    catch(const exception& exception)
    {
        cerr << exception.what() << endl;
        return EXIT_FAILURE;
    }

    /* Select params */
    swss::Select select{};
    int rc {swss::Select::ERROR};
    swss::Selectable* currentSelectable {nullptr};
    select.addSelectable(&sock);

    /* Send request to daemon */
    string request = argv[1];
    if (!sock.send(request))
    {
        cout << "Failed to send request to daemon" << endl;
    }
    
    /* Wait for daemon reply */
    rc = select.select(&currentSelectable, DefaultTimeout * 1000);
    if (rc == swss::Select::ERROR)
    {
        cout << "Select returned error " << rc << endl;
    }
    else if (rc == swss::Select::TIMEOUT)
    {
        cout << "Timeout reached for daemon reply" << endl;
    }
    else if (rc != swss::Select::OBJECT)
    {
        cout << "Select unexpected return" << endl;
    }

    /* Print daemon reply */
    string recvmsg;
    if (!sock.recv(recvmsg) || recvmsg.empty())
    {
        cout << "Failed to receive reply from daemon" << endl;
    }
    cout << recvmsg;

    return EXIT_SUCCESS;
}
