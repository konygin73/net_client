#ifndef CHANNEL_CLASS_HPP
#define CHANNEL_CLASS_HPP

#include <mutex>
#include <condition_variable>
#include "capacity.hpp"

class Packet;
class QueuePacket;

class Channel {
public:
    static const int BUF_COUNT = 40;//только не старые пакеты

    Channel(uint8_t num, QueuePacket *parent);
    
    bool movePacket(Packet *packet);//переместить пакет
    bool appendPacket(const Packet *packet);//скопировать
    uint16_t getQueueSize() const;//размер очереди для отправки
    void appendServerLosses(float losses);
    void appendClientLosses(float losses);
    float getLosses() const;
    uint32_t getAndClearDublSendingSize();
    Packet* getForSend();
    void appendSendingTraffic(const Packet *packet);
    void appendLossTraffic(const Packet *packet);
    //void clearQueue();
    uint8_t getNum() const;
    int32_t getLikeliness() const;
    void setLikeliness(int32_t like);
    uint32_t getEndTimestamp();
    float getSpeed();
    int16_t getStart() { return m_point_start; }
    int16_t getEnd() { return m_point_end; }

    Packet* getFreePacket();
    void returnFreePacket(Packet *packet);
    QueuePacket* getParent() { return m_parent; }

    void done();
    bool isDone() const;

    Capacity::CapacityInterval getCapacityForSend();

private:
    uint8_t const m_num;
    QueuePacket *const m_parent;
    
    Packet* m_buffers[BUF_COUNT];//очередь буферов
    int16_t m_point_start;//позиция для чтения, если не ровна m_point_end иначе пустая очередь
    int16_t m_point_end;//позиция для записи
    mutable std::mutex m_mtxBuffers;
    mutable std::condition_variable m_bufcheck;

    volatile bool m_proc_done;//установка и чтение в разных потоках
    Capacity m_capacity;//выделение кадров

    float m_losses;//коэффициент потерь = передано/получено - 1.0 - все получено без потерь
    uint32_t m_dublSendingSize;//двойной размер кадра, если шлет только этот канал
    int32_t m_likeliness;//вероятность пересылки данным каналом - производительность канала
    uint32_t m_lastTimestamp;//время последнего пакета для отсеивания старых
    mutable std::mutex m_mtxChannel;
};

#endif //CHANNEL_CLASS_HPP
