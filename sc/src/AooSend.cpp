#include "AooSend.hpp"

static InterfaceTable *ft;

/*////////////////// AooSend ////////////////*/

void AooSend::init(int32_t port, aoo_id id) {
    auto data = CmdData::create<OpenCmd>(world());
    if (data){
        data->port = port;
        data->id = id;
        data->sampleRate = unit().sampleRate();
        data->blockSize = unit().bufferSize();
        data->numChannels = static_cast<AooSendUnit&>(unit()).numChannels();

        doCmd(data,
            [](World *world, void *data){
                LOG_DEBUG("try to get node");
                // open in NRT thread
                auto cmd = (OpenCmd *)data;
                auto node = INode::get(world, *cmd->owner, AOO_TYPE_SOURCE,
                                       cmd->port, cmd->id);
                if (node){
                    auto source = aoo::isource::create(cmd->id,
                        (aoo_replyfn)INodeClient::reply,
                        static_cast<INodeClient *>(cmd->owner.get()));
                    if (source){
                        cmd->node = node;
                        source->setup(cmd->sampleRate, cmd->blockSize,
                                      cmd->numChannels);

                        source->set_buffersize(DEFBUFSIZE);

                        aoo_format_storage f;
                        makeDefaultFormat(f, cmd->sampleRate,
                                          cmd->blockSize, cmd->numChannels);
                        source->set_format(f.header);

                        cmd->obj.reset(source);
                        return true;
                    }
                }
                return false;
            },
            [](World *world, void *data){
                auto cmd = (OpenCmd *)data;
                auto& owner = static_cast<AooSend&>(*cmd->owner);
                owner.source_ = std::move(cmd->obj);
                owner.setNode(std::move(cmd->node)); // last!
                LOG_DEBUG("AooSend initialized");
                return false; // done
            }
        );
    }
}

void AooSend::onDetach() {
    auto data = CmdData::create<CmdData>(world());
    if (data){
        doCmd(data,
            [](World *world, void *data){
                // release in NRT thread
                auto cmd = (CmdData*)data;
                auto& owner = static_cast<AooSend&>(*cmd->owner);
                owner.releaseNode(); // first!
                owner.source_ = nullptr;
                return false; // done
            }
        );
    }
}

void AooSend::handleEvent(const aoo_event *event){
    char buf[256];
    osc::OutboundPacketStream msg(buf, 256);

    switch (event->type){
    case AOO_PING_EVENT:
    {
        auto e = (const aoo_ping_event *)event;
        aoo::ip_address addr((const sockaddr *)e->address, e->addrlen);
        double diff1 = aoo_osctime_duration(e->tt1, e->tt2);
        double diff2 = aoo_osctime_duration(e->tt2, e->tt3);
        double rtt = aoo_osctime_duration(e->tt1, e->tt3);

        beginEvent(msg, "/ping", addr, e->id);
        msg << diff1 << diff2 << rtt << e->lost_blocks;
        sendMsgRT(msg);
        break;
    }
    case AOO_INVITE_EVENT:
    {
        auto e = (const aoo_sink_event *)event;
        aoo::ip_address addr((const sockaddr *)e->address, e->addrlen);

        if (accept_){
            addSinkEvent(addr, e->id, 0);
        } else {
            beginEvent(msg, "/invite", addr, e->id);
            sendMsgRT(msg);
        }
        break;
    }
    case AOO_UNINVITE_EVENT:
    {
        auto e = (const aoo_sink_event *)event;
        aoo::ip_address addr((const sockaddr *)e->address, e->addrlen);

        if (accept_){
            removeSinkEvent(addr, e->id);
        } else {
            beginEvent(msg, "/uninvite", addr, e->id);
            sendMsgRT(msg);
        }
        break;
    }
    default:
        break;
    }
}

void AooSend::addSinkEvent(const aoo::ip_address& addr, aoo_id id,
                           int32_t channelOnset) {
    auto cmd = CmdData::create<OptionCmd>(world());
    if (cmd){
        cmd->address = addr;
        cmd->id = id;
        cmd->i = channelOnset;

        doCmd(cmd, [](World * world, void *cmdData){
            auto data = (OptionCmd *)cmdData;
            auto& owner = static_cast<AooSend &>(*data->owner);

            if (owner.addSink(data->address, data->id, data->i)){
                char buf[256];
                osc::OutboundPacketStream msg(buf, sizeof(buf));
                owner.beginEvent(msg, "/add", data->address, data->id);
                owner.sendMsgNRT(msg);
            }

            return false; // done
        });
    }
}

bool AooSend::addSink(const aoo::ip_address& addr, aoo_id id,
                      int channelOnset){
    if (source()->add_sink(addr.address(), addr.length(), id) > 0){
        if (channelOnset > 0){
            source()->set_sink_channelonset(addr.address(), addr.length(),
                                            id, channelOnset);
        }
        return true;
    } else {
        return false;
    }
}

void AooSend::removeSinkEvent(const aoo::ip_address& addr, aoo_id id){
    auto cmd = CmdData::create<OptionCmd>(world());
    if (cmd){
        cmd->address = addr;
        cmd->id = id;

        doCmd(cmd, [](World * world, void *cmdData){
            auto data = (OptionCmd *)cmdData;
            auto& owner = static_cast<AooSend &>(*data->owner);

            if (owner.removeSink(data->address, data->id)){
                char buf[256];
                osc::OutboundPacketStream msg(buf, sizeof(buf));
                owner.beginEvent(msg, "/remove", data->address, data->id);
                owner.sendMsgNRT(msg);
            }

            return false; // done
        });
    }
}

bool AooSend::removeSink(const aoo::ip_address& addr, aoo_id id){
    return source()->remove_sink(addr.address(), addr.length(), id) > 0;
}

void AooSend::removeAll(){
    source()->remove_all();
}

/*////////////////// AooSendUnit ////////////////*/

AooSendUnit::AooSendUnit(){
    int32_t port = in0(0);
    aoo_id id = in0(1);
    auto delegate = rt::make_shared<AooSend>(mWorld, *this);
    delegate->init(port, id);
    delegate_ = std::move(delegate);

    set_calc_function<AooSendUnit, &AooSendUnit::next>();
}

void AooSendUnit::next(int numSamples){
    auto source = delegate().source();
    if (source){
        // check if play state has changed
        bool playing = in0(2);
        if (playing != playing_) {
            if (playing){
                source->start();
            } else {
                source->stop();
            }
            playing_ = playing;
        }

        auto vec = (const float **)mInBuf + channelOnset_;
        uint64_t t = getOSCTime(mWorld);

        if (source->process(vec, numSamples, t) > 0){
            delegate().node()->notify();
        }

        source->poll_events(
            [](void *user, const aoo_event *event){
                static_cast<AooSend *>(user)->handleEvent(event);
            }, delegate_.get());
    }
}

/*//////////////////// Unit commands ////////////////////*/

namespace {

void aoo_send_add(AooSendUnit *unit, sc_msg_iter* args){
    auto cmd = UnitCmd::create(unit->mWorld, args);
    unit->delegate().doCmd(cmd,
        [](World *world, void *cmdData){
            auto data = (UnitCmd *)cmdData;
            auto& owner = static_cast<AooSend&>(*data->owner);

            sc_msg_iter args(data->size, data->data);
            skipUnitCmd(&args);

            auto replyID = args.geti();

            char buf[256];
            osc::OutboundPacketStream msg(buf, sizeof(buf));
            owner.beginReply(msg, "/aoo/add", replyID);

            aoo::ip_address addr;
            aoo_id id;
            if (getSinkArg(owner.node(), &args, addr, id)){
                auto channelOnset = args.geti();

                // only send IP address on success
                if (owner.addSink(addr, id, channelOnset)){
                    msg << addr.name() << addr.port() << id;
                }
            }

            owner.sendMsgNRT(msg);

            return false; // done
        });
}

void aoo_send_remove(AooSendUnit *unit, sc_msg_iter* args){
    auto cmd = UnitCmd::create(unit->mWorld, args);
    unit->delegate().doCmd(cmd,
        [](World *world, void *cmdData){
            auto data = (UnitCmd *)cmdData;
            auto& owner = static_cast<AooSend&>(*data->owner);

            sc_msg_iter args(data->size, data->data);
            skipUnitCmd(&args);

            auto replyID = args.geti();

            char buf[256];
            osc::OutboundPacketStream msg(buf, sizeof(buf));
            owner.beginReply(msg, "/aoo/remove", replyID);

            if (args.remain() > 0){
                aoo::ip_address addr;
                aoo_id id;
                if (getSinkArg(owner.node(), &args, addr, id)){
                    if (owner.removeSink(addr, id)){
                        // only send IP address on success
                        msg << addr.name() << addr.port() << id;
                    }
                }
            } else {
                owner.removeAll();
            }

            owner.sendMsgNRT(msg);

            return false; // done
        });
}

void aoo_send_accept(AooSendUnit *unit, sc_msg_iter* args){
    unit->delegate().setAccept(args->geti());
}

void aoo_send_format(AooSendUnit *unit, sc_msg_iter* args){
    auto cmd = UnitCmd::create(unit->mWorld, args);
    unit->delegate().doCmd(cmd,
        [](World *world, void *cmdData){
            auto data = (UnitCmd *)cmdData;
            auto& owner = static_cast<AooSend&>(*data->owner);

            sc_msg_iter args(data->size, data->data);
            skipUnitCmd(&args);

            auto replyID = args.geti();

            char buf[256];
            osc::OutboundPacketStream msg(buf, sizeof(buf));
            owner.beginReply(msg, "/aoo/format", replyID);

            aoo_format_storage f;
            int nchannels = static_cast<AooSendUnit&>(owner.unit()).numChannels();
            if (parseFormat(owner.unit(), nchannels, &args, f)){
                if (f.header.nchannels > nchannels){
                    LOG_ERROR("AooSend: 'channel' argument (" << f.header.nchannels
                              << ") out of range");
                    f.header.nchannels = nchannels;
                }
                if (owner.source()->set_format(f.header) > 0){
                    // only send format on success
                    serializeFormat(msg, f.header);
                }
            }

            owner.sendMsgNRT(msg);

            return false; // done
        });
}

void aoo_send_channel(AooSendUnit *unit, sc_msg_iter* args){
    auto cmd = UnitCmd::create(unit->mWorld, args);
    unit->delegate().doCmd(cmd,
        [](World *world, void *cmdData){
            auto data = (UnitCmd *)cmdData;
            auto& owner = static_cast<AooSend&>(*data->owner);

            sc_msg_iter args(data->size, data->data);
            skipUnitCmd(&args);

            aoo::ip_address addr;
            aoo_id id;
            if (getSinkArg(owner.node(), &args, addr, id)){
                auto channelOnset = args.geti();
                owner.source()->set_sink_channelonset(
                    addr.address(), addr.length(), id, channelOnset);
            }

            return false; // done
        });
}

void aoo_send_packetsize(AooSendUnit *unit, sc_msg_iter* args){
    unit->delegate().source()->set_packetsize(args->geti());
}

void aoo_send_ping(AooSendUnit *unit, sc_msg_iter* args){
    int32_t ms = args->getf() * 1000.f;
    unit->delegate().source()->set_ping_interval(ms);
}

void aoo_send_resend(AooSendUnit *unit, sc_msg_iter* args){
    int32_t ms = args->getf() * 1000.f;

    auto cmd = CmdData::create<OptionCmd>(unit->mWorld);
    cmd->i = ms;
    unit->delegate().doCmd(cmd,
        [](World *world, void *cmdData){
            auto data = (OptionCmd *)cmdData;
            auto& owner = static_cast<AooSend&>(*data->owner);
            owner.source()->set_resend_buffersize(data->i);

            return false; // done
        });
}

void aoo_send_redundancy(AooSendUnit *unit, sc_msg_iter* args){
    unit->delegate().source()->set_redundancy(args->geti());
}

void aoo_send_timefilter(AooSendUnit *unit, sc_msg_iter* args){
    unit->delegate().source()->set_timefilter_bandwidth(args->getf());
}

using AooSendUnitCmdFunc = void (*)(AooSendUnit*, sc_msg_iter*);

// make sure that unit commands only run after the instance
// has been fully initialized.
template<AooSendUnitCmdFunc fn>
void runUnitCmd(AooSendUnit* unit, sc_msg_iter* args) {
    if (unit->initialized() && unit->delegate().initialized()) {
        fn(unit, args);
    } else {
        LOG_WARNING("AooSend instance not initialized");
    }
}

#define AooUnitCmd(cmd) \
    DefineUnitCmd("AooSend", "/" #cmd, (UnitCmdFunc)runUnitCmd<aoo_send_##cmd>)

} // namespace

/*////////////////// Setup ///////////////////////*/

void AooSendLoad(InterfaceTable *inTable){
    ft = inTable;

    registerUnit<AooSendUnit>(ft, "AooSend");

    AooUnitCmd(add);
    AooUnitCmd(remove);
    AooUnitCmd(accept);
    AooUnitCmd(format);
    AooUnitCmd(channel);
    AooUnitCmd(packetsize);
    AooUnitCmd(ping);
    AooUnitCmd(resend);
    AooUnitCmd(redundancy);
    AooUnitCmd(timefilter);
}
