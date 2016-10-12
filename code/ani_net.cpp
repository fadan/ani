#define CONNECTION_PROTOCOL_ID     0x11112222
#define CONNECTION_TIMEOUT_SEC     10.0f

//
// NOTE(dan): packet queues
//

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

inline PacketData *allocate_packet_data(NetworkState *net)
{
    if (!net->first_free_data)
    {
        net->first_free_data = push_struct(&net->packet_memchunk, PacketData);
        net->first_free_data->next_free = 0;
    }

    PacketData *data = net->first_free_data;
    net->first_free_data = data->next_free;

    *data = {0};
    data->next = data;
    data->prev = data;
    
    data->next_free = net->first_data;
    net->first_data = data;
    return data;
}

inline void packet_data_insert_before(NetworkState *net, PacketData *node, u32 sequence, f32 time, i32 size)
{
    if (!net->first_free_data)
    {
        net->first_free_data = push_struct(&net->packet_memchunk, PacketData);
        net->first_free_data->next_free = 0;
    }

    PacketData *new_node = net->first_free_data;
    net->first_free_data = new_node->next_free;

    *new_node = {0};
    new_node->sequence = sequence;
    new_node->time = time;
    new_node->size = size;

    new_node->next = node;
    new_node->prev = node->prev;
    new_node->next->prev = new_node;
    new_node->prev->next = new_node;

    new_node->next_free = net->first_data;
    net->first_data = new_node;
}

inline void packet_data_delete_node(NetworkState *net, PacketData *node)
{
    node->next->prev = node->prev;
    node->prev->next = node->next;

    node->next_free = net->first_free_data;
    net->first_free_data = node;
}

inline void packet_data_clear(NetworkState *net, PacketData *data)
{
    while (data->next != data)
    {
        packet_data_delete_node(net, data->next);
    }
}

inline void packet_data_delete(NetworkState *net, PacketData *data)
{
    packet_data_clear(net, data);

    data->next_free = net->first_free_data;
    net->first_free_data = data;
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

inline void packet_data_insert_sorted(NetworkState *net, PacketData *queue, u32 max_sequence, u32 sequence, f32 time, i32 size)
{
    if (packet_data_empty(queue))
    {
        packet_data_insert_before(net, queue, sequence, time, size);
    }
    else
    {
        if (!sequence_more_recent(sequence, queue->next->sequence, max_sequence))
        {
            packet_data_insert_before(net, queue->next, sequence, time, size);
        }
        else if (sequence_more_recent(sequence, queue->prev->sequence, max_sequence))
        {
            packet_data_insert_before(net, queue, sequence, time, size);
        }
        else
        {
            for (PacketData *node = queue->next; node != queue; node = node->next)
            {
                assert(node->sequence != sequence);
                if (sequence_more_recent(node->sequence, sequence, max_sequence))
                {
                    packet_data_insert_before(net, node, sequence, time, size);
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

//
// NOTE(dan): connection
//

inline void reset_connection_conditions(Connection *connection, f32 penalty_time = 4.0f)
{
    connection->good_condition = false;
    connection->penalty_time   = penalty_time;

    connection->good_conditions_time    = 0.0f;
    connection->penalty_reduction_accum = 0.0f;
}

inline void init_connection(Connection *connection, i16 port)
{
    *connection = {0};

    reset_connection_conditions(connection);

    connection->socket = platform.socket_create_udp(port);
    connection->state  = ConnectionState_Disconnected;

    connection->protocol_id = CONNECTION_PROTOCOL_ID;
    connection->timeout_sec = CONNECTION_TIMEOUT_SEC;
}

inline void init_server_connection(Connection *connection, i16 port)
{
    init_connection(connection, port);

    connection->state  = ConnectionState_Listening;
    connection->is_server = true;
}

inline void init_client_connection(Connection *connection, u16 client_port, u32 server_ip, u16 server_port)
{
    init_connection(connection, client_port);

    connection->state = ConnectionState_Connecting;
    connection->ip    = server_ip;
    connection->port  = server_port;
}

inline void init_net(NetworkState *net, u32 max_sequence = 0xFFFFFFFF, f32 max_rtt = 1.0f)
{
    net->max_sequence = max_sequence;
    net->max_rtt = max_rtt;

    net->first_data = 0;
    net->first_free_data = 0;

    net->sent_queue = allocate_packet_data(net);
    net->recv_queue = allocate_packet_data(net);
    net->acked_queue = allocate_packet_data(net);
    net->pending_ack_queue = allocate_packet_data(net);
}

static void update_connection_conditions(Connection *connection, f32 dt, f32 rtt)
{
    f32 rtt_threshold = 250.0f;

    if (connection->good_condition)
    {
        if (rtt > rtt_threshold)
        {
            // NOTE(dan): dropping to bad condition
            connection->good_condition = false;

            if (connection->good_conditions_time < 10.0f && connection->penalty_time < 60.0f)
            {
                connection->penalty_time *= 2.0f;

                if (connection->penalty_time > 60.0f)
                {
                    connection->penalty_time = 60.0f;
                }
            }

            connection->good_conditions_time = 0.0f;
            connection->penalty_reduction_accum = 0.0f;
        }
        else
        {
            connection->good_conditions_time += dt;
            connection->penalty_reduction_accum += dt;

            if (connection->penalty_reduction_accum > 10.0f && connection->penalty_time > 1.0f)
            {
                connection->penalty_time /= 2.0f;

                if (connection->penalty_time < 1.0f)
                {
                    connection->penalty_time = 1.0f;
                }

                connection->penalty_reduction_accum = 0.0f;
            }
        }
    }
    else
    {
        if (rtt <= rtt_threshold)
        {
            connection->good_conditions_time += dt;
        }
        else
        {
            connection->good_conditions_time = 0.0f;
        }

        if (connection->good_conditions_time > connection->penalty_time)
        {
            connection->good_conditions_time = 0.0f;
            connection->penalty_reduction_accum = 0.0f;

            connection->good_condition = true;
        }
    }
}

static void update_connection(NetworkState *net, Connection *connection, f32 dt)
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

static void process_ack(NetworkState *net, u32 ack, u32 ack_bits)
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

static i32 send_packet(NetworkState *net, Connection *connection, void *buffer, i32 buffer_size)
{
    TempMemchunk temp = begin_temp_memchunk(net->temp_memchunk);
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

    i32 sent_bytes = platform.socket_send(connection->socket, connection->ip, connection->port, packet, header_size + buffer_size);
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

static i32 recv_packet(NetworkState *net, Connection *connection, void *buffer, i32 buffer_size)
{
    TempMemchunk temp = begin_temp_memchunk(net->temp_memchunk);
    i32 header_size = 4 * sizeof(u32);
    void *packet = push_size(temp.memchunk, header_size + buffer_size);
    i32 received_bytes = 0;
    
    u32 sender_ip;
    u16 sender_port;
    received_bytes = platform.socket_recv(connection->socket, &sender_ip, &sender_port, packet, header_size + buffer_size);
    
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
