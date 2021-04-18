#ifndef SERVER_H
#define SERVER_H

int server_connect(bool (*is_running)(), void (*relay)());

#endif
