#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <sys/types.h>

/**
 * @brief Path of the public FIFO used by runners to contact the controller.
 */
#define SERVER_FIFO_PATH "/tmp/server_fifo"

/**
 * @brief Fixed payload size chosen so RpcMessage stays small enough for atomic FIFO writes.
 */
#define RPC_PAYLOAD_SIZE 480

/**
 * @brief Message kinds exchanged between runner processes and the controller.
 */
typedef enum {
	SUBMIT,       /**< Runner requests permission to execute a command. */
	STATUS,      /**< Reserved/legacy status message kind. */
	SHUTDOWN,    /**< Reserved/legacy shutdown message kind. */
	ACK,         /**< Controller authorizes a runner to start executing. */
	DONE,        /**< Runner notifies that a command has finished. */
	STATUS_REQ,  /**< Runner requests the current scheduler state. */
	STATUS_RESP, /**< Controller sends one chunk of the status response. */
	STATUS_END,  /**< Controller marks the end of a status response. */
	SHUTDOWN_REQ,/**< Runner requests controlled controller shutdown. */
	SHUTDOWN_ACK /**< Controller confirms shutdown or rejects new work after shutdown starts. */
} MsgType;

/**
 * @brief Fixed-size protocol message sent through FIFOs.
 *
 * Keeping this structure fixed-size makes the receiver logic simple: each
 * process reads exactly sizeof(RpcMessage). The payload size is intentionally
 * bounded so messages can be written with one write() call and remain atomic
 * under the POSIX PIPE_BUF guarantee.
 */
typedef struct {
	MsgType type;                    /**< Semantic kind of this message. */
	pid_t sender_pid;                /**< PID of the sending process, used to locate its private FIFO. */
	int user_id;                     /**< User identifier associated with the command. */
	long command_id;                 /**< Command identifier used to match SUBMIT, ACK and DONE. */
	char payload[RPC_PAYLOAD_SIZE];  /**< Command string or status response text. */
} RpcMessage;

#endif /* PROTOCOL_H */
