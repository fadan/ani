#include <winsock2.h>

static b32 win32_socket_init()
{
    WORD version_requested = MAKEWORD(2, 2);

    WSADATA data;
    b32 return_code = WSAStartup(version_requested, &data) != NO_ERROR;

    b32 initialized = (0 == return_code);
    return initialized;
}

static PLATFORM_SOCKET_CREATE_UDP_PROC(win32_socket_create_udp)
{
    Socket winsocket = (Socket)socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (winsocket != INVALID_SOCKET)
    {
        sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port);

        if (bind(winsocket, (sockaddr *)&address, sizeof(address)) != SOCKET_ERROR)
        {
            ulong non_blocking = 1;
            if (ioctlsocket(winsocket, FIONBIO, &non_blocking) != SOCKET_ERROR)
            {
            }
            invalid_else;
        }
        invalid_else;
    }
    return winsocket;
}

static PLATFORM_SOCKET_SEND_PROC(win32_socket_send)
{
    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(ip);
    address.sin_port = htons(port);
    
    i32 sent_bytes = sendto(socket, (char *)buffer, buffer_size, 0, (sockaddr *)&address, sizeof(address));
    return sent_bytes;
}

static PLATFORM_SOCKET_RECV_PROC(win32_socket_recv)
{
    sockaddr_in from;
    i32 from_size = sizeof(from);

    i32 received_bytes = recvfrom(socket, (char *)buffer, buffer_size, 0, (sockaddr *)&from, &from_size);
    *ip = ntohl(from.sin_addr.s_addr);
    *port = ntohs(from.sin_port);
    return received_bytes;
}

static PLATFORM_SOCKET_CLOSE_PROC(win32_socket_close)
{
    assert(socket);
    closesocket(socket);
}

static void win32_socket_shutdown()
{
    WSACleanup();
}
