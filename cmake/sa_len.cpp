#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif

int main(int argc, char *argv[])
{
    sockaddr addr;
    addr.sa_len = 0;
    return 0;
}
