#define ADDRESS(a, b, c, d)  ((a << 24) | (b << 16) | (c << 8) | (d << 0))

static void win32_init_net(Win32Net *net)
{
    WORD version_requested = MAKEWORD(2, 2);
    WSADATA data;

    if (!WSAStartup(version_requested, &data))
    {
        net->socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

        if (net->socket != INVALID_SOCKET)
        {
            sockaddr_in addr;
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = INADDR_ANY;
            addr.sin_port = htons(net->listen_port);

            if (bind(net->socket, (sockaddr *)&addr, sizeof(addr)) != SOCKET_ERROR)
            {
                ulong non_blocking = 1;
                if (ioctlsocket(net->socket, FIONBIO, &non_blocking) != SOCKET_ERROR)
                {
                }
            }
        }
    }
}

static b32 win32_net_send(Win32Net *net, u32 address, u16 port, void *data, i32 size)
{
    sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(address);
    addr.sin_port = htons(port);

    i32 sent_bytes = sendto(net->socket, (char *)data, size, 0, (sockaddr *)&addr, sizeof(addr));

    b32 result = (sent_bytes == size);
    return result;
}

struct Win32NetPacket
{
    u32 address;
    u16 port;
    i32 received_bytes;
};

static Win32NetPacket win32_net_receive(Win32Net *net, void *buffer, u32 buffer_size)
{
    sockaddr_in from;
    i32 from_length = sizeof(from);

    Win32NetPacket result = {0};
    result.received_bytes = recvfrom(net->socket, (char *)buffer, buffer_size, 0, (sockaddr *)&from, &from_length);
    result.address        = ntohl(from.sin_addr.s_addr);
    result.port           = ntohs(from.sin_port);
    return result;
}

static void win32_net_close(Win32Net *net)
{
    if (net->socket)
    {
        closesocket(net->socket);
    }

    WSACleanup();
}
