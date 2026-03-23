#ifndef PACKET_CLASS_HPP
#define PACKET_CLASS_HPP

#include <mutex>
#include <condition_variable>
#include <chrono>
#include <vector>

/*
The RTP header has the following format:
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |V=2|P|X|  CC   |M|     PT      |       sequence number         |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                           timestamp                           |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |           synchronization source (SSRC) identifier            |
   +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
   |            contributing source (CSRC) identifiers             |
   |                             ....                              |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/


const int PACK_BUFFER_SIZE = 2000;

#pragma pack(push, 1)
struct Packet {
    static const int OFFSET_NUMBER = 2;//смещение позиции номера пакета в rtp пакете
    static const int OFFSET_TIMESTAMP = 4;//смещение позиции timestamp в rtp пакете

    Packet();

    uint16_t getNum() const;
    uint32_t getTimestamp() const;
    void setLen(uint16_t dataLen);
    uint16_t getBuffSize() const;
    uint16_t getDataLen() const;
    Packet& operator=(const Packet& other);

    uint16_t m_dubl;//послана по двум каналам
    uint16_t m_size;
    char m_data[PACK_BUFFER_SIZE];//body - rtp пакет
};
#pragma pack(pop)

#endif //PACKET_CLASS_HPP
