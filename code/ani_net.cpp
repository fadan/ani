#define CONNECTION_PROTOCOL_ID     0x11112222
#define CONNECTION_TIMEOUT_SEC     10.0f

static PlatformSocketsApi sockets_api;

//
// NOTE(dan): connection
//

inline void init_server_connection(Connection *connection, i16 port)
{
    *connection = {0};
    connection->socket = sockets_api.create_udp(port);
    connection->state  = ConnectionState_Listening;

    connection->protocol_id = CONNECTION_PROTOCOL_ID;
    connection->timeout_sec = CONNECTION_TIMEOUT_SEC;

    connection->is_server = true;
}

inline void init_client_connection(Connection *connection, u16 client_port, u32 server_ip, u16 server_port)
{
    *connection = {0};
    connection->socket = sockets_api.create_udp(client_port);
    connection->state  = ConnectionState_Connecting;

    connection->ip   = server_ip;
    connection->port = server_port;

    connection->protocol_id = CONNECTION_PROTOCOL_ID;
    connection->timeout_sec = CONNECTION_TIMEOUT_SEC;
}

inline void init_net(PlatformSocketsApi api, Net *net, Memchunk *memchunk, u32 max_sequence = 0xFFFFFFFF, f32 max_rtt = 1.0f)
{
    sockets_api = api;
    sockets_api.init();

    *net = {0};

    net->max_sequence = max_sequence;
    net->max_rtt = max_rtt;

    net->memchunk = memchunk;
    sub_memchunk(&net->packet_data_memory, net->memchunk, 16*KB);

    net->first_data = 0;
    net->first_free_data = 0;

    net->sent_queue = allocate_packet_data(net);
    net->recv_queue = allocate_packet_data(net);
    net->acked_queue = allocate_packet_data(net);
    net->pending_ack_queue = allocate_packet_data(net);
}

inline void shutdown_net(Net *net)
{
    sockets_api.shutdown();
}

static void update_connection(Net *net, Connection *connection, f32 dt)
{
    // NOTE(dan): clear acks array
    net->num_acks = 0;

    // NOTE(dan): update connection
    {
        connection->timeout_accum += dt;
        if (connection->timeout_accum > connection->timeout_sec)
        {
            if (connection->state == ConnectionState_Connecting ||
                connection->state == ConnectionState_Connected)
            {
                connection->state = ConnectionState_ConnectionFailed;
            }
        }
    }

    // NOTE(dan): advance queue time
    {
        for (PacketData *node = net->sent_queue->next; node != net->sent_queue; node = node->next)
        {
            node->time += dt;
        }
        for (PacketData *node = net->recv_queue->next; node != net->recv_queue; node = node->next)
        {
            node->time += dt;
        }
        for (PacketData *node = net->pending_ack_queue->next; node != net->pending_ack_queue; node = node->next)
        {
            node->time += dt;
        }
        for (PacketData *node = net->acked_queue->next; node != net->acked_queue; node = node->next)
        {
            node->time += dt;
        }
    }

    // NOTE(dan): update queues
    {
        f32 epsilon = 0.001f;
        while (!packet_data_empty(net->sent_queue) && (net->sent_queue->next->time > (net->max_rtt + epsilon)))
        {
            packet_data_delete_node(net, net->sent_queue->next);
        }

        if (!packet_data_empty(net->recv_queue))
        {
            u32 latest_sequence = net->recv_queue->prev->sequence;
            u32 min_sequence = latest_sequence >= 34 ? (latest_sequence - 34) : net->max_sequence - (34 - latest_sequence);

            while (!packet_data_empty(net->recv_queue) && !sequence_more_recent(net->recv_queue->next->sequence, min_sequence, net->max_sequence))
            {
                packet_data_delete_node(net, net->recv_queue->next);
            }
        }

        while (!packet_data_empty(net->acked_queue) && (net->acked_queue->next->time > (net->max_rtt * 2 - epsilon)))
        {
            packet_data_delete_node(net, net->acked_queue->next);
        }

        while (!packet_data_empty(net->pending_ack_queue) && (net->pending_ack_queue->next->time > (net->max_rtt + epsilon)))
        {
            packet_data_delete_node(net, net->pending_ack_queue->next);
            ++net->lost_packets;
        }
    }

    // NOTE(dan): update stats
    {
        i32 sent_bytes_per_sec = 0;
        i32 acked_packets_per_sec = 0;
        i32 acked_bytes_per_sec = 0;

        for (PacketData *node = net->sent_queue->next; node != net->sent_queue; node = node->next)
        {
            sent_bytes_per_sec += node->size;
        }

        for (PacketData *node = net->acked_queue->next; node != net->acked_queue; node = node->next)
        {
            if (node->time >= net->max_rtt)
            {
                ++acked_packets_per_sec;
                acked_bytes_per_sec += node->size;
            }
        }

        sent_bytes_per_sec = (i32)(sent_bytes_per_sec / net->max_rtt);
        acked_bytes_per_sec = (i32)(acked_bytes_per_sec / net->max_rtt);

        net->sent_bandwidth = sent_bytes_per_sec * (8 / 1000.0f);
        net->acked_bandwidth = acked_bytes_per_sec * (8 / 1000.0f);
    }

    // NOTE(dan): validate
    #if INTERNAL_BUILD
    {
        packet_data_verify_sorted(net->sent_queue, net->max_sequence);
        // packet_data_verify_sorted(net->recv_queue, net->max_sequence);
        packet_data_verify_sorted(net->pending_ack_queue, net->max_sequence);
        packet_data_verify_sorted(net->acked_queue, net->max_sequence);
    }
    #endif
}

//
// NOTE(dan): acks
//

static i32 bit_index_for_sequence(u32 sequence, u32 ack, u32 max_sequence)
{
    assert(sequence != ack);
    assert(!sequence_more_recent(sequence, ack, max_sequence));

    i32 result = 0;
    if (sequence > ack)
    {
        assert(ack < 33);
        assert(max_sequence >= sequence);

        result = ack + (max_sequence - sequence);
    }
    else
    {
        assert(ack >= 1);
        assert(sequence  <= (ack - 1));

        result = ack - 1 - sequence;
    }
    return result;
}

static u32 generate_ack_bits(u32 ack, PacketData *recv_queue, u32 max_sequence)
{
    u32 ack_bits = 0;
    for (PacketData *node = recv_queue->next; node != recv_queue; node = node->next)
    {
        if (node->sequence == ack || sequence_more_recent(node->sequence, ack, max_sequence))
        {
            break;
        }

        i32 bit_index = bit_index_for_sequence(node->sequence, ack, max_sequence);        
        if (bit_index <= 31)
        {
            ack_bits |= 1 << bit_index;
        }
    }
    return ack_bits;
}

static void process_ack(Net *net, u32 ack, u32 ack_bits)
{
    if (!packet_data_empty(net->pending_ack_queue))
    {
        PacketData *node = net->pending_ack_queue->next;
        while (node != net->pending_ack_queue)
        {
            b32 acked = false;
            if (node->sequence == ack)
            {
                acked = true;
            }
            else if (!sequence_more_recent(node->sequence, ack, net->max_sequence))
            {
                i32 bit_index = bit_index_for_sequence(node->sequence, ack, net->max_sequence);
                if (bit_index <= 31)
                {
                    acked = (ack_bits >> bit_index) & 1;
                }
            }

            if (acked)
            {
                net->rtt += (node->time - net->rtt) * 0.1f;

                packet_data_insert_sorted(net, net->acked_queue, net->max_sequence, node->sequence, node->time, node->size);

                assert((net->num_acks + 1) < array_size(net->acks));
                net->acks[net->num_acks++] = node->sequence;
                ++net->acked_packets;

                node = node->next;
                packet_data_delete_node(net, node->prev);
            }
            else
            {
                node = node->next;
            }
        }
    }
}

//
// NOTE(dan): handling packets
//

static i32 send_packet(Net *net, Connection *connection, void *buffer, i32 buffer_size)
{
    TempMemchunk temp = begin_temp_memchunk(net->memchunk);
    i32 header_size = 4 * sizeof(u32);

    void *packet = push_array(temp.memchunk, 4, u32);
    u32 sequence = net->local_sequence;
    u32 ack = net->remote_sequence;
    u32 ack_bits = generate_ack_bits(net->remote_sequence, net->recv_queue, net->max_sequence);

    u32 *packet_u32 = (u32 *)packet;
    packet_u32[0] = htonl(connection->protocol_id);
    packet_u32[1] = htonl(sequence);
    packet_u32[2] = htonl(ack);
    packet_u32[3] = htonl(ack_bits);
    push_copy(temp.memchunk, buffer_size, buffer);

    i32 sent_bytes = sockets_api.send(connection->socket, connection->ip, connection->port, packet, header_size + buffer_size);
    if (sent_bytes > 0)
    {
        assert(!sequence_exists(net->sent_queue, net->local_sequence));
        assert(!sequence_exists(net->pending_ack_queue, net->local_sequence));

        packet_data_insert_before(net, net->sent_queue, net->local_sequence, 0, buffer_size);
        packet_data_insert_before(net, net->pending_ack_queue, net->local_sequence, 0, buffer_size);

        ++net->sent_packets;
        ++net->local_sequence;

        if (net->local_sequence > net->max_sequence)
        {
            net->local_sequence = 0;
        }
    }

    end_temp_memchunk(temp);
    return sent_bytes;
}

static i32 recv_packet(Net *net, Connection *connection, void *buffer, i32 buffer_size)
{
    TempMemchunk temp = begin_temp_memchunk(net->memchunk);
    i32 header_size = 4 * sizeof(u32);
    void *packet = push_size(temp.memchunk, header_size + buffer_size);
    i32 received_bytes = 0;
    
    u32 sender_ip;
    u16 sender_port;
    received_bytes = sockets_api.recv(connection->socket, &sender_ip, &sender_port, packet, header_size + buffer_size);
    
    if (received_bytes > header_size)
    {
        u32 *packet_u32 = (u32 *)packet;
        u32 protocol_id = ntohl(packet_u32[0]);
        u32 sequence    = ntohl(packet_u32[1]);
        u32 ack         = ntohl(packet_u32[2]);
        u32 ack_bits    = ntohl(packet_u32[3]);

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

                ++net->recv_packets;

                if (!sequence_exists(net->recv_queue, sequence))
                {
                    packet_data_insert_before(net, net->recv_queue, sequence, 0, received_bytes - header_size);
                    if (sequence_more_recent(sequence, net->remote_sequence, net->max_sequence))
                    {
                        net->remote_sequence = sequence;
                    }
                }

                process_ack(net, ack, ack_bits);

                memcpy(buffer, (u8 *)packet + header_size, received_bytes - header_size);
                received_bytes -= header_size;
            }
        }
    }
    end_temp_memchunk(temp);
    return received_bytes;
}
