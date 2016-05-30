#ifndef CLIENT_H
#define CLIENT_H

#include <mutex>
#include <vector>
#include <thread>
#include <queue>

#include "tcpconnector.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "../../Kernel/GeneratedFiles/gen_moduleinfo.h"
#ifdef TESTING
#include "../../Kernel/testskeleton_modulemanager.h"
#else
#include "../../Kernel/modulemanager.h"
#endif
#include "../../Functions/functions_text_std.h"
#include "../../Functions/LoaderConfAndConverters/LoaderConfFromBase.h"

#include "tcp_helper.h"


class Client : public TCPHelper
{
public:
    Client(TcpConnector *tcpconn);
    void init(const std::string &port);
    void sockread();
    void sockwrite();

private:
    //! порт сервера, к которому будет происходить коннект
    // (ip нет, т.к. его мы знаем из TcpConnector-а)
    std::string dest_port;

};


#endif // CLIENT_H
