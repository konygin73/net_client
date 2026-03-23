#ifndef CAPACITY_CLASS_HPP
#define CAPACITY_CLASS_HPP

#include <cstdint>
#include <mutex>
#include "packet.hpp"

const uint32_t PACK_HEADER = 0xAACCAACC;
const uint32_t TIMEPERSEC = 90000*3;//timestamp / sec

class Capacity {
public:
    #pragma pack(push, 1)
    struct LossesPacket {
        uint32_t timestamp;
        float losses;
        float speed;
        LossesPacket(): timestamp{0}, losses{-1.0f}, speed{0} {}
    };
    #pragma pack(pop)

    #pragma pack(push, 1)
    struct CapacityInterval {
        uint32_t header;
        uint32_t startTimestamp;
        uint32_t endTimestamp;
        uint32_t sendVolume;
        uint32_t lossVolume;
        uint32_t count;
        CapacityInterval(): header{PACK_HEADER}, startTimestamp{0}, 
            endTimestamp{0}, sendVolume{0}, lossVolume{0}, count{0} {}
    };
    #pragma pack(pop)

    Capacity(uint8_t num);

    void appendSendingTraffic(const Packet *packet);
    void appendLossTraffic(const Packet *packet);
    CapacityInterval getCapacityForSend();
    float getSpeed();
    void toSendingTraffic();

private:
    uint8_t const m_num;//номер канала
    CapacityInterval m_interval;
    CapacityInterval m_sendInterval;
    float m_speed;//скорость канала bt/msec
    mutable std::mutex m_mtxCapacity;
};

#endif //CAPACITY_CLASS_HPP
