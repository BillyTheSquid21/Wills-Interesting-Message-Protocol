#include <wimp_reciever.h>

/*
* Represents the state the reciever is in
*/
enum RecieverState
{
	REC_IDLE,
	REC_READING_INSTRUCTION,
	//TODO - add other states
};

/*
* Reciever loop
* 
* @param args The arguments to pass to the reciever
*/
void wimp_reciever_recieve(RecieverArgs args);

/*
* Allocates the instruction for the incoming queue
*/
WimpInstr wimp_reciever_allocateinstr(pssize size);

/*
* Initializes the sockets for the reciever and checks the handshake
* 
* @param recsock Pointer to the reciever socket
* @param rec_address Pointer to the reciever address
* @param args Reciever args
* 
* @return Returns either WIMP_RECIEVER_SUCCESS or WIMP_RECIEVER_FAIL
*/
int32_t wimp_reciever_init(PSocket** recsock, PSocketAddress** rec_address, RecieverArgs args);


WimpInstrMeta wimp_get_instr_from_buffer(uint8_t* buffer, size_t buffsize)
{
	WimpInstrMeta instr;
	instr.arg_bytes = 0;
	instr.dest_process = NULL;
	instr.source_process = NULL;
	instr.instr = NULL;
	instr.args = NULL;
	instr.instr_bytes = 0;
	instr.total_bytes = 0;

	//if buffer is nullptr, was unable to allocate!
	if (buffer == NULL)
	{
		printf("Attempting to get instr from invalid buffer!\n");
		return instr;
	}

	//Size of the full instruction is the start of the buffer - if is \0 is a
	//ping packet so return as is
	if (buffer[0] == '\0')
	{
		return instr;
	}

	instr.dest_process = &buffer[WIMP_INSTRUCTION_DEST_OFFSET];

	//Find start of source process
	char current_char = ' ';
	size_t offset = WIMP_INSTRUCTION_DEST_OFFSET + 1;
	while (current_char != '\0' && offset < buffsize)
	{
		current_char = (char)buffer[offset];
		offset++;
	}

	instr.source_process = &buffer[offset];
	offset++;
	current_char = ' ';

	//Find start of instruction
	while (current_char != '\0' && offset < buffsize)
	{
		current_char = (char)buffer[offset];
		offset++;
	}

	instr.instr = &buffer[offset];
	offset++;
	current_char = ' ';

	//Find start of arg bytes
	while (current_char != '\0' && offset < buffsize)
	{
		current_char = (char)buffer[offset];
		offset++;
	}

	instr.arg_bytes = *(int32_t*)&buffer[offset];
	offset += sizeof(int32_t);

	if (instr.arg_bytes != 0)
	{
		instr.args = &buffer[offset];
	}
	
	instr.total_bytes = *(int32_t*)&buffer[0];

	return instr;
}

int32_t wimp_get_instr_size(uint8_t* buffer)
{
	return *(uint32_t*)buffer;
}

WimpHandshakeHeader wimp_create_handshake(const char* process_name, uint8_t* message_buffer)
{
	int32_t process_name_bytes = (int32_t)(strlen(process_name) + 1) * sizeof(char);
	
	WimpHandshakeHeader header = { 0, 0 }; //Values if below fails

	//Copy the header and the name
	size_t offset = sizeof(WimpHandshakeHeader);

	if (offset + process_name_bytes < WIMP_MESSAGE_BUFFER_BYTES)
	{
		memcpy(&message_buffer[offset], process_name, process_name_bytes);
		header.handshake_header = WIMP_RECIEVER_HANDSHAKE;
		header.process_name_bytes = process_name_bytes;

		memcpy(message_buffer, &header, sizeof(WimpHandshakeHeader));
	}
	return header;
}

RecieverArgs wimp_get_reciever_args(const char* process_name, const char* recfrom_domain, int32_t recfrom_port, WimpInstrQueue* incomingq)
{
	RecieverArgs recargs = malloc(sizeof(struct _RecieverArgs));
	if (recargs == NULL)
	{
		return NULL;
	}

	recargs->incoming_queue = incomingq;

	//Assume all the strings provided are valid
	size_t process_name_bytes = (strlen(process_name) + 1) * sizeof(char);
	size_t recfrom_bytes = (strlen(recfrom_domain) + 1) * sizeof(char);
	recargs->process_name = malloc(process_name_bytes);
	recargs->recfrom_domain = malloc(recfrom_bytes);

	if (recargs->process_name == NULL || recargs->recfrom_domain == NULL)
	{
		free(recargs);
		return NULL;
	}

	memcpy(recargs->process_name, process_name, process_name_bytes);
	memcpy(recargs->recfrom_domain, recfrom_domain, recfrom_bytes);

	recargs->recfrom_port = recfrom_port;
	return recargs;
}

void wimp_free_reciever_args(RecieverArgs args)
{
	free(args->recfrom_domain);
	free(args);
}

int32_t wimp_reciever_init(PSocket** recsock, PSocketAddress** rec_address, RecieverArgs args)
{
	WimpMsgBuffer recbuffer;
	WimpMsgBuffer sendbuffer;
	WIMP_ZERO_BUFFER(recbuffer); WIMP_ZERO_BUFFER(sendbuffer);

	//Create client socket, connect to recfrom server
	//Then send handshake and process name
	WimpHandshakeHeader header = wimp_create_handshake(args->process_name, sendbuffer);

	//Construct address for client, which should be listening
    *rec_address = p_socket_address_new(args->recfrom_domain, args->recfrom_port);
    if (*rec_address == NULL)
    {
		WIMP_ZERO_BUFFER(sendbuffer);
        return WIMP_RECIEVER_FAIL;
    }

    //Create the main listen/recieve socket - currently hard coded
    *recsock = p_socket_new(P_SOCKET_FAMILY_INET, P_SOCKET_TYPE_STREAM, P_SOCKET_PROTOCOL_TCP, NULL);
    if (*recsock == NULL)
    {
		WIMP_ZERO_BUFFER(sendbuffer);
        return WIMP_RECIEVER_FAIL;
    }

    //Connect to end process, which should be waiting to accept
    if (!p_socket_connect(*recsock, *rec_address, NULL))
    {
        p_socket_address_free(*rec_address);
        p_socket_free(*recsock);
		WIMP_ZERO_BUFFER(sendbuffer);
        printf("END PROCESS NOT WAITING!\n");
        return WIMP_RECIEVER_FAIL;
    }

	//Send the handshake
	p_socket_send(*recsock, sendbuffer, sizeof(WimpHandshakeHeader) + header.process_name_bytes, NULL);
	WIMP_ZERO_BUFFER(sendbuffer);

	//Read next handshake
	pssize handshake_size = p_socket_receive(*recsock, recbuffer, WIMP_MESSAGE_BUFFER_BYTES, NULL);

	if (handshake_size <= 0)
	{
        p_socket_address_free(*rec_address);
        p_socket_free(*recsock);
		WIMP_ZERO_BUFFER(recbuffer);
        return WIMP_RECIEVER_FAIL;
	}

	//Check start of handshake
	WimpHandshakeHeader* recheader = (WimpHandshakeHeader*)recbuffer;
	if (recheader->handshake_header != WIMP_RECIEVER_HANDSHAKE)
	{
        p_socket_address_free(*rec_address);
        p_socket_free(*recsock);
		WIMP_ZERO_BUFFER(recbuffer);
		printf("HANDSHAKE FAILED!\n");
		return WIMP_RECIEVER_FAIL;
	}
	WIMP_ZERO_BUFFER(recbuffer);

	printf("RECIEVER HANDSHAKE SUCCESS!\n");
	return WIMP_RECIEVER_SUCCESS;
}

void wimp_reciever_recieve(RecieverArgs args)
{	
	WimpMsgBuffer recbuffer;
	WimpMsgBuffer sendbuffer;
	WIMP_ZERO_BUFFER(recbuffer); WIMP_ZERO_BUFFER(sendbuffer);

	//Initialize the sockets for the reciever and send handshake
	PSocket* recsock;
    PSocketAddress* rec_address;
	if (wimp_reciever_init(&recsock, &rec_address, args) == WIMP_RECIEVER_FAIL)
	{
		wimp_free_reciever_args(args);
		p_uthread_exit(WIMP_RECIEVER_FAIL);
		return;
	}

	//Test connecting packets - if a packet gets split, rebuild it here
	WimpMsgBuffer repbuffer;
	WIMP_ZERO_BUFFER(repbuffer);

	int32_t reciever_state = REC_IDLE;
	int32_t instr_size_read = 0;
	size_t recbuff_offset = 0;
	WimpInstr instr;

	pssize incoming_size = 0;
	bool disconnect = false;
	while (!disconnect)
	{
		//IDLE STATE
		//Idle will sit and poll recieve
		if (reciever_state == REC_IDLE)
		{
			incoming_size = p_socket_receive(recsock, recbuffer, WIMP_MESSAGE_BUFFER_BYTES, NULL);
			if (incoming_size == 0)
			{
				continue;
			}

			//If something recieved, is reading instr state from idle
			reciever_state = REC_READING_INSTRUCTION;
		}

		//READING INSTR STATE
		//Reading will pull in all availible instructions until an empty point is hit
		if (reciever_state == REC_READING_INSTRUCTION)
		{
			int32_t instr_size_bytes = wimp_get_instr_size(&recbuffer[recbuff_offset]);
			if (instr_size_bytes == 0)
			{
				reciever_state = REC_IDLE;
				instr_size_read = 0;
				recbuff_offset = 0;
				instr.instruction = NULL;
				instr.instruction_bytes = 0;
				WIMP_ZERO_BUFFER(recbuffer);
				continue;
			}

			//Allocate instr and loop until read
			instr = wimp_reciever_allocateinstr(instr_size_bytes);
			while (instr_size_read < instr_size_bytes)
			{
				int32_t size_left = instr_size_bytes - instr_size_read;
				int32_t bytes_to_buff_end = WIMP_MESSAGE_BUFFER_BYTES - recbuff_offset;

				if (size_left <= bytes_to_buff_end)
				{
					memcpy(&instr.instruction[instr_size_read], &recbuffer[recbuff_offset], size_left);
					instr_size_read += size_left;
					if (size_left == bytes_to_buff_end)
					{
						recbuff_offset = 0;
					}
					else
					{
						recbuff_offset += size_left;
					}
					break;
				}
				
				//Otherwise read what can be read and rec
				memcpy(&instr.instruction[instr_size_read], &recbuffer[recbuff_offset], bytes_to_buff_end);
				instr_size_read += bytes_to_buff_end;

				WIMP_ZERO_BUFFER(recbuffer);
				incoming_size = p_socket_receive(recsock, recbuffer, WIMP_MESSAGE_BUFFER_BYTES, NULL);
				recbuff_offset = 0;
			}

			//Add the complete instruction
			//Ensure to check for the special signals here
			WimpInstrMeta meta = wimp_get_instr_from_buffer(instr.instruction, instr.instruction_bytes);
			if (strcmp(meta.instr, "exit") == 0)
			{
				printf("Reciever thread closing!: %s\n", args->process_name);
				disconnect = true;
			}
			DEBUG_WIMP_PRINT_INSTRUCTION_META(meta);

			wimp_instr_queue_add(args->incoming_queue, instr.instruction, instr.instruction_bytes);
			instr.instruction = NULL;
			instr.instruction_bytes = 0;
			instr_size_read = 0;
		}
	}

	WIMP_ZERO_BUFFER(recbuffer);
    p_socket_address_free(rec_address);
    p_socket_free(recsock);
	wimp_free_reciever_args(args);
	return;
}

/*
* Creates a unique name assuming any two processes can only have one connection
* each way and that they do not operate on the same port.
*/
char* wimp_name_rec_thread(const char* recname, const char* recfrom, const char* procdom, int32_t recport, int32_t procport)
{
	//Naming convention:
	//RECPROC is what process the reciever recieves instr from
	//RECFROM is what domain the socket gets data from
	//PROCDOM is what domain the thread writes instructions to
	//Name is: RECPROC-PROCDOM:PROCPORT-RECFROM:RECPORT
	char recportstr[6];  //Max port is 5 chars long
	char procportstr[6];
	_itoa(recport, recportstr, 10); _itoa(procport, procportstr, 10);

	size_t procname_s_bytes = strlen(recname) * sizeof(char);
	size_t recfrom_s_bytes = strlen(recfrom) * sizeof(char);
	size_t procdom_s_bytes = strlen(procdom) * sizeof(char);
	size_t recport_s_bytes = strlen(recportstr) * sizeof(char);
	size_t procport_s_bytes = strlen(procportstr) * sizeof(char);
	size_t total_bytes = procname_s_bytes + recfrom_s_bytes + procdom_s_bytes + recport_s_bytes + procport_s_bytes + 1 + 4; //+1 for null, +4 for hyphens and colons

	uint8_t* name_str = malloc(total_bytes);
	if (name_str == NULL)
	{
		return NULL;
	}

	size_t offset = 0;

	memcpy(&name_str[offset], recname, procname_s_bytes);
	offset += procname_s_bytes;

	name_str[offset] = '-';
	offset++;

	memcpy(&name_str[offset], procdom, procdom_s_bytes);
	offset += procdom_s_bytes;

	name_str[offset] = ':';
	offset++;

	memcpy(&name_str[offset], procportstr, procport_s_bytes);
	offset += procport_s_bytes;

	name_str[offset] = '-';
	offset++;

	memcpy(&name_str[offset], recfrom, recfrom_s_bytes);
	offset += recfrom_s_bytes;

	name_str[offset] = ':';
	offset++;

	memcpy(&name_str[offset], recportstr, recport_s_bytes);
	offset += recport_s_bytes;

	name_str[offset] = '\0';
	return name_str;
}

int32_t wimp_start_reciever_thread(const char* recfrom_name, const char* process_domain, int32_t process_port, RecieverArgs args)
{
	char* recname = wimp_name_rec_thread(recfrom_name, args->recfrom_domain, process_domain, args->recfrom_port, process_port);
	printf("\n(Receiver format: RECPROC-PROCDOM:PROCPORT-RECFROM:RECPORT)\n");
	printf("Starting Reciever: %s!\n\n", recname);

	PUThread* process_thread = p_uthread_create((PUThreadFunc)&wimp_reciever_recieve, args, false, recname);
	if (process_thread == NULL)
	{
		printf("Failed to create thread: %s", recname);
		free(recname);
		return WIMP_RECIEVER_FAIL;
	}
	free(recname);
	return WIMP_RECIEVER_SUCCESS;
}

WimpInstr wimp_reciever_allocateinstr(pssize size)
{
	WimpInstr instr = { NULL, 0 };

	void* i = malloc(size);
	if (i == NULL)
	{
		return instr;
	}
	
	instr.instruction = i;
	instr.instruction_bytes = size;
	return instr;
}