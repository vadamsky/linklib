#ifndef SERVER_H
#define SERVER_H
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include <queue>

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

#include "tcpconnector.h"
#include "tcp_helper.h"


class Server : public TCPHelper
{
public:
    Server(TcpConnector *tcpconn);

    void init(std::string _port);
    void sockread();
    void sockwrite();

private:
    //! отправка str на клиент
    bool reply(const std::string &str, sockaddr_storage &peer_addr,
               socklen_t &peer_addr_len);

private:
    //! сокет для чтения/отправки
    int sock;

    // служебные
    sockaddr_storage _sockaddr_storage;
    std::vector<socklen_t>        peer_addr_lens;
    struct sockaddr_storage their_addr;
    socklen_t their_addr_size = sizeof(their_addr);
};

#endif // SERVER_H
