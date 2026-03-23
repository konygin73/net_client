#include <string>
#include <cstring>
#include <arpa/inet.h>
#include "packet.hpp"
#include "channel.hpp"
#include "queuepacket.hpp"
#include "capacity.hpp"

Channel::Channel(uint8_t num, QueuePacket *parent):
    m_num{num},
    m_parent{parent},
    m_point_start{0},//читать готовые, если не равно m_point_end, то очередь пустая
    m_point_end{0},//писать новые в конец
    m_proc_done{false},
    m_capacity{num},
    m_losses{1.0f},//без потерь
    m_dublSendingSize{0},
    m_likeliness{0},
    m_lastTimestamp{0}
{
    for(int i = 0; i < BUF_COUNT; ++i)
    {
	    m_buffers[i] = new Packet;
    }
}

float Channel::getSpeed()
{
    return m_capacity.getSpeed();
}

int32_t Channel::getLikeliness() const
{
    std::unique_lock<std::mutex> lock(m_mtxChannel);
    return m_likeliness;
}

void Channel::setLikeliness(int32_t like)
{
    std::unique_lock<std::mutex> lock(m_mtxChannel);
    m_likeliness = like;
}

uint8_t Channel::getNum() const
{
    return m_num;
}

void Channel::appendServerLosses(float losses)
{
    std::unique_lock<std::mutex> lock(m_mtxChannel);
    m_losses += (losses - m_losses) / 4;
    if(m_losses < 0.1f)
    {
        m_losses = 0.1f;
    }
    if(m_losses > 1.0f)
    {
        m_losses = 1.0f;
    }
    //printf("srv new losses%u: %.2f", m_num, m_losses);
}

void Channel::appendClientLosses(float losses)
{
    std::unique_lock<std::mutex> lock(m_mtxChannel);
    m_losses += (losses - m_losses) / 4;
    if(m_losses < 0.1f)
    {
        m_losses = 0.1f;
    }
    if(m_losses > 1.0f)
    {
        m_losses = 1.0f;
    }
    //printf("cl new losses%u: %.2f", m_num, m_losses);
}

float Channel::getLosses() const
{
    std::unique_lock<std::mutex> lock(m_mtxChannel);
    return m_losses;
}

void Channel::appendSendingTraffic(const Packet *packet)
{
    uint16_t size = packet->getDataLen();
    m_dublSendingSize += packet->m_dubl == 0 ? size * 2 : size;
    m_capacity.appendSendingTraffic(packet);
}

void Channel::appendLossTraffic(const Packet *packet)
{
    m_capacity.appendLossTraffic(packet);
    {
        std::unique_lock<std::mutex> lock(m_mtxChannel);
        m_losses -= m_losses / 3.0f;//при каждом не переданном пакете уменьшаем на треть
        //printf("\n%u m_losses=%.2f\n", m_num, m_losses);
    }
}

uint32_t Channel::getAndClearDublSendingSize()
{
    std::unique_lock<std::mutex> lock(m_mtxChannel);
    uint32_t size = m_dublSendingSize;
    m_dublSendingSize = 0;
    return size;
}

void nextStep(int16_t &num)
{
    ++num;
    if(num >= Channel::BUF_COUNT)
    {
        num -= Channel::BUF_COUNT;
    }
}

bool isThrow(int16_t start, int16_t end)
{
    nextStep(end);
    if(end == start)
    {
        return true;
    }
    return false;
}

bool Channel::movePacket(Packet *packet)
{
    std::unique_lock<std::mutex> lock(m_mtxBuffers);

    if(isThrow(m_point_start, m_point_end))
    {
        m_parent->returnFreePacket(packet);
        return false;
    }
    Packet *freePacket = m_buffers[m_point_end];
    m_buffers[m_point_end] = packet;
    nextStep(m_point_end);
    m_parent->returnFreePacket(freePacket);
    m_bufcheck.notify_one();
    return true;
}

bool Channel::appendPacket(const Packet *packet)
{
    std::unique_lock<std::mutex> lock(m_mtxBuffers);

    if(isThrow(m_point_start, m_point_end))
    {
        return false;
    }
    *m_buffers[m_point_end] = *packet;
    nextStep(m_point_end);
    m_bufcheck.notify_one();
    return true;
}

uint16_t Channel::getQueueSize() const
{
    std::unique_lock<std::mutex> lock(m_mtxBuffers);
    if(m_point_end >= m_point_start)
    {
        return m_point_end - m_point_start;
    }
    return BUF_COUNT - (m_point_start - m_point_end);
}

Packet* Channel::getForSend()
{
    Packet *startPacket;
    Packet *freePacket = m_parent->getFreePacket();
    {
        std::unique_lock<std::mutex> lock(m_mtxBuffers);
        m_bufcheck.wait(lock, [this] { return m_point_start != m_point_end || m_proc_done; });
        if(m_proc_done)
        {
            delete freePacket;
            return nullptr;
        }

        startPacket = m_buffers[m_point_start];
        m_buffers[m_point_start] = freePacket;
        nextStep(m_point_start);//m_point_start-для передачи ->>> m_point_end -для записи
    }

    {
        std::unique_lock<std::mutex> lock(m_mtxChannel);
        m_lastTimestamp = startPacket->getTimestamp();
    }
    
    return startPacket;
}

uint32_t Channel::getEndTimestamp()
{
    std::unique_lock<std::mutex> lock(m_mtxChannel);
    return m_lastTimestamp;
}

/*
void Channel::clearQueue()
{
    std::unique_lock<std::mutex> lock(m_mtxBuffers);
    m_point_start = m_point_end - 1;
    if(m_point_start < 0)
    {
        m_point_start += BUF_COUNT;
    }
}
*/

void Channel::done()
{
    m_proc_done = true;
    m_bufcheck.notify_all();
}

bool Channel::isDone() const
{
    return m_proc_done;
}

Packet* Channel::getFreePacket()
{
    return m_parent->getFreePacket();
}

void Channel::returnFreePacket(Packet *packet)
{
    m_parent->returnFreePacket(packet);
}

Capacity::CapacityInterval Channel::getCapacityForSend()
{
    return m_capacity.getCapacityForSend();
}
