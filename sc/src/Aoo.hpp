#pragma once

#include "SC_PlugIn.hpp"
#undef IN
#undef OUT

#include "rt_shared_ptr.hpp"

#include "aoo.h"
#include "aoo_client.hpp"

#include "common/log.hpp"
#include "common/net_utils.hpp"
#include "common/sync.hpp"
#include "common/time.hpp"
#include "common/utils.hpp"

#include "oscpack/osc/OscOutboundPacketStream.h"

#include <string>
#include <atomic>
#include <optional>

/*//////////////////////// AooNode ////////////////////////*/

namespace sc {
class AooClient;
}

class INode {
public:
    using ptr = std::shared_ptr<INode>;

    static INode::ptr get(int port);

    virtual ~INode() {}

    virtual int port() const = 0;

    virtual AooClient * client() = 0;

    virtual bool registerClient(sc::AooClient *c) = 0;

    virtual void unregisterClient(sc::AooClient *c) = 0;

    virtual void notify() = 0;

    void sendReply(const osc::OutboundPacketStream& msg) {
        sendReply(msg.Data(), msg.Size());
    }

    virtual void sendReply(const char *data, size_t size) = 0;

    virtual bool getSinkArg(sc_msg_iter *args, aoo::ip_address& addr,
                            AooId &id) const = 0;

    virtual bool getSourceArg(sc_msg_iter *args, aoo::ip_address& addr,
                              AooId &id) const = 0;

    virtual bool getPeerArg(sc_msg_iter *args, aoo::ip_address& addr) const = 0;
};

/*//////////////////// Reply ////////////////////////*/

aoo::ip_address findReplyAddr(int port);

void sendReply(int port, const char *msg, size_t size);

inline void sendReply(int port, const osc::OutboundPacketStream& msg){
    sendReply(port, msg.Data(), msg.Size());
}

/*/////////////////// Commands //////////////////////*/

class AooDelegate;

#define AooPluginCmd(x) DefinePlugInCmd("/" #x, x, 0)

struct CmdData {
    template<typename T>
    static T* create(World *world, size_t extra = 0){
        auto data = doCreate(world, sizeof(T) + extra);
        if (data){
            new (data) T();
        }
        return (T *)data;
    }

    template<typename T>
    static void free(World *world, void *cmdData){
        if (cmdData) {
            auto data = (T*)cmdData;
            // destruct members
            // (e.g. release rt::shared_pointer in RT thread)
            data->~T();
            doFree(world, data);
        }
    }

    bool alive() const;

    // data
    rt::shared_ptr<AooDelegate> owner;
private:
    static void * doCreate(World *world, int size);
    static void doFree(World *world, void *cmdData);
};

template<typename T>
struct OpenCmd_ : CmdData {
    int32_t port;
    AooId id;
    INode::ptr node;
    T *obj;
    AooSampleRate sampleRate;
    int32_t blockSize;
    int32_t numChannels;
    AooSeconds latency;
};

struct EndpointCmd : CmdData {
    aoo::ip_address address;
    AooId id = 0; // not kAooIdInvalid!
    int32_t replyID = -1;
};

struct OptionCmd : EndpointCmd {
    AooFlag flags = 0;
    union {
        float f;
        int i;
    };
};

struct UnitCmd : CmdData {
    static UnitCmd *create(World *world, sc_msg_iter *args);
    size_t size;
    char data[1];
};

inline void skipUnitCmd(sc_msg_iter *args){
    args->geti(); // node ID
    args->geti(); // synth index
    args->gets(); // command name
}

/*//////////////////////// AooDelegate /////////////////////////*/

class AooUnit;

class AooDelegate : public std::enable_shared_from_this<AooDelegate>
{
public:
    AooDelegate(AooUnit& owner);

    ~AooDelegate();

    bool alive() const {
        return owner_ != nullptr;
    }

    bool initialized() const {
        return node_ != nullptr;
    }

    void detach() {
        owner_ = nullptr; // first
        onDetach();
    }

    World* world() const {
        return world_;
    }

    AooUnit& unit() {
        return *owner_;
    }

    void setNode(std::shared_ptr<INode> node) {
        node_ = node;
    }

    INode *node(){
        return node_.get();
    }

    // perform sequenced command
    template<typename T>
    void doCmd(T* cmdData, AsyncStageFn stage2, AsyncStageFn stage3 = nullptr,
               AsyncStageFn stage4 = nullptr) {
        doCmd(cmdData, stage2, stage3, stage4, CmdData::free<T>);
    }

    // reply messages
    osc::OutboundPacketStream& beginReply(osc::OutboundPacketStream& msg,
                                          const char *cmd, int replyID) {
        return beginReply(msg, cmd) << replyID;
    }
    osc::OutboundPacketStream& beginEvent(osc::OutboundPacketStream &msg, const char *event) {
        return beginReply(msg, "/aoo/event") << event;
    }
    osc::OutboundPacketStream& beginEvent(osc::OutboundPacketStream& msg, const char *event,
                                          const AooEndpoint& ep) {
        aoo::ip_address addr((const sockaddr *)ep.address, ep.addrlen);
        return beginEvent(msg, event) << addr.name() << addr.port() << ep.id;
    }

    void sendMsgRT(osc::OutboundPacketStream& msg);

    void sendMsgNRT(osc::OutboundPacketStream& msg) {
        msg << osc::EndMessage;
        node_->sendReply(msg);
    }
protected:
    virtual void onDetach() = 0;

    void doCmd(CmdData *cmdData, AsyncStageFn stage2, AsyncStageFn stage3,
               AsyncStageFn stage4, AsyncFreeFn cleanup);

    osc::OutboundPacketStream& beginReply(osc::OutboundPacketStream& msg, const char *cmd);
private:
    World *world_;
    AooUnit *owner_;
    std::shared_ptr<INode> node_;
};

/*//////////////////////// AooUnit /////////////////////////////*/

class AooUnit : public SCUnit {
public:
    AooUnit(){
        LOG_DEBUG("AooUnit");
        mSpecialIndex = 1;
    }
    ~AooUnit(){
        LOG_DEBUG("~AooUnit");
        delegate_->detach();
    }
    // only returns true after the constructor
    // has been called.
    bool initialized() const {
        return mSpecialIndex != 0;
    }
protected:
    rt::shared_ptr<AooDelegate> delegate_;
};

/*/////////////////////// Helper functions ////////////////*/

uint64_t getOSCTime(World *world);

void makeDefaultFormat(AooFormatStorage& f, int sampleRate,
                       int blockSize, int numChannels);

bool serializeFormat(osc::OutboundPacketStream& msg, const AooFormat& f);

bool parseFormat(const AooUnit& unit, int defNumChannels, sc_msg_iter *args,
                 AooFormatStorage &f);

bool serializeData(osc::OutboundPacketStream& msg, const AooData* data);

std::optional<AooData> parseData(sc_msg_iter *args);
