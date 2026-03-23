//g++ rtsp_client.cpp -o rtsp_client -lcurl
//./rtsp_client rtsp://192.168.2.110/34567

#ifndef RTSP_CLIENT_HPP
#define RTSP_CLIENT_HPP

#include <string>

void rtsp_client(const std::string argv);

#endif //RTSP_CLIENT_HPP