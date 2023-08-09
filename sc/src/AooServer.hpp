#include "Aoo.hpp"
#include "aoo/aoo_server.hpp"

#include "common/sync.hpp"

#include <thread>

namespace sc {

class AooServer {
public:
    AooServer(World *world, int port, const char *password);
    ~AooServer();
private:
    World* world_;
    int port_;
    ::AooServer::Ptr server_;
    std::thread thread_;
    std::thread udp_thread_;

    void handleEvent(const AooEvent *event);

    void run();

    void receive();
};

struct AooServerCmd {
    int port;
};

struct AooServerCreateCmd : AooServerCmd {
    char password[64];
};

} // sc
