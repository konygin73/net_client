#include <string>
#include <cstring>
#include <arpa/inet.h>
#include "queuepacket.hpp"
#include "packet.hpp"

QueuePacket::QueuePacket(): 
    m_channels{{0, this}, {1, this}},
    m_fullSize{0},
    m_startTimestamp{0}
    //m_now{std::chrono::high_resolution_clock::now()}
{
}

Channel* QueuePacket::getChannel(int num)
{
    return &m_channels[num];
}

Packet* QueuePacket::getFreePacket()
{
    std::unique_lock<std::mutex> lock(m_mtxFreeBuffers);
    if(m_freeBuffers.empty())
    {
        printf("new packet created\n");
        return new Packet;
    }
    Packet *packet = m_freeBuffers.back();
    m_freeBuffers.pop_back();
    return packet;
}

void QueuePacket::returnFreePacket(Packet *packet)
{
    std::unique_lock<std::mutex> lock(m_mtxFreeBuffers);
    m_freeBuffers.push_back(packet);
}

//переслать в другой канал отличный от channel
void QueuePacket::movePacket(Packet *packet, Channel *channel)
{
    uint8_t any_num_channel = &m_channels[0] == channel ? 1 : 0;
    
    if(!m_channels[any_num_channel].movePacket(packet))
    {
        printf("\n-R->%u %u\n", any_num_channel, packet->getNum());//выкинули пакет
        returnFreePacket(packet);
    }
    else
    {
        printf("+r->%u\n", any_num_channel);
    }
}

void QueuePacket::movePacket(Packet *packet)
{
    uint32_t packetTemestamp = packet->getTimestamp();
    if(m_startTimestamp == 0)
    {
       m_startTimestamp = packetTemestamp;
       m_fullSize = 0;
    }

    //float losses0 = m_channels[0].getLosses();
    //if(losses0 < 0.9)
    //{
    //    losses0 += 0.05f;//посылаем немного больше, чем можем
    //}

    //float losses1 = m_channels[1].getLosses();
    //if(losses1 < 0.9)
    //{
    //    losses1 += 0.05f;//посылаем немного больше, чем можем
    //}

    static float s_speed = 0;
    static float s_speed0 = 0;
    static float s_speed1 = 0;

    uint16_t queueSize0 = m_channels[0].getQueueSize();
    uint16_t queueSize1 = m_channels[1].getQueueSize();

    if(m_startTimestamp + TIMEPERSEC < packetTemestamp)//каждую секунду
    {
        float interval = (packetTemestamp - m_startTimestamp) / 90000.0f;//sec
        s_speed = (float)m_fullSize / interval;
        s_speed0 = m_channels[0].getSpeed();
        s_speed1 = m_channels[1].getSpeed();
        printf("\nfull speed: %.2f bt/s\n", s_speed);
        printf("%sspeed0: %.2f; speed1: %.2f bt/s\n", (s_speed0 + s_speed1 < s_speed ? "!!!" : ""), s_speed0, s_speed1);
        //printf("\nlosses0: %.2f losses1: %.2f\n", losses0, losses1);
        printf("queueSize0: %u; queueSize1: %u\n", queueSize0, queueSize1);
        s_speed0 += s_speed0 / 10;
        s_speed1 += s_speed1 / 10;
        m_fullSize = 0;
        m_startTimestamp = packetTemestamp;
    }

    m_fullSize += packet->getDataLen();//подсчет входного трафика

    //int32_t likeliness0 = m_channels[0].getLikeliness();
    //likeliness0 += packet->getDataLen() * losses0;

    //int32_t likeliness1 = m_channels[1].getLikeliness();
    //likeliness1 += packet->getDataLen() * losses1;

    //likeliness1 -= likeliness0;//нормализация
    //likeliness0 = 0;

    packet->m_dubl = 0;
    //if(s_speed0 > s_speed / 2 && s_speed1 > s_speed / 2)
    if(queueSize0 < 5 && queueSize1 < 5)//шлем в оба канала
    {
        packet->m_dubl = 1;

        //likeliness0 -= packet->getDataLen();
        //m_channels[0].setLikeliness(likeliness0);

        //likeliness1 -= packet->getDataLen();
        //m_channels[1].setLikeliness(likeliness1);

        if(m_channels[0].appendPacket(packet))//копирование
        {
            //printf("a0_%u ", packet->getNum());
            printf("a0");
        }
        else
        {
            printf("\nk0!!! %u-%u", m_channels[0].getStart(), m_channels[0].getEnd());
            printf("\nqueueSize0 %u\n", queueSize0);
        }
        
        if(m_channels[1].movePacket(packet))//перенос
        {
            //printf("a1_%u ", packet->getNum());
            printf("a1");
        }
        else
        {
            printf("\nk1!!! %u-%u ", m_channels[1].getStart(), m_channels[1].getEnd());
            printf("\nqueueSize1 %u\n", queueSize1);
        }

        //printf("&");
        return;
    }

    if(queueSize0 < queueSize1)//likeliness0 > likeliness1)
    {
        if(!m_channels[0].movePacket(packet))//перенос
        {
            printf("\n I- %u ", packet->getNum());
        }
        else
        {
            printf("A0");
            //printf("A0_%u ", packet->getNum());
        }
        return;
    }
    
    if(!m_channels[1].movePacket(packet))//перенос
    {
        printf("\n I- %u", packet->getNum());
    }
    else
    {
        printf("A1");
        //printf("A1_%u ", packet->getNum());
    }
}

void QueuePacket::done()
{
    for(int i = 0; i < AMOUNT_CHANNELS; ++i)
    {
        m_channels[i].done();
    }
}
