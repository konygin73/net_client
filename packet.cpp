#include <string>
#include <cstring>
#include <arpa/inet.h>
#include "packet.hpp"

Packet::Packet(): m_dubl{0}, m_size{0}
{
}

uint16_t Packet::getNum() const
{
    uint16_t number = *(uint16_t*)(m_data + OFFSET_NUMBER);
    return htons(number);
}

uint32_t Packet::getTimestamp() const
{
    uint32_t timestamp = *(uint32_t*)(m_data + OFFSET_TIMESTAMP);
    return htonl(timestamp);
}

void Packet::setLen(uint16_t len)
{
    m_size = len;
}

uint16_t Packet::getBuffSize() const
{
    return PACK_BUFFER_SIZE;
}

uint16_t Packet::getDataLen() const
{
    return m_size;
}

Packet& Packet::operator=(const Packet &other)
{
    m_size = other.getDataLen();
    memcpy(m_data, other.m_data, m_size);
    return *this;
}

