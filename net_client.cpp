
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <signal.h>
#include <chrono>
#include <string>
#include <fcntl.h>
#include <sstream>

#include "queuepacket.hpp"
#include "packet.hpp"
#include "rtsp_client.hpp"

//#define PORT 10001//порт для прослушивания udp
const uint32_t delta3sec = 90000 * 3;//for h264/90000

volatile bool gInterrupted = false;

void printChar(const char *data, uint32_t size)
{
    static char buf[] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
    for(uint32_t i = 0; i < size; ++i)
    {
        uint8_t dd = *((uint8_t*)data + i);
        uint8_t dd1 = dd >> 4;
        dd = dd & 0xF;
        dd1 = dd1 & 0xF;
        printf("%c%c \n", buf[dd1], buf[dd]);
    }
}

// Обработчик сигнала SIGINT
void signalHandler(int signum) {
    printf("\nПерехвачен сигнал SIGINT (%d)!\n", signum);
    gInterrupted = true; //Устанавливаем флаг для завершения программы
}

void taskTCP(Channel *channel, const char *addrBind, const char *addrSend, uint16_t port)
{
    int sockTCP = 0;
    struct sockaddr_in serverTCPaddr;

    memset(&serverTCPaddr, 0, sizeof(serverTCPaddr));
    serverTCPaddr.sin_family = AF_INET;
    if(inet_pton(AF_INET, addrSend, &serverTCPaddr.sin_addr) != 1)
    {
        printf("err TCP addr sending no convert: %s", addrSend);
        exit(EXIT_FAILURE);
    }
    serverTCPaddr.sin_port = htons(port);
    printf("TCP for sending %s:%d\n", addrSend, port);

    struct sockaddr_in bindTCPaddr;
    memset(&bindTCPaddr, 0, sizeof(bindTCPaddr));
    bindTCPaddr.sin_family = AF_INET;
    if(inet_pton(AF_INET, addrBind, &bindTCPaddr.sin_addr) != 1)
    {
        printf("err TCP addr binding no convert: %s", addrBind);
        exit(EXIT_FAILURE);
    }
    bindTCPaddr.sin_port = htons(port);
    printf("TCP for binding %s:%d\n", addrBind, port);

    while(!channel->isDone())
    {
        if(sockTCP <= 0)
        {
            sockTCP = socket(AF_INET, SOCK_STREAM, 0);//IPPROTO_TCP
            if(sockTCP < 0)
            {
                perror("TCP socket creation failed");
                exit(EXIT_FAILURE);
            }
            printf("create tcp socket\n");

            int opt = 1;
            if(setsockopt(sockTCP, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0)
            {
                perror("Tcp setsockopt failed\n");
                close(sockTCP);
                sockTCP = 0;
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }

            // Bind the socket to localhost port
            if(bind(sockTCP, (struct sockaddr*)&bindTCPaddr, sizeof(bindTCPaddr)) < 0)
            {
                perror("bind failed");
                close(sockTCP);
                sockTCP = 0;
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }
            printf("TCP binded on %s:%d\n", addrBind, port);
        }

        int res = connect(sockTCP, (struct sockaddr*)&serverTCPaddr, sizeof(serverTCPaddr));
        if (res < 0)
        {
            printf("error Tcp connect to %u\n", port);
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }
        printf("TCP connected on %s:%d\n", addrSend, port);

        fd_set rfds;
        fd_set wfds;
        fd_set efds;
        struct timeval tv;
        Packet *packet = nullptr;
        //int max = 0;
        bool conneckted = false;
        Capacity::CapacityInterval interval;
        std::chrono::system_clock::time_point now = std::chrono::high_resolution_clock::now();
        while (!channel->isDone())
        {
            FD_ZERO(&rfds);
            FD_ZERO(&wfds);
            FD_ZERO(&efds);

            FD_SET(sockTCP, &efds);
            FD_SET(sockTCP, &rfds);

            if(interval.startTimestamp == 0)
            {
                interval = channel->getCapacityForSend();
                if(interval.startTimestamp > 0)
                {
                    printf("1)%u tcp get for send %u %u\n", channel->getNum(), interval.startTimestamp, interval.sendVolume);
                }
            }

            if(interval.startTimestamp > 0)
            {
                FD_SET(sockTCP, &wfds);
            }

            tv.tv_sec = 0; tv.tv_usec = 100000;// 0.1 sec
            int ret = select(sockTCP + 1, &rfds, &wfds, &efds, &tv);

            auto stop = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - now).count();
            if(duration > 5000)// 5 sec
            {
                now = std::chrono::high_resolution_clock::now();
                printf("timeout %lu append losses 0.0\n", duration);
                channel->appendServerLosses(0.0f);//все теряет
                break;//reconnect
            }

            if(ret == 0)//timeout
            {
                if(interval.startTimestamp > 0)
                {
                    printf("!!!!!TCP %u is not selectable\n", channel->getNum());
                    Capacity::CapacityInterval newInterval = channel->getCapacityForSend();
                    if(newInterval.startTimestamp > 0)
                    {
                        interval = newInterval;//заменяем на более новые данные, если есть
                    }
                }
                continue;
            }

            if(ret < 0)
            {
                perror("select error");
                break;
            }

            //server is err
            if(FD_ISSET(sockTCP, &efds))
            {
                int saved_errno = errno;
                printf("error TCP socket: %s\n", strerror(saved_errno));
                break;
            }

            //server is readable
            if(FD_ISSET(sockTCP, &rfds))
            {
                //printf("server is redy read\n");
                Capacity::LossesPacket lossesPacket;
                ssize_t received_count = recv(sockTCP, &lossesPacket, sizeof(lossesPacket), MSG_DONTWAIT);
                if(received_count == 0)
                {
                    printf("Close TCP connection\n");
                    break;
                }

                if(received_count < 0)
                {
                    int saved_errno = errno;
                    printf("Error TCP data: %s\n", strerror(saved_errno));
                    break;
                }

                if(received_count == sizeof(Capacity::LossesPacket))
                {
                    uint32_t timestamp = channel->getEndTimestamp();
                    if(timestamp - lossesPacket.timestamp > delta3sec)
                    {
                        printf("\n3)recv TCP channel %u old timestamp: %u now: %u\n", channel->getNum(),
                            lossesPacket.timestamp, timestamp);
                    }
                    else
                    {
                        channel->appendServerLosses(lossesPacket.losses);
                        printf("\n3)recv TCP channel %u losses: %.2f delta %u\n", channel->getNum(), 
                            lossesPacket.losses, timestamp - lossesPacket.timestamp);
                    }
                    now = std::chrono::high_resolution_clock::now();
                }
                else
                {
                    printf("no all packet recv\n");
                }
            }

            //server is writable
            if(FD_ISSET(sockTCP, &wfds))
            {
                ssize_t ret = send(sockTCP, &interval, sizeof(interval), 0);//!MSG_DONTWAIT
                if(ret < 0)
                {
                    int saved_errno = errno; // Save errno immediately
                    printf(" Error send TCP data: %s\n", strerror(saved_errno));
                    break;
                }
                if(ret == sizeof(interval))
                {
                    printf("2)Send TCP %u %u %u %u\n", channel->getNum(), 
                        interval.startTimestamp, interval.sendVolume, interval.lossVolume);
                    now = std::chrono::system_clock::now();
                }
                else
                {
                    printf("Not all send TCP size %ld != %lu\n", ret, sizeof(interval));
                }
                interval.startTimestamp = 0;
            }
            else if(interval.startTimestamp > 0)
            {
                printf("TCP %u is not writable\n", channel->getNum());
            }
        }
        if(sockTCP > 0)
        {
            close(sockTCP);
            sockTCP = 0;
        }
        printf("reconnect TCP\n");
    }
    printf("exit TCP port: %d\n", port);
}

void taskUDP(Channel *channel, const char *addrBind, const char *addrSend, uint16_t port)
{
    int sockUDP = 0;
    struct sockaddr_in serverUDPaddr;

    //Prepare UDP address structure
    memset(&serverUDPaddr, 0, sizeof(serverUDPaddr));
    serverUDPaddr.sin_family = AF_INET;
    if(inet_pton(AF_INET, addrSend, &serverUDPaddr.sin_addr) != 1)
    {
        printf("err UDP addr no convert: %s", addrSend);
        exit(EXIT_FAILURE);
    }
    serverUDPaddr.sin_port = htons(port);
    printf("UDP for sending %s:%d\n", addrSend, port);

    struct sockaddr_in bindUDPaddr;
    memset(&bindUDPaddr, 0, sizeof(bindUDPaddr));
    bindUDPaddr.sin_family = AF_INET;
    if(inet_pton(AF_INET, addrBind, &bindUDPaddr.sin_addr) != 1)
    {
        printf("err UDP addr no convert: %s", addrBind);
        exit(EXIT_FAILURE);
    }
    bindUDPaddr.sin_port = htons(port);
    printf("UDP for binding %s:%d\n", addrBind, port);

    while(!channel->isDone())
    {
        if(sockUDP <= 0)
        {
            sockUDP = socket(AF_INET, SOCK_DGRAM, 0);//IPPROTO_UDP
            if(sockUDP < 0)
            {
                perror("UDP socket creation failed");
                exit(EXIT_FAILURE);
            }
            printf("create udp socket\n");
        }

        int opt = 1;
        if(setsockopt(sockUDP, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0)
        {
            perror("UDP setsockopt failed\n");
            close(sockUDP);
            sockUDP = 0;
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        //Bind the socket для отправки пакетов по разным каналам на одинаковый адрес
        if(bind(sockUDP, (const struct sockaddr*)&bindUDPaddr, sizeof(bindUDPaddr)) < 0)
        {
            perror("bind failed");
            close(sockUDP);
            sockUDP = 0;
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }
        printf("UDP client binding on port %d\n", port);

        fd_set rfds;
        fd_set wfds;
        fd_set efds;
        struct timeval tv;
        Packet *packet = nullptr;
        while (!channel->isDone())
        {
            FD_ZERO(&rfds);
            FD_ZERO(&wfds);
            FD_ZERO(&efds);

            if(packet == nullptr)
            {
                packet = channel->getForSend();
            }

            if(packet != nullptr)
            {
                FD_SET(sockUDP, &wfds);
            }

            FD_SET(sockUDP, &efds);

            std::chrono::system_clock::time_point start = std::chrono::high_resolution_clock::now();

            tv.tv_sec = 0; tv.tv_usec = 100000;// 0.1 sec - для заполнения очереди на пересылку
            int ret = select(sockUDP + 1, &rfds, &wfds, &efds, &tv);

            if(ret == 0)//timeout
            {
                if(packet)
                {
                    channel->appendClientLosses(0.0f);
                    channel->appendLossTraffic(packet);
                    if(packet->m_dubl)
                    {
                        printf("\ndt%u %u ", channel->getNum(), packet->getNum());
                        channel->returnFreePacket(packet);//выкидываем дублированный пакет
                    }
                    else
                    {
                        //пересылка в соседний канал
                        printf("\nrt%u %u ", channel->getNum(), packet->getNum());
                        channel->getParent()->movePacket(packet, channel);
                    }
                    packet = nullptr;
                    //channel->clearQueue();
                    //printf("UDP %u is not selectable\n", channel->getNum());
                }
                continue;
            }

            if(ret < 0)
            {
                perror("select error");
                close(sockUDP);
                sockUDP = 0;
                if(packet)
                {
                    channel->returnFreePacket(packet);
                    packet = nullptr;
                }
                break;
            }

            //server is err
            if(FD_ISSET(sockUDP, &efds))
            {
                int saved_errno = errno;
                printf("error UDP socket: %s\n", strerror(saved_errno));
                close(sockUDP);
                sockUDP = 0;
                if(packet)
                {
                    channel->returnFreePacket(packet);
                    packet = nullptr;
                }
                break;
            }

            //server is writable
            if(FD_ISSET(sockUDP, &wfds))
            {
                uint16_t len = packet->getDataLen();
                ssize_t ret = sendto(sockUDP, packet->m_data, len, 0,//!MSG_DONTWAIT
                                (const struct sockaddr*)&serverUDPaddr, sizeof(serverUDPaddr));
                if(ret > 0)
                {
                    //printf("s%u_%u ", channel->getNum(), packet->getNum());
                    printf("s%u", channel->getNum());
                    channel->appendSendingTraffic(packet);
                    channel->appendClientLosses(1.0f);
                    int32_t likeliness = channel->getLikeliness();
                    likeliness += ret;
                    channel->setLikeliness(likeliness);
                    if(ret != len)
                    {
                        printf("\nNo all sending UDP %ld, need %d\n", ret, len);
                    }
                }
                channel->returnFreePacket(packet);
                packet = nullptr;
                if(ret < 0)
                {
                    int saved_errno = errno; // Save errno immediately
                    printf("\nError UDP sending data: %s\n", strerror(saved_errno));
                    break;
                }
            }
            else if(packet)
            {
                printf("\nUDP %u is not writable\n", channel->getNum());
            }
        }
        if(sockUDP > 0)
        {
            close(sockUDP);
            sockUDP = 0;
        }
        printf("reconnect\n");
    }
    printf("exit UDP port: %d\n", port);
}

int main(int argc, char *argv[])
{
    std::string addrCam;//адрес для прослушивания udp от камеры
    uint16_t portCam;//порт для прослушивания udp от камеры

    std::string addrBind0;//адрес биндинга udp на 0-й канал
    std::string addrSend0;//адрес для отсылки udp на 0-й канал
    uint16_t port0;//порт для отсылки udp на 0-й канал

    std::string addrBind1;//адрес биндинга udp на 1-й канал
    std::string addrSend1;//адрес для отсылки udp на 1-й канал
    uint16_t port1;//порт для отсылки udp на 1-й канал

    if(argc != 4)
    {
        printf("start with parameters: \"192.168.1.100:10001\""
            " \"192.168.1.100:192.168.1.210:20001\" \"192.168.1.101:192.168.1.210:20003\"\n");
        printf("адрес для прослушивания udp от камеры 192.168.1.100:10001\n");
        printf("адрес биндинга для 0-го канала 192.168.1.100\n");
        printf("адрес для отсылки udp на 0-й канал 192.168.1.210:20001\n");
        printf("адрес подключения TCP на 0-й канал 20001 + 1\n");
        printf("адрес биндинга для 1-го канала 192.168.1.101\n");
        printf("адрес для отсылки udp на 1-й канал 192.168.1.210:20003\n");
        printf("адрес подключения TCP на 1-й канал 20003 + 1\n");

        addrCam = "192.168.1.100";
        portCam = 10001;

        addrBind0 = "192.168.1.100";
        addrSend0 = "192.168.1.210";
        port0 = 20001;

        addrBind1 = "192.168.1.101";
        addrSend1 = "192.168.1.210";
        port1 = 20003;
    }
    else
    {
        for (int i = 1; i < argc; ++i)
        {
            std::vector<std::string> elems;
            std::stringstream ss(argv[i]);
            std::string item;
            while (std::getline(ss, item, ':')) {
                    elems.push_back(item);
            }

            switch(i)
            {
                case 1:
                {
                    if(elems.size() != 2)
                     {
                        return -1;
                    }
                    uint16_t port = std::atoi(elems[1].c_str());
                    if(port == 0)
                    {
                        return -1;
                    }
                    addrCam = elems[0];
                    portCam = port;
                    break;
                }
                case 2:
                {
                    if(elems.size() != 3)
                     {
                        return -1;
                    }
                    uint16_t port = std::atoi(elems[2].c_str());
                    if(port == 0)
                    {
                        return -1;
                    }
                    addrBind0 = elems[0];
                    addrSend0 = elems[1];
                    port0 = port;
                    break;
                }
                case 3:
                {
                    if(elems.size() != 3)
                     {
                        return -1;
                    }
                    uint16_t port = std::atoi(elems[2].c_str());
                    if(port == 0)
                    {
                        return -1;
                    }
                    addrBind1 = elems[0];
                    addrSend1 = elems[1];
                    port1 = port;
                    break;
                }
                default:
                    break;
            }
        }
    }

    QueuePacket queuePacket;

    // Устанавливаем обработчик для сигнала SIGINT
    if(signal(SIGINT, signalHandler) == SIG_ERR)
    {
        perror("Ошибка установки обработчика сигнала");
        exit(EXIT_FAILURE);
    }

    std::thread tu1(taskUDP, queuePacket.getChannel(0), addrBind0.c_str(), addrSend0.c_str(), port0);
    printf("tu1 = %s->%s:%d\n", addrBind0.c_str(), addrSend0.c_str(), port0);

    std::thread tcp1(taskTCP, queuePacket.getChannel(0), addrBind0.c_str(), addrSend0.c_str(), port0 + 1);
    printf("tcp1 = %s->%s:%d\n", addrBind0.c_str(), addrSend0.c_str(), port0 + 1);

    std::thread tu2(taskUDP, queuePacket.getChannel(1), addrBind1.c_str(), addrSend1.c_str(), port1);
    printf("tu2 = %s->%s:%d\n", addrBind1.c_str(), addrSend1.c_str(), port1);

    std::thread tcp2(taskTCP, queuePacket.getChannel(1), addrBind1.c_str(), addrSend1.c_str(), port1 + 1);
    printf("tcp2 = %s->%s:%d\n", addrBind1.c_str(), addrSend1.c_str(), port1 + 1);

    int sockfd = 0;
    struct sockaddr_in camAddr;

    memset(&camAddr, 0, sizeof(camAddr));
    camAddr.sin_family = AF_INET;
    camAddr.sin_addr.s_addr = INADDR_ANY; // Listen on all available interfaces
    camAddr.sin_port = htons(portCam);

    Packet *packet = nullptr;

    while (!gInterrupted)
    {
        // 1. Create UDP socket
        sockfd = socket(AF_INET, SOCK_DGRAM, 0);//IPPROTO_UDP
        if(sockfd < 0)
        {
            perror("socket creation failed");
            exit(EXIT_FAILURE);
        }

        struct timeval timeout;
        timeout.tv_sec = 1;  // 1 seconds
        timeout.tv_usec = 0; // 0 microseconds
        if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
        {
            perror("setsockopt failed");
            exit(EXIT_FAILURE);
        }

        // 3. Bind the socket to the server address
        if(bind(sockfd, (const struct sockaddr*)&camAddr, sizeof(camAddr)) < 0)
        {
            perror("bind failed");
            close(sockfd);
            exit(EXIT_FAILURE);
        }

        printf("UDP server binding on port %d\n", portCam);

        int printNum = 0;
        while (!gInterrupted)
        {
            if(packet == nullptr)
            {
                packet = queuePacket.getFreePacket();
            }
            socklen_t addr_len = sizeof(camAddr);
            //прием от камеры
            ssize_t bytes_received = recvfrom(sockfd, packet->m_data, PACK_BUFFER_SIZE, 0,
                (struct sockaddr*)&camAddr, &addr_len);
            if(bytes_received < 0)
            {
                if(errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
                {
                    continue;
                }
                else
                {
                    perror("recvfrom failed");
                    break;
                }
            }
            else if (bytes_received > 0)
            {
                uint16_t num = packet->getNum();
                uint32_t timestamp = packet->getTimestamp();
//printf("%u-%u\n", timestamp, num);

                packet->setLen(bytes_received);//размер пакета
                queuePacket.movePacket(packet);
                packet = nullptr;
                /*
                //printf("+");
                ++printNum;
                if(printNum > 100)
                {
                    printf("\n");
                    printNum = 0;
                }
                */
            }
        }
        if(sockfd > 0)
        {
            close(sockfd);
            sockfd = 0;
        }
    }

    queuePacket.done();

    tu1.join();
    tcp1.join();
    tu2.join();
    tcp2.join();

    return 0;
}
