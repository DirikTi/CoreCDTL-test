#ifndef CORECDTL_GATEWAY_DISPATCHER_H
#define CORECDTL_GATEWAY_DISPATCHER_H

#include <stdio.h>

typedef void (*dispatcher_on_data_fn)(const char* msg, size_t len);

void gateway_dispatcher_init(dispatcher_on_data_fn on_data_fn);
void gateway_dispatcher_close(void);

void gateway_d_msg_add(const char *msg, size_t msg_len);
void gateway_d_flush(void);

#endif //CORECDTL_GATEWAY_DISPATCHER_H