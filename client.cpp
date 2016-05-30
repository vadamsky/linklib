#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <string>
#include <pthread.h>

#include "client.h"

#include "../../BaseMessages/baseengine_low.h"
#include "../../BaseMessages/baseengine.h"

#include "message_send_helper.h"


Client::Client(TcpConnector *tcpconn) :
    TCPHelper(tcpconn)
{
    std::string port = std::to_string(std::stoi(PORT) + static_cast<int>(tcpconn->getNoiMy().tn));
    // thread для коннекта/реконнекта
    tconn = new std::thread(&Client::init, this, port);
    // хандлер нужно запомнить до detouch-а, чтобы потом
    // поток можно было прибить
    handler_tconn = tconn->native_handle();
    tconn->detach();
}


void Client::init(const std::string &port)
{
    // предварительные прыжки с бубном, чтобы в
    // дальнейшем создать сокет на коннект
    struct addrinfo hints;
    struct addrinfo *rp;
    int s;

    std::string dest_ip;
    dest_ip = connector->getNoiOp().ip;
    dest_port = port;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = IPPROTO_TCP;

    s = getaddrinfo(dest_ip.c_str(), dest_port.c_str(), &hints, &result);
    if (s != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        exit(EXIT_FAILURE);
    }
    // закончили прыгать с бубном

    // загрузка неотправленных сообщений этим
    // НИЦ-ем только(!) текущему серверу/клиенту

    ///load_not_sent(connector, _write_msgs);

    int *p_int;
    p_int = (int*)malloc(sizeof(int));
    *p_int = 1;

    rp = result;
    connectionLost = true;
    isStopped = false;

    // если нужно остановить поток tconn
    // (по факту меняется только в деструкторе)
    while(!isStopped){
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

        // создадим сокет для связи с сервером
        sfd = socket(rp->ai_family, rp->ai_socktype,
                     rp->ai_protocol);
        // если сокет не создало
        if (sfd == -1)
        {
            mutexThreadKilled.unlock();
            continue;
        }

        // форсим сокет использовать этот адрес, даже
        // если этот адрес перед этим пользовался другой сокет
        if(setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, (char*)p_int, sizeof(int)) == -1)
        {
            mutexThreadKilled.unlock();
            continue;
        }

        // если законнектило
        if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
        {
            isStopped = true;
            //usleep(50000);
            // если внезапно живы потоки на чтение/запись
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
            tread = new std::thread(&Client::sockread, this);
            twrite = new std::thread(&Client::sockwrite, this);
            // запомним хандлеры
            handler_tread = tread->native_handle();
            handler_twrite = twrite->native_handle();
            tread->detach();
            twrite->detach();

            // соединение и потоки созданы
            // (nothing to do here)
            connectionLost = false;
        }
        else
        {
            close(sfd);
        }

        mutexThreadKilled.unlock();
        //usleep(20000);
    }

    freeaddrinfo(result);
}


void Client::sockread()
{
    char buf[BUF_SIZE + 1];
    ssize_t nread;
    std::string ip = connector->getNoiOp().ip;
    while(!isStopped)
    {
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
            nread = recv(sfd, buf, BUF_SIZE + 1, MSG_DONTWAIT);
        // если вывалились по таймауту
        else
            nread = -1;
        //// думаю, это надо убрать отсюда, ибо могут прийти
        //// данные, мы с ними ничего не сделаем, а узел
        //// с другой стороны будет считать их полученными
        if(isStopped)
            break;
        // если считали ерунду или не считали ничего
        if (nread == -1){
            usleep(20000);
            continue;
        }
        // если соединение разорвано
        else if(nread == 0)
        {
            mutexThreadKilled.lock();
            connectionLost = true;
            mutexThreadKilled.unlock();
            return;
        }

        // парсинг пришедших данных
        recvData(buf, nread, ip, dest_port);
    //usleep(20000);
    }
}


void Client::sockwrite()
{
    size_t len;
    ssize_t nwritten;
    while(!isStopped)
    {
        _write_msg_mutex.lock();
        // если есть, что отправить
        bool is_to_write = !(_write_msgs.empty());
        if(is_to_write)
        {
            // возьмем первое сообщение
            const std::string& _write_msg = _write_msgs.front().first;
            len = _write_msg.size();
            mutexThreadKilled.lock();
            // если связь оборвана, нечего и пытаться
            // отослать что-нибудь
            if(connectionLost)
            {
                _write_msg_mutex.unlock();
                mutexThreadKilled.unlock();
                return;
            }
            // собственно, отсылка сообщения
            nwritten = send(sfd, _write_msg.c_str(), len + 1, MSG_NOSIGNAL);
            // если все плохо, скажем об этом
            if (nwritten != len + 1) {
//                fprintf(stderr, "partial/failed write\n");
                _write_msg_mutex.unlock();
                mutexThreadKilled.unlock();
//                usleep(1000);
                continue;
            }
            // если все хорошо отправилось
            else
            {
                // поменяем DELIVERY у этого сообщения
                ///make_sent(be, bd, _write_msgs.front().second); // Это дико грузит проц!
                // и уберем его из очереди на отправку
                ++num_write;
                _write_msgs.pop();
            }
            mutexThreadKilled.unlock();
        }
        _write_msg_mutex.unlock();
        if(!is_to_write)
            usleep(20000);
    }
}
