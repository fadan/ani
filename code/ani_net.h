#ifndef ANI_NET_H

#define IPV4_TO_U32(a, b, c, d)  ((a << 24) | (b << 16) | (c << 8) | (d << 0))

#if PLATFORM == PLATFORM_WINDOWS
    typedef unsigned long Socket;
#else
    typedef int Socket;
#endif

#define PLATFORM_SOCKET_CREATE_UDP_PROC(name)   Socket name(u16 port)
#define PLATFORM_SOCKET_SEND_PROC(name)         i32 name(Socket socket, u32 ip, u16 port, void *buffer, i32 buffer_size)
#define PLATFORM_SOCKET_RECV_PROC(name)         i32 name(Socket socket, u32 *ip, u16 *port, void *buffer, i32 buffer_size)
#define PLATFORM_SOCKET_CLOSE_PROC(name)        void name(Socket socket)

typedef PLATFORM_SOCKET_CREATE_UDP_PROC(PlatformSocketCreateUDPProc);
typedef PLATFORM_SOCKET_SEND_PROC(PlatformSocketSendProc);
typedef PLATFORM_SOCKET_RECV_PROC(PlatformSocketRecvProc);
typedef PLATFORM_SOCKET_CLOSE_PROC(PlatformSocketCloseProc);

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

    b32 good_condition;
    f32 penalty_time;
    f32 good_conditions_time;
    f32 penalty_reduction_accum;
};

struct PacketData
{    
    PacketData *next;
    PacketData *prev;

    PacketData *next_free;

    u32 sequence;
    f32 time;
    i32 size;    
};

#define MAX_NUM_ACKS 64

struct NetworkState
{
    b32 initialized;

    Memchunk packet_memchunk;
    Memchunk *temp_memchunk;

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

    PacketData *first_data;
    PacketData *first_free_data;

    PacketData *sent_queue;
    PacketData *recv_queue;
    PacketData *acked_queue;
    PacketData *pending_ack_queue;

    b32 connected;
    f32 send_accum;
    f32 stats_accum;
    
    Connection server;
    Connection client;
};

#define ANI_NET_H
#endif
