#include "ani_audio.cpp"
#include "ani_ui.cpp"

#define SERVER_IP       IPV4_TO_U32(127, 0, 0, 1)
#define SERVER_PORT     30000
#define CLIENT_PORT     30001

inline ProgramState *get_or_create_program_state(Memchunk *memchunk)
{
    if (!memchunk->used)
    {
        assert(memchunk->size >= sizeof(ProgramState));
        push_struct(memchunk, ProgramState);
    }

    ProgramState *program_state = (ProgramState *)memchunk->base;
    return program_state;
}

static void ods(char *fmt, ...)
{
   char buffer[1000];
   va_list va;
   va_start(va, fmt);
   vsprintf_s(buffer, fmt, va);
   va_end(va);
   OutputDebugStringA(buffer);
}

static UPDATE_AND_RENDER_PROC(update_and_render)
{
    platform = memory->platform;

    ProgramState *program = get_or_create_program_state(&memory->memchunk);
    if (!program->initialized)
    {
        init_ui();

        program->initialized = true;
    }

    NetworkState *net = &program->network_state;
    if (!net->initialized)
    {
        *net = {0};
        sub_memchunk(&net->packet_memchunk, &memory->memchunk, 16*KB);
        net->temp_memchunk = &memory->memchunk;
        init_net(net);

        init_server_connection(&net->server, SERVER_PORT);
        init_client_connection(&net->client, CLIENT_PORT, SERVER_IP, SERVER_PORT);

        net->initialized = true;
    }
    
    // NOTE(dan): networking
    {
        // NOTE(dan): update conditions

        if (net->client.state == ConnectionState_Connected)
        {
            update_connection_conditions(&net->client, input->dt, net->rtt * 1000.0f);
        }

        if (net->server.state == ConnectionState_Connected)
        {
            update_connection_conditions(&net->server, input->dt, net->rtt * 1000.0f);
        }

        f32 client_send_rate = (net->client.good_condition ? 60.0f : 30.0f);
        f32 server_send_rate = (net->server.good_condition ? 60.0f : 30.0f);

        // NOTE(dan): state changes

        if (net->connected && net->server.state != ConnectionState_Connected)
        {
            reset_connection_conditions(&net->server);
            net->connected = false;
        }

        if (!net->connected && net->client.state == ConnectionState_Connected && net->server.state == ConnectionState_Connected)
        {
            net->connected = true;
        }

        // NOTE(dan): send and receive packets

        net->send_accum += input->dt;

        while (net->send_accum > (1.0f / client_send_rate))
        {
            char client_packet[] = "client to server";
            send_packet(net, &net->client, client_packet, array_size(client_packet));
            net->send_accum -= (1.0f / client_send_rate);
        }

        while (net->send_accum > (1.0f / server_send_rate))
        {
            char server_packet[] = "server to client";
            send_packet(net, &net->server, server_packet, array_size(server_packet));
            net->send_accum -= (1.0f / server_send_rate);
        }

        char packet[256];
        i32 packet_size;
        while (0 < (packet_size = recv_packet(net, &net->client, packet, sizeof(packet))))
        {
            int breakhere = 1;
        }

        while (0 < (packet_size = recv_packet(net, &net->server, packet, sizeof(packet))))
        {
            int breakhere = 4;
        }

        update_connection(net, &net->client, input->dt);
        update_connection(net, &net->server, input->dt);

        net->stats_accum += input->dt;

        while (net->stats_accum >= 0.25f && net->client.state == ConnectionState_Connected)
        {
            ods("rtt %.1fms, sent %d, acked %d, lost %d (%.1f%%), sent bandwidth = %.1fkbps, acked bandwidth = %.1fkbps\n",
                   net->rtt * 1000.0f, net->sent_packets, net->acked_packets, net->lost_packets,
                   net->sent_packets > 0.0f ? (f32)net->lost_packets / (f32)net->sent_packets * 100.0f : 0.0f,
                   net->sent_bandwidth, net->acked_bandwidth);

            net->stats_accum -= 0.25f;
        }
    }

    begin_ui(input, window_width, window_height);
    {
        ImGui::Begin("Audio", 0, 0);

        ImGui::Checkbox("Playback Mic", &program->playback_mic);
        ImGui::DragFloat("Left Master Volume", &program->audio_state.master_volume[0], 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("Right Master Volume", &program->audio_state.master_volume[1], 0.01f, 0.0f, 1.0f);

        ImGui::End();
    }
    end_ui();
}
