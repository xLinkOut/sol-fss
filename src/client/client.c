// @author Luca Cirillo (545480)

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

int main(int argc, char* argv[]) {
    int client_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un saddr;
    saddr.sun_family = AF_UNIX;
    strcpy(saddr.sun_path, "../server/fss.sk");

    printf("%d, %d\n", connect(client_socket, (struct sockaddr*)&saddr, sizeof(saddr)), errno);
    sleep(2);
    close(client_socket);
    return 0;
}