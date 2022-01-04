/*
                   Podrum R3 Copyright MFDGaming & PodrumTeam
                 This file is licensed under the GPLv2 license.
              To use this file you must own a copy of the license.
                       If you do not you can get it from:
            http://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html
 */

#include "./rakserver.h"
#include "./handler.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>

double get_raknet_timestamp(raknet_server_t *server)
{
	return (time(NULL) * 1000) - server->epoch;
}

char has_raknet_connection(misc_address_t address, raknet_server_t *server)
{
	int i;
	for (i = 0; i < server->connections_count; ++i) {
		if ((server->connections[i].address.port == address.port) && (strcmp(server->connections[i].address.address, address.address) == 0)) {
			return 1;
		}
	}
	return 0;
}

void add_raknet_connection(misc_address_t address, unsigned short mtu_size, unsigned long long guid, raknet_server_t *server)
{
	if (has_raknet_connection(address, server) == 0) {
		printf("%s:%d connected!\n", address.address, address.port);
		connection_t connection;
		connection.ack_queue = malloc(0);
		connection.ack_queue_size = 0;
		connection.address = address;
		connection.compound_id = 0;
		connection.frame_holder = malloc(0);
		connection.frame_holder_size = 0;
		connection.guid = guid;
		connection.last_ping_time = time(NULL);
		connection.last_receive_time = time(NULL);
		connection.ms = 0;
		connection.mtu_size = mtu_size;
		connection.nack_queue = malloc(0);
		connection.nack_queue_size = 0;
		connection.queue.frames = malloc(0);
		connection.queue.frames_count = 0;
		connection.queue.sequence_number = 0;
		connection.receiver_ordered_frame_index = 0;
		connection.receiver_reliable_frame_index = 0;
		connection.receiver_sequence_number = 0;
		connection.receiver_sequenced_frame_index = 0;
		connection.recovery_queue = malloc(0);
		connection.recovery_queue_size = 0;
		memset(connection.sender_order_channels, 0, sizeof(connection.sender_order_channels));
		connection.sender_ordered_frame_index = 0;
		connection.sender_reliable_frame_index = 0;
		memset(connection.sender_sequence_channels, 0, sizeof(connection.sender_order_channels));
		connection.sender_sequence_number = 0;
		connection.sender_sequenced_frame_index = 0;
		++server->connections_count;
		server->connections = realloc(server->connections, server->connections_count * sizeof(connection_t));
		server->connections[server->connections_count - 1] = connection;
	}
}

void remove_raknet_connection(misc_address_t address, raknet_server_t *server)
{
	if (has_raknet_connection(address, server) == 1) {
		int i;
		connection_t *connections = malloc((server->connections_count - 1) * sizeof(connection_t));
		int connections_count = 0;
		for (i = 0; i < server->connections_count; ++i) {
			if ((server->connections[i].address.port != address.port) || (strcmp(server->connections[i].address.address, address.address) != 0)) {
				connections[connections_count] = server->connections[i];
				++connections_count;
			}
		}
		free(server->connections);
		server->connections = connections;
		--server->connections_count;
	}
}

connection_t *get_raknet_connection(misc_address_t address, raknet_server_t *server)
{
	int i;
	for (i = 0; i < server->connections_count; ++i) {
		if ((server->connections[i].address.port == address.port) && (strcmp(server->connections[i].address.address, address.address) == 0)) {
			return (&(server->connections[i]));
		}
	}
	return NULL;
}

char is_in_raknet_recovery_queue(unsigned long sequence_number, connection_t *connection)
{
	int i;
	for (i = 0; i < connection->recovery_queue_size; ++i) {
		if (connection->recovery_queue->sequence_number == sequence_number) {
			return 1;
		}
	}
	return 0;
}

void append_raknet_recovery_queue(packet_frame_set_t frame_set, connection_t *connection)
{
	if (is_in_raknet_recovery_queue(frame_set.sequence_number, connection) == 1) {
		++connection->recovery_queue_size;
		connection->recovery_queue = realloc(connection->recovery_queue, connection->recovery_queue_size * sizeof(packet_frame_set_t));
		connection->recovery_queue[connection->recovery_queue_size - 1] = frame_set;
	}
}

void deduct_raknet_recovery_queue(unsigned long sequence_number, connection_t *connection)
{
	if (is_in_raknet_recovery_queue(sequence_number, connection) == 1) {
		int i;
		packet_frame_set_t *recovery_queue = malloc((connection->recovery_queue_size - 1) * sizeof(packet_frame_set_t));
		int recovery_queue_size = 0;
		for (i = 0; i < connection->recovery_queue_size; ++i) {
			if (connection->recovery_queue->sequence_number != sequence_number) {
				recovery_queue[recovery_queue_size] = connection->recovery_queue[i];
				++recovery_queue_size;
			}
		}
		free(connection->recovery_queue);
		connection->recovery_queue = recovery_queue;
		--connection->recovery_queue_size;
	}
}

packet_frame_set_t pop_raknet_recovery_queue(unsigned long sequence_number, connection_t *connection)
{
	if (is_in_raknet_recovery_queue(sequence_number, connection) == 1) {
		int i;
		packet_frame_set_t *recovery_queue = malloc((connection->recovery_queue_size - 1) * sizeof(packet_frame_set_t));
		int recovery_queue_size = 0;
		packet_frame_set_t output_frame_set;
		for (i = 0; i < connection->recovery_queue_size; ++i) {
			if (connection->recovery_queue->sequence_number != sequence_number) {
				recovery_queue[recovery_queue_size] = connection->recovery_queue[i];
				++recovery_queue_size;
			} else {
				output_frame_set = connection->recovery_queue[i];
			}
		}
		free(connection->recovery_queue);
		connection->recovery_queue = recovery_queue;
		--connection->recovery_queue_size;
		return output_frame_set;
	}
	packet_frame_set_t output_frame_set;
	output_frame_set.frames = NULL;
	output_frame_set.frames_count = 0;
	output_frame_set.sequence_number = 0;
	return output_frame_set;
}

void handle_raknet_packet(raknet_server_t *server)
{
	socket_data_t socket_data = receive_data(server->sock);
	if (socket_data.stream.size > 0) {
		/* Just a debug thing
			int i;
			for (i = 0; i < socket_data.stream.size; ++i) {
				printf("0x%X ", socket_data.stream.buffer[i] & 0xff);
			}
			printf("\n");
		*/
		if ((socket_data.stream.buffer[0] & 0xff) == ID_UNCONNECTED_PING)
		{
			socket_data_t output_socket_data;
			output_socket_data.stream = handle_unconneted_ping((&(socket_data.stream)), server);
			output_socket_data.address = socket_data.address;
			send_data(server->sock, output_socket_data);
			free(output_socket_data.stream.buffer);
			memset(&output_socket_data, 0, sizeof(socket_data_t));
		} else if ((socket_data.stream.buffer[0] & 0xff) == ID_OPEN_CONNECTION_REQUEST_1) {
			socket_data_t output_socket_data;
			output_socket_data.stream = handle_open_connection_request_1((&(socket_data.stream)), server);
			output_socket_data.address = socket_data.address;
			send_data(server->sock, output_socket_data);
			free(output_socket_data.stream.buffer);
			memset(&output_socket_data, 0, sizeof(socket_data_t));
		} else if ((socket_data.stream.buffer[0] & 0xff) == ID_OPEN_CONNECTION_REQUEST_2) {
			socket_data_t output_socket_data;
			output_socket_data.stream = handle_open_connection_request_2((&(socket_data.stream)), server, socket_data.address);
			output_socket_data.address = socket_data.address;
			send_data(server->sock, output_socket_data);
			free(output_socket_data.stream.buffer);
			memset(&output_socket_data, 0, sizeof(socket_data_t));
		} else if ((socket_data.stream.buffer[0] & 0xff) == ID_ACK) {
			connection_t *connection = get_raknet_connection(socket_data.address, server);
			if (connection != NULL) {
				handle_ack((&(socket_data.stream)), server, connection);
			}
		} else if ((socket_data.stream.buffer[0] & 0xff) == ID_NACK) {
			connection_t *connection = get_raknet_connection(socket_data.address, server);
			if (connection != NULL) {
				handle_nack((&(socket_data.stream)), server, connection);
			}
		} else {
			printf("0x%X\n", socket_data.stream.buffer[0] & 0xff);
		}
	}
	free(socket_data.stream.buffer);
	memset(&socket_data, 0, sizeof(socket_data_t));
}
