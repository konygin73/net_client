#ifndef QUEUEPACKET_CLASS_HPP
#define QUEUEPACKET_CLASS_HPP

#include <mutex>
//#include <condition_variable>
//#include <chrono>
#include <vector>
#include "channel.hpp"

class Packet;

class QueuePacket {
    static const int AMOUNT_CHANNELS = 2;
public:
    QueuePacket();

    Packet* getFreePacket();
    void returnFreePacket(Packet *packet);
    void movePacket(Packet *packet);
    void movePacket(Packet *packet, Channel *channel);//пересылка в другой канал
    Channel* getChannel(int num);
    void done();

private:
    Channel m_channels[AMOUNT_CHANNELS];

    std::vector<Packet*> m_freeBuffers;
    mutable std::mutex m_mtxFreeBuffers;

    uint32_t m_fullSize;
    uint32_t m_startTimestamp;
    //std::chrono::system_clock::time_point m_now;
};

#endif //QUEUEPACKET_CLASS_HPP
