#define IPV4_TO_U32(a, b, c, d)  ((a << 24) | (b << 16) | (c << 8) | (d << 0))

inline b32 win32_init_net()
{
    WORD version_requested = MAKEWORD(2, 2);
    WSADATA data;

    b32 result = WSAStartup(version_requested, &data) != NO_ERROR;
    return result;
}

inline void win32_shutdown_net()
{
    WSACleanup();
}

static SOCKET win32_create_socket(u16 port)
{
    SOCKET result = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (result != INVALID_SOCKET)
    {
        sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port);

        if (bind(result, (sockaddr *)&address, sizeof(address)) != SOCKET_ERROR)
        {
            ulong non_blocking = 1;
            if (ioctlsocket(result, FIONBIO, &non_blocking) != SOCKET_ERROR)
            {
                // NOTE(dan): created, bound, and set to non blocking
            }
            invalid_else;
        }
        invalid_else;
    }
    return result;
}

inline void win32_close_socket(SOCKET socket)
{
    assert(socket);
    closesocket(socket);
}

static i32 win32_socket_send(SOCKET socket, u32 dest_ip, u16 dest_port, void *data, i32 size)
{
    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(dest_ip);
    address.sin_port = htons(dest_port);

    i32 sent_bytes = sendto(socket, (char *)data, size, 0, (sockaddr *)&address, sizeof(address));
    return sent_bytes;
}

static i32 win32_socket_receive(SOCKET socket, u32 *sender_ip, u16 *sender_port, void *data, i32 size)
{
    sockaddr_in from;
    i32 from_size = sizeof(from);

    i32 received_bytes = recvfrom(socket, (char *)data, size, 0, (sockaddr *)&from, &from_size);
    *sender_ip = ntohl(from.sin_addr.s_addr);
    *sender_port = ntohs(from.sin_port);
    return received_bytes;   
}

enum ConnectionState
{
    ConnectionState_Disconnected,
    ConnectionState_Listening,
    ConnectionState_Connecting,
    ConnectionState_ConnectionFailed,
    ConnectionState_Connected,
};

struct Win32Connection
{
    u32 state;

    u32 protocol_id;
    f32 timeout_secs;
    f32 timeout_accum;
    b32 is_server;

    SOCKET socket;
    u32 ip;
    u16 port;
};

static void win32_update_connection(Win32Connection *connection, f32 dt)
{
    connection->timeout_accum += dt;

    if (connection->timeout_accum > connection->timeout_secs)
    {
        if (connection->state == ConnectionState_Connecting ||
            connection->state == ConnectionState_Connected)
        {
            connection->state = ConnectionState_ConnectionFailed;
        }
    }
}

static i32 win32_send_packet(Memchunk *memchunk, Win32Connection *connection, void *data, i32 size)
{
    TempMemchunk temp = begin_temp_memchunk(memchunk);

    void *packet = push_struct(temp.memchunk, u32);
    *(u32 *)packet = connection->protocol_id;
    push_copy(temp.memchunk, size, data);

    b32 sent_bytes = win32_socket_send(connection->socket, connection->ip, connection->port, packet, sizeof(u32) + size);

    end_temp_memchunk(temp);
    return sent_bytes;
}

static i32 win32_receive_packet(Memchunk *memchunk, Win32Connection *connection, void *data, i32 size)
{
    i32 packet_size = 0;
    TempMemchunk temp = begin_temp_memchunk(memchunk);
    u8 *packet = (u8 *)temp.memchunk;

    i32 header_size = sizeof(u32);

    u32 sender_ip;
    u16 sender_port;
    i32 received_bytes = win32_socket_receive(connection->socket, &sender_ip, &sender_port, packet, header_size + size);

    if (received_bytes > 4)
    {
        u32 protocol_id = *(u32 *)packet;
        if (connection->protocol_id == protocol_id)
        {
            if (connection->is_server && connection->state != ConnectionState_Connected)
            {
                // NOTE(dan): server accepts connection from client
                connection->state = ConnectionState_Connected;
                connection->ip = sender_ip;
                connection->port = sender_port;
            }   

            if (connection->ip == sender_ip && connection->port == sender_port)
            {
                if (!connection->is_server && connection->state == ConnectionState_Connecting)
                {
                    // NOTE(dan): client connected to server
                    connection->state = ConnectionState_Connected;
                }

                connection->timeout_accum = 0.0f;
                memcpy(data, packet + header_size, received_bytes - header_size);
                packet_size = received_bytes - header_size;
            }
        }
    }
    end_temp_memchunk(temp);
    return packet_size;
}

// struct PacketData
// {
//     u32 sequence;
//     f32 time;
//     i32 size;
// };

// inline b32 sequence_more_recent(u32 sequence1, u32 sequence2, u32 max_sequence)
// {
//     u32 half_range = max_sequence / 2;
//     b32 result = (((sequence1 > sequence2) && ((sequence1 - sequence2) <= half_range)) || 
//                   ((sequence2 > sequence1) && ((sequence2 - sequence1) > half_range)));
//     return result;
// }
