#include <stdio.h>
#include <stdint.h>
#include <uni_socket.h>

#define MAX_MESSAGE_LENGTH 1024
#define SERVER_PORT 5432

int main(void) 
{
	p_libsys_init();
	char buffer[MAX_MESSAGE_LENGTH + 1]; // Supports messages up to max length (plus null character that terminates the string, since we're sending text)

	PSocketAddress* addr;
	PSocket* sock;

	// Binding socket to local host (we are a server, the appropriate port.  Typically this will always be a localhost port, because we are going to listen to this)
	if ((addr = p_socket_address_new("127.0.0.1", SERVER_PORT)) == NULL)
		return 1;

	// Create socket
	if ((sock = p_socket_new(P_SOCKET_FAMILY_INET, P_SOCKET_TYPE_STREAM, P_SOCKET_PROTOCOL_TCP, NULL)) == NULL)
	{
		// Failed to create socket -- cleanup
		p_socket_address_free(addr);

		return 2;
	}

	// Bind to local host (server) socket
	if (!p_socket_bind(sock, addr, FALSE, NULL))
	{
		// Couldn't bind socket, cleanup
		p_socket_address_free(addr);
		p_socket_free(sock);

		return 3;
	}

	// Listen for incoming connections on localhost SERVER_PORT
	if (!p_socket_listen(sock, NULL))
	{
		// Couldn't start listening, cleanup
		p_socket_address_free(addr);
		p_socket_close(sock, NULL);

		return 4;
	}

	// Forever, try to accept incoming connections.
	while (1)
	{
		// Blocks until connection accept happens by default -- this can be changed
		PSocket* con = p_socket_accept(sock, NULL);

		if (con != NULL)
		{
			// Send "Message from server" to the client, and terminate their connection.
			printf("Sending message...\n");
			strcpy(buffer, "Message from server\n");
			p_socket_send(con, buffer, strlen(buffer), NULL);
			
			p_socket_close(con, NULL);
		}
		else
			printf("Can't make con, tried and failed...\n");
	}

	// Cleanup
	p_socket_address_free(addr);
	p_socket_close(sock, NULL);
	p_libsys_shutdown();
	return 0;
}