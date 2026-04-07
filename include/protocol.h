#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <sys/types.h>

#define SERVER_FIFO_PATH "/tmp/server_fifo"

typedef enum {
	SUBMIT,
	STATUS,
	SHUTDOWN,
	ACK,
	DONE,
	STATUS_REQ,
	STATUS_RESP,
	STATUS_END
} MsgType;

typedef struct {
	MsgType type;
	pid_t sender_pid;
	int user_id;
	long command_id;
	char payload[300];
} RpcMessage;

#endif /* PROTOCOL_H */
