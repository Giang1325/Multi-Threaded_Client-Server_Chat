#include "broadcast.h"
#include "registry.h"
#include <string.h>

void send_message(char *s, int uid){
    registry_broadcast(s, strlen(s), uid);
}