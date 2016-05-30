#include "server.h"

#include "../../BaseMessages/baseengine_low.h"
#include "../../BaseMessages/baseengine.h"

#include "message_send_helper.h"


Server::Server(TcpConnector *tcpconn) :
    TCPHelper(tcpconn)
{
    // порт, на который повесим сокет для входящих соединений
    std::string port = std::to_string(std::stoi(PORT) +
                                    static_cast<int>(tcpconn->getNoiOp().tn));
    tconn = new std::thread(&Server::init, this, port);
    handler_tconn = tconn->native_handle();
    tconn->detach();
}


void Server::init(std::string _port)
{
    // предварительные прыжки с бубном, чтобы
    // создать сокет на аксепт
    struct addrinfo hints;
    struct addrinfo *rp;
    int s;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    s = getaddrinfo(NULL, _port.c_str(), &hints, &result);
    if (s != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        exit(EXIT_FAILURE);
    }
    struct sockaddr_in *addr_in_ptr = (sockaddr_in*)result->ai_addr;
    std::cout << "IP: " << inet_ntoa(addr_in_ptr->sin_addr) << std::endl;
    // закончили прыгать

    // загрузка неотправленных сообщений этим
    // НИЦ-ем только(!) текущему серверу/клиенту

    ///load_not_sent(connector, _write_msgs);

    int *p_int;
    p_int = (int*)malloc(sizeof(int));
    *p_int = 1;

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        // создаем сокет
        sfd = socket(rp->ai_family, rp->ai_socktype,
             0);//rp->ai_protocol);
        // если не создало
        if (sfd == -1){
            printf("Error opening socket = %d", errno);
            continue;
        }
//SOCK_STREAM
        // форсим сокет на реюз
        int enable = 1;
        if(setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, /*(char*)p_int*/&enable, sizeof(int)) < 0){
            printf("setsockopt(SO_REUSEADDR) failed = %d", errno);
            continue;
        }

        // пытаемся биндить
        int start = bind(sfd, rp->ai_addr, sizeof(sockaddr)/*rp->ai_addrlen*/);
        if (start < 0)
            printf("Socket bind failed = %d\n", errno);
        if (start == 0)
            break;                  /* Success */
//EINVAL
        close(sfd);
    }

    // если не удалось создать нужный сокет
    if (rp == NULL) {
        fprintf(stderr, "Could not bind\n");
        exit(EXIT_FAILURE);
    }

    // очередь входящих соединений
    listen(sfd, 5);
    isStopped = false;
    connectionLost = true;
    // если нужно остановить поток tconn
    // (по факту меняется только в деструкторе)
    while(!isStopped)
    {
        mutexThreadKilled.lock();
        // если за время mutex::lock() пришла инфа о том,
        // что надо остановить поток tconn
        if(isStopped) break;
        // если соединение живое, незачем нам дальше ходить
        if(!connectionLost)
        {
            mutexThreadKilled.unlock();
            usleep(200000); // reconnect
            continue;
        }

//        usleep(10000);                  /// ???
        their_addr_size = sizeof(their_addr);

        // пытаемся создать сокет на аксепт пакетов
        sock = accept(sfd, (struct sockaddr*)&their_addr, &their_addr_size);
        char host[NI_MAXHOST], service[NI_MAXSERV];
        getnameinfo((struct sockaddr *)&their_addr, their_addr_size, host,
                    NI_MAXHOST, service, NI_MAXSERV, NI_NUMERICHOST);
        // если не создало
        if(sock < 0)
        {
            mutexThreadKilled.unlock();
            continue;
        }
        // если создало
        else
        {
            isStopped = true;
            //usleep(50000);
            // если есть старые потоки, прибьем их
            if(twrite)
            {
                try{ twrite->join(); }
                catch(...){}
                delete twrite;
                twrite = 0;
            }
            if(tread)
            {
                try{ tread->join(); }
                catch(...){}
                delete tread;
                tread = 0;
            }
            // создаем новые потоки
            isStopped = false;
            tread = new std::thread(&Server::sockread, this);
            twrite = new std::thread(&Server::sockwrite, this);
            // запомним хандлеры
            handler_tread = tread->native_handle();
            handler_twrite = twrite->native_handle();
            tread->detach();
            twrite->detach();
            //mutexThread.unlock();
            connectionLost = false;
            mutexThreadKilled.unlock();
        }
        mutexThreadKilled.unlock();
        //usleep(20000);
    }

    freeaddrinfo(result);           /* No longer needed */
}


void Server::sockread()
{
    ssize_t nread;
    char buf[BUF_SIZE + 1];

    struct in_addr ip_addr;
    while(!isStopped){
        // select
        fd_set rfds;
        struct timeval tv;
        int retval;

        FD_ZERO(&rfds);
        FD_SET(0, &rfds);

        // select timeout = 0.01 sec
        tv.tv_sec = 0;
        tv.tv_usec = 10000;

        // ЗЫ: select вываливается сразу по приходу данных,
        // если они есть, а не ждет таймаута
        retval = select(1, &rfds, NULL, NULL, &tv);
        // если что-то не так с селектом
        if(retval == -1)
            nread = -1;
        // если есть данные
        else if(retval > -1)
            nread = recv(sock, buf, BUF_SIZE + 1, MSG_DONTWAIT);
        // если вывалились по таймауту
        else
            nread = -1;
        //// думаю, это надо убрать отсюда, ибо могут прийти
        //// данные, мы с ними ничего не сделаем, а узел
        //// с другой стороны будет считать их полученными
        if(isStopped)
            break;
        // если считали ерунду или не считали ничего
        if (nread == -1)
            continue;
        // если соединение разорвано
        else if(nread == 0)
        {
            mutexThreadKilled.lock();
            connectionLost = true;
            mutexThreadKilled.unlock();
            return;
        }

        char host[NI_MAXHOST], service[NI_MAXSERV];

        getnameinfo((struct sockaddr *)&their_addr, their_addr_size, host,
                    NI_MAXHOST, service, NI_MAXSERV, NI_NUMERICHOST);

        ip_addr.s_addr = their_addr.__ss_align;
        std::string ip = inet_ntoa(ip_addr);
        std::string port(service);
        ip = connector->getNoiOp().ip;
        // парсинг пришедших данных
        recvData(buf, nread, ip, port);
    }
}




void Server::sockwrite()
{
    //mutexThread.lock();
    socklen_t slen = sizeof(struct sockaddr_storage);
    while(!isStopped){
        _write_msg_mutex.lock();
        // если есть, что отправить
        if(!(_write_msgs.empty()))
        {
            // возьмем первое сообщение
            const std::string& _write_msg = _write_msgs.front().first;
            bool sent = true;
            mutexThreadKilled.lock();
            // если связь оборвана, нечего и пытаться
            // отослать что-нибудь
            if(connectionLost)
            {
                close(sock);
                _write_msg_mutex.unlock();
                mutexThreadKilled.unlock();
                //mutexThread.unlock();
                return;
            }
            // собственно, отсылка сообщения
            if(!reply(_write_msg, _sockaddr_storage, slen))
                sent = false;
            // если все хорошо отправилось
            if(sent)
            {
                // поменяем DELIVERY у этого сообщения
                ///make_sent(be, bd, _write_msgs.front().second); // грузит проц
                // и уберем его из очереди на отправку
                ++num_write;
                _write_msgs.pop();
            }
            mutexThreadKilled.unlock();
        }
        _write_msg_mutex.unlock();
        usleep(10000);
    }
    //mutexThread.unlock();
}


bool Server::reply(const std::string &str, sockaddr_storage &peer_addr,
                   socklen_t &peer_addr_len)
{
    // если отправились не все данные
    if(sendto(sock, str.c_str(), str.size() + 1, MSG_NOSIGNAL,
              (struct sockaddr *) &peer_addr, peer_addr_len) !=
            (int)(str.size() + 1))
    {
        /// мб стоит ифдеф здесь прописать?
        printf("Error sending response\n");
        return false;
    }
    return true;
}
