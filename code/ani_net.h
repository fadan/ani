#ifndef ANI_NET_H

#define IPV4_TO_U32(a, b, c, d)  ((a << 24) | (b << 16) | (c << 8) | (d << 0))

//
// NOTE(dan): sockets
//

#if PLATFORM == PLATFORM_WINDOWS
    typedef unsigned long Socket;
#else
    typedef int Socket;
#endif

#define PLATFORM_SOCKET_INIT_PROC(name)         b32 name()
#define PLATFORM_SOCKET_CREATE_UDP_PROC(name)   Socket name(u16 port)
#define PLATFORM_SOCKET_SEND_PROC(name)         i32 name(Socket socket, u32 ip, u16 port, void *buffer, i32 buffer_size)
#define PLATFORM_SOCKET_RECV_PROC(name)         i32 name(Socket socket, u32 *ip, u16 *port, void *buffer, i32 buffer_size)
#define PLATFORM_SOCKET_CLOSE_PROC(name)        void name(Socket socket)
#define PLATFORM_SOCKET_SHUTDOWN_PROC(name)     void name()

typedef PLATFORM_SOCKET_INIT_PROC(PlatformSocketInitProc);
typedef PLATFORM_SOCKET_CREATE_UDP_PROC(PlatformSocketCreateUDPProc);
typedef PLATFORM_SOCKET_SEND_PROC(PlatformSocketSendProc);
typedef PLATFORM_SOCKET_RECV_PROC(PlatformSocketRecvProc);
typedef PLATFORM_SOCKET_CLOSE_PROC(PlatformSocketCloseProc);
typedef PLATFORM_SOCKET_SHUTDOWN_PROC(PlatformSocketShutdownProc);

struct PlatformSocketsApi
{
    PlatformSocketInitProc       *init;
    PlatformSocketCreateUDPProc  *create_udp;
    PlatformSocketSendProc       *send;
    PlatformSocketRecvProc       *recv;
    PlatformSocketCloseProc      *close;
    PlatformSocketShutdownProc   *shutdown;
};

//
// NOTE(dan): virtual connection
//

enum ConnectionState
{
    ConnectionState_Disconnected,

    ConnectionState_Listening,
    ConnectionState_Connecting,
    ConnectionState_ConnectionFailed,
    ConnectionState_Connected,
};

struct Connection
{
    Socket socket;

    ConnectionState state;
    b32 is_server;

    u32 ip;
    u16 port;

    u32 protocol_id;
    f32 timeout_sec;
    f32 timeout_accum;
};

//
// NOTE(dan): packet queue
//

struct PacketData
{    
    PacketData *next;
    PacketData *prev;

    u32 sequence;
    f32 time;
    i32 size;    
};

inline b32 sequence_more_recent(u32 sequence1, u32 sequence2, u32 max_sequence)
{
    u32 half_range = max_sequence / 2;
    b32 result = (((sequence1 > sequence2) && ((sequence1 - sequence2) <= half_range)) || 
                  ((sequence2 > sequence1) && ((sequence2 - sequence1) > half_range)));
    return result;
}

inline b32 sequence_exists(PacketData *queue, u32 sequence)
{
    b32 result = false;
    for (PacketData *node = queue->next; node != queue; node = node->next)
    {
        if (node->sequence == sequence)
        {
            result = true;
            break;
        }
    }
    return result;
}   

// TODO(dan): remove this
#include <malloc.h>

inline PacketData *allocate_packet_data()
{
    // TODO(dan): replace malloc with a free list
    PacketData *data = (PacketData *)malloc(sizeof(*data));
    *data = {0};
    data->next = data;
    data->prev = data;
    return data;
}

inline void packet_data_insert_before(PacketData *node, u32 sequence, f32 time, i32 size)
{
    // TODO(dan): replace malloc with a free list
    PacketData *new_node = (PacketData *)malloc(sizeof(*new_node));
    *new_node = {0};
    new_node->sequence = sequence;
    new_node->time = time;
    new_node->size = size;

    new_node->next = node;
    new_node->prev = node->prev;
    new_node->next->prev = new_node;
    new_node->prev->next = new_node;
}

inline void packet_data_delete_node(PacketData *node)
{
    node->next->prev = node->prev;
    node->prev->next = node->next;
    free(node);
}

inline void packet_data_clear(PacketData *data)
{
    while (data->next != data)
    {
        packet_data_delete_node(data->next);
    }
}

inline void packet_data_delete(PacketData *data)
{
    packet_data_clear(data);
    free(data);
}

inline b32 packet_data_empty(PacketData *data)
{
    b32 result = (data->next == data);
    return result;
}

inline u32 packet_data_size(PacketData *data)
{
    u32 size = 0;
    for (PacketData *node = data->next; node != data; node = node->next)
    {
        ++size;
    }
    return size;
}

static void packet_data_insert_sorted(PacketData *queue, u32 max_sequence, u32 sequence, f32 time, i32 size)
{
    if (packet_data_empty(queue))
    {
        packet_data_insert_before(queue, sequence, time, size);
    }
    else
    {
        if (!sequence_more_recent(sequence, queue->next->sequence, max_sequence))
        {
            packet_data_insert_before(queue->next, sequence, time, size);
        }
        else if (sequence_more_recent(sequence, queue->prev->sequence, max_sequence))
        {
            packet_data_insert_before(queue, sequence, time, size);
        }
        else
        {
            for (PacketData *node = queue->next; node != queue; node = node->next)
            {
                assert(node->sequence != sequence);
                if (sequence_more_recent(node->sequence, sequence, max_sequence))
                {
                    packet_data_insert_before(node, sequence, time, size);
                    break;
                }
            }
        }
    }
}   

inline void packet_data_verify_sorted(PacketData *queue, u32 max_sequence)
{
    for (PacketData *node = queue->next; node != queue; node = node->next)
    {
        assert(node->sequence <= max_sequence);
        if (node->prev != queue)
        {
            assert(sequence_more_recent(node->sequence, node->prev->sequence, max_sequence));
        }
    }
}

#define MAX_NUM_ACKS 64

struct Net
{
    u32 max_sequence;
    u32 local_sequence;
    u32 remote_sequence;

    u32 sent_packets;
    u32 recv_packets;
    u32 lost_packets;
    u32 acked_packets;

    f32 sent_bandwidth;
    f32 acked_bandwidth;
    f32 rtt;
    f32 max_rtt;

    u32 num_acks;
    u32 acks[MAX_NUM_ACKS];

    Memchunk *memchunk;

    PacketData *sent_queue;
    PacketData *recv_queue;
    PacketData *acked_queue;
    PacketData *pending_ack_queue;
};

#define ANI_NET_H
#endif
