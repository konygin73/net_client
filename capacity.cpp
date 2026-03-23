#include <string>
#include <chrono>

#include "capacity.hpp"

//const int COUNT_PACKET = 20;
//const int VOLUME_HALF_FULL = 30000;

Capacity::Capacity(uint8_t num):
    m_num{num},
    m_speed{0}
{
}

float Capacity::getSpeed()
{
    std::unique_lock<std::mutex> lock(m_mtxCapacity);
    return m_speed;
}

void Capacity::toSendingTraffic()
{
    uint32_t endTimestamp = m_interval.endTimestamp;
            
    if(m_sendInterval.startTimestamp != 0)
    {
        printf("no sending interval=%u\n", m_interval.startTimestamp);
    }
    m_sendInterval = m_interval;
    float loss = 1.0f - (float)m_interval.lossVolume / (float)m_interval.sendVolume;
    printf("\nsending interval%u=%u loss=%.2f\n", m_num, m_sendInterval.startTimestamp, loss);
    uint32_t interval = (m_interval.endTimestamp - m_interval.startTimestamp) / 90;//ms (90000 in sec)
    m_speed = (float)m_interval.sendVolume / interval;
    printf("output %u count: %u, speed: %.2f bt/msec, send volume: %u\n", m_num, m_interval.count, 
        m_speed, m_interval.sendVolume);
    m_interval.startTimestamp = endTimestamp;
    m_interval.sendVolume = 0;
    m_interval.lossVolume = 0;
    m_interval.count = 0;
}

void Capacity::appendSendingTraffic(const Packet *packet)
{
    std::unique_lock<std::mutex> lock(m_mtxCapacity);
    uint32_t timestamp = packet->getTimestamp();
    uint16_t volume = packet->getDataLen();
    if(m_interval.startTimestamp + TIMEPERSEC < timestamp && m_interval.startTimestamp != 0)
    {
        toSendingTraffic();
    }
    if(m_interval.startTimestamp == 0)
    {
        m_interval.startTimestamp = timestamp;
    }
    if(m_interval.startTimestamp != timestamp)//отсчет начинаем со смены timestamp-а
    {
        m_interval.sendVolume += volume;
        m_interval.count++;
        if(m_interval.endTimestamp != timestamp)
        {
            m_interval.endTimestamp = timestamp;
        }
    }
}

void Capacity::appendLossTraffic(const Packet *packet)
{
    std::unique_lock<std::mutex> lock(m_mtxCapacity);
    uint32_t timestamp = packet->getTimestamp();
    uint16_t volume = packet->getDataLen();
    if(m_interval.startTimestamp + TIMEPERSEC < timestamp && m_interval.startTimestamp != 0)
    {
        toSendingTraffic();
    }
    if(m_interval.startTimestamp == 0)
    {
        m_interval.startTimestamp = timestamp;
    }
    if(m_interval.startTimestamp != timestamp)
    {
        m_interval.lossVolume += volume;
        if(m_interval.endTimestamp != timestamp)
        {
            m_interval.endTimestamp = timestamp;
        }
    }
}

Capacity::CapacityInterval Capacity::getCapacityForSend()
{
    std::unique_lock<std::mutex> lock(m_mtxCapacity);
    CapacityInterval interval = m_sendInterval;
    m_sendInterval.startTimestamp = 0;
    return interval;
}
