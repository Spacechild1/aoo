#include "AooSend.hpp"

#if AOO_USE_OPUS
# include "codec/aoo_opus.h"
#endif

static InterfaceTable *ft;

/*////////////////// AooSend ////////////////*/

void AooSend::init(int32_t port, AooId id) {
    auto data = CmdData::create<OpenCmd>(world());
    if (data) {
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
                auto node = INode::get(cmd->port);
                if (!node) {
                    return false;
                }
                auto source = AooSource::create(cmd->id);
                source->setEventHandler(
                    [](void *user, const AooEvent *event, int32_t) {
                        static_cast<AooSend *>(user)->handleEvent(event);
                    }, cmd->owner.get(), kAooEventModePoll);

                if (node->client()->addSource(source.get()) == kAooOk) {
                    source->setup(cmd->numChannels, cmd->sampleRate, cmd->blockSize,
                                  kAooFixedBlockSize);

                    source->setBufferSize(DEFBUFSIZE);

                    AooFormatStorage f;
                    makeDefaultFormat(f, cmd->sampleRate, cmd->blockSize, cmd->numChannels);
                    source->setFormat(f.header);

                    cmd->node = std::move(node);
                    cmd->obj = source.release();

                    return true; // success!
                } else {
                    LOG_ERROR("aoo source with ID " << cmd->id << " on port "
                              << node->port() << " already exists");
                    return false;
                }
            },
            [](World *world, void *data) {
                auto cmd = (OpenCmd *)data;
                auto& owner = static_cast<AooSend&>(*cmd->owner);
                owner.source_.reset(cmd->obj);
                owner.setNode(std::move(cmd->node));
                LOG_DEBUG("AooSend initialized");
                return false; // done
            }
        );
    }
}

void AooSend::onDetach() {
    auto data = CmdData::create<CmdData>(world());
    doCmd(data,
        [](World *world, void *data) {
            // release in NRT thread
            auto cmd = (CmdData*)data;
            auto& owner = static_cast<AooSend&>(*cmd->owner);
            auto node = owner.node();
            if (node) {
                // release node
                node->client()->removeSource(owner.source());
                owner.setNode(nullptr);
            }
            // release source
            owner.source_ = nullptr;
            return false; // done
        }
    );
}

void AooSend::handleEvent(const AooEvent *event){
    char buf[256];
    osc::OutboundPacketStream msg(buf, 256);

    switch (event->type) {
    case kAooEventSinkPing:
    {
        auto& e = event->sinkPing;
        auto delta1 = aoo_ntpTimeDuration(e.t1, e.t2);
        auto delta2 = aoo_ntpTimeDuration(e.t3, e.t4);
        auto total_rtt = aoo_ntpTimeDuration(e.t1, e.t4);
        auto network_rtt = total_rtt - aoo_ntpTimeDuration(e.t2, e.t3);

        beginEvent(msg, "ping", e.endpoint)
            << delta1 << delta2 << network_rtt << total_rtt << e.packetLoss;
        sendMsgRT(msg);
        break;
    }
    case kAooEventSinkAdd:
    {
        beginEvent(msg, "add", event->sinkAdd.endpoint);
        sendMsgRT(msg);
        break;
    }
    case kAooEventSinkRemove:
    {
        beginEvent(msg, "remove", event->sinkRemove.endpoint);
        sendMsgRT(msg);
        break;
    }
    case kAooEventInvite:
    {
        auto& e = event->invite;
        if (autoInvite_) {
            // accept by default
            source_->handleInvite(e.endpoint, e.token, true);
        } else {
            beginEvent(msg, "invite", e.endpoint) << e.token;
            serializeData(msg, e.metadata);
            sendMsgRT(msg);
        }
        break;
    }
    case kAooEventUninvite:
    {
        auto& e = event->uninvite;
        if (autoInvite_) {
            // accept by default
            source_->handleUninvite(e.endpoint, e.token, true);
        } else {
            beginEvent(msg, "uninvite", e.endpoint) << e.token;
            sendMsgRT(msg);
        }
        break;
    }
    case kAooEventFrameResend:
    {
        beginEvent(msg, "frameResent", event->frameResend.endpoint)
            << event->frameResend.count;
        sendMsgRT(msg);
        break;
    }
    default:
        LOG_DEBUG("AooSend: got unknown event " << event->type);
        break;
    }
}

bool AooSend::addSink(const aoo::ip_address& addr, AooId id, bool active) {
    AooEndpoint ep { addr.address(), (AooAddrSize)addr.length(), id };
    return source()->addSink(ep, active) == kAooOk;
}

bool AooSend::removeSink(const aoo::ip_address& addr, AooId id){
    AooEndpoint ep { addr.address(), (AooAddrSize)addr.length(), id };
    return source()->removeSink(ep) == kAooOk;
}

void AooSend::removeAll(){
    source()->removeAll();
}

/*////////////////// AooSendUnit ////////////////*/

AooSendUnit::AooSendUnit() {
    int32_t port = in0(portIndex);
    AooId id = in0(idIndex);
    lastGate_ = in0(gateIndex);
    numChannels_ = in0(channelIndex);
    assert(numChannels_ >= 0 &&
           numChannels_ <= (numInputs() - bufferIndex));
    auto delegate = rt::make_shared<AooSend>(mWorld, *this);
    if (delegate) {
        delegate->init(port, id);
        delegate_ = std::move(delegate);
        set_calc_function<AooSendUnit, &AooSendUnit::next>();
    } else {
        LOG_ERROR("RTAlloc() failed");
        mCalcFunc = ClearUnitOutputs;                                                                                        \
        mDone = true;
    }
}

void AooSendUnit::next(int numSamples){
    auto source = delegate().source();
    if (source) {
        // check if gate has changed
        auto rate = mInput[gateIndex]->mCalcRate;
        if (rate == calc_FullRate) {
            // audio rate -> sample accurate
            auto prev = lastGate_;
            auto buf = in(gateIndex);
            for (int i = 0; i < numSamples; ++i) {
                auto gate = buf[i];
                if (gate != prev) {
                    if (gate != 0.0) {
                        delegate().source()->startStream(i, nullptr);
                    } else {
                        delegate().source()->stopStream(i);
                    }
                    prev = gate;
                }
            }
            lastGate_ = prev;
        } else if (rate == calc_BufRate) {
            // control rate -> quantized to block boundaries
            auto gate = in0(gateIndex);
            if (gate != lastGate_) {
                if (gate != 0.0) {
                    delegate().source()->startStream(0, nullptr);
                } else {
                    delegate().source()->stopStream(0);
                }
                lastGate_ = gate;
            }
        }

        uint64_t t = getOSCTime(mWorld);
        auto vec = mInBuf + bufferIndex;

        if (source->process(vec, numSamples, t) == kAooOk){
            delegate().node()->notify();
        }

        source->pollEvents();
    }
}

/*//////////////////// Unit commands ////////////////////*/

namespace {

void aoo_send_add(AooSendUnit *unit, sc_msg_iter* args) {
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
            AooId id;
            if (owner.node()->getSinkArg(&args, addr, id)){
                auto active = args.geti();
                // only send IP address on success
                if (owner.addSink(addr, id, active)) {
                    msg << (int32_t)1 << addr.name() << addr.port() << id;

                    owner.sendMsgNRT(msg);

                    return false; // success! (done)
                }
            }

            msg << (int32_t)0;
            owner.sendMsgNRT(msg);

            return false; // failure! (done)
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
                AooId id;
                if (owner.node()->getSinkArg(&args, addr, id)){
                    if (!owner.removeSink(addr, id)) {
                        msg << (int32_t)0;
                        owner.sendMsgNRT(msg);
                        return false; // failure! (done)
                    }
                    // only send IP address on success
                    msg << (int32_t)1 << addr.name() << addr.port() << id;
                }
            } else {
                msg << (int32_t)1;
                owner.removeAll();
            }

            owner.sendMsgNRT(msg);

            return false; // success! (done)
        });
}

void aoo_send_auto_invite(AooSendUnit *unit, sc_msg_iter* args){
    unit->delegate().setAutoInvite(args->geti());
}

void aoo_send_format(AooSendUnit *unit, sc_msg_iter* args) {
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

            AooFormatStorage f;
            int nchannels = static_cast<AooSendUnit&>(owner.unit()).numChannels();
            if (parseFormat(owner.unit(), nchannels, &args, f)){
                if (f.header.numChannels > nchannels){
                    LOG_ERROR("AooSend: 'channel' argument (" << f.header.numChannels
                              << ") out of range");
                    f.header.numChannels = nchannels;
                }
                auto err = owner.source()->setFormat(f.header);
                if (err == kAooOk) {
                    // only send format on success
                    msg << (int32_t)1;
                    serializeFormat(msg, f.header);

                    owner.sendMsgNRT(msg);

                    return false; // success! (done)
                } else {
                    LOG_ERROR("AooSend: couldn't set format: " << aoo_strerror(err));
                }
            }

            msg << (int32_t)0;
            owner.sendMsgNRT(msg);

            return false; // failure! (done)
        });
}

void aoo_send_start(AooSendUnit *unit, sc_msg_iter* args) {
    auto offset = unit->mWorld->mSampleOffset;
    auto md = parseData(args);
    unit->delegate().source()->startStream(offset, md ? &md.value() : nullptr);
}

void aoo_send_stop(AooSendUnit *unit, sc_msg_iter* args) {
    auto offset = unit->mWorld->mSampleOffset;
    unit->delegate().source()->stopStream(offset);
}

#if AOO_USE_OPUS

bool get_opus_bitrate(AooSource *src, osc::OutboundPacketStream& msg) {
    // NOTE: because of a bug in opus_multistream_encoder (as of opus v1.3.2)
    // OPUS_GET_BITRATE always returns OPUS_AUTO
    opus_int32 value;
    auto err = AooSource_getOpusBitrate(src, 0, &value);
    if (err != kAooOk){
        LOG_ERROR("could not get bitrate: " << aoo_strerror(err));
        return false;
    }
    switch (value){
    case OPUS_AUTO:
        msg << "auto";
        break;
    case OPUS_BITRATE_MAX:
        msg << "max";
        break;
    default:
        msg << (float)(value * 0.001); // bit -> kBit
        break;
    }
    return true;
}

void set_opus_bitrate(AooSource *src, sc_msg_iter &args) {
    // "auto", "max" or number
    opus_int32 value;
    if (args.nextTag() == 's'){
        const char *s = args.gets();
        if (!strcmp(s, "auto")){
            value = OPUS_AUTO;
        } else if (!strcmp(s, "max")){
            value = OPUS_BITRATE_MAX;
        } else {
            LOG_ERROR("bad bitrate argument '" << s << "'");
            return;
        }
    } else {
        opus_int32 bitrate = args.getf() * 1000.0; // kBit -> bit
        if (bitrate > 0){
            value = bitrate;
        } else {
            LOG_ERROR("bitrate argument " << bitrate << " out of range");
            return;
        }
    }
    auto err = AooSource_setOpusBitrate(src, 0, value);
    if (err != kAooOk){
        LOG_ERROR("could not set bitrate: " << aoo_strerror(err));
    }
}

bool get_opus_complexity(AooSource *src, osc::OutboundPacketStream& msg){
    opus_int32 value;
    auto err = AooSource_getOpusComplexity(src, 0, &value);
    if (err != kAooOk){
        LOG_ERROR("could not get complexity: " << aoo_strerror(err));
        return false;
    }
    msg << value;
    return true;
}

void set_opus_complexity(AooSource *src, sc_msg_iter &args){
    // 0-10
    opus_int32 value = args.geti();
    if (value < 0 || value > 10){
        LOG_ERROR("complexity value " << value << " out of range");
        return;
    }
    auto err = AooSource_setOpusComplexity(src, 0, value);
    if (err != kAooOk){
        LOG_ERROR("could not set complexity: " << aoo_strerror(err));
    }
}

bool get_opus_signal(AooSource *src, osc::OutboundPacketStream& msg){
    opus_int32 value;
    auto err = AooSource_getOpusSignalType(src, 0, &value);
    if (err != kAooOk){
        LOG_ERROR("could not get signal type: " << aoo_strerror(err));
        return false;
    }
    switch (value){
    case OPUS_SIGNAL_MUSIC:
        msg << "music";
        break;
    case OPUS_SIGNAL_VOICE:
        msg << "voice";
        break;
    default:
        msg << "auto";
        break;
    }
    return true;
}

void set_opus_signal(AooSource *src, sc_msg_iter &args){
    // "auto", "music", "voice"
    opus_int32 value;
    const char *s = args.gets();
    if (!strcmp(s, "auto")) {
        value = OPUS_AUTO;
    } else if (!strcmp(s, "music")) {
        value = OPUS_SIGNAL_MUSIC;
    } else if (!strcmp(s, "voice")) {
        value = OPUS_SIGNAL_VOICE;
    } else {
        LOG_ERROR("unsupported signal type '" << s << "'");
        return;
    }
    auto err = AooSource_setOpusSignalType(src, 0, value);
    if (err != kAooOk){
        LOG_ERROR("could not set signal type: " << aoo_strerror(err));
    }
}

#endif

void aoo_send_codec_set(AooSendUnit *unit, sc_msg_iter* args){
    auto cmd = UnitCmd::create(unit->mWorld, args);
    unit->delegate().doCmd(cmd,
        [](World *world, void *cmdData){
            auto data = (UnitCmd *)cmdData;
            auto& owner = static_cast<AooSend&>(*data->owner);

            sc_msg_iter args(data->size, data->data);
            skipUnitCmd(&args);

            auto codec = args.gets();
            auto param = args.gets();

        #if AOO_USE_OPUS
            if (!strcmp(codec, "opus")) {
                if (!strcmp(param, "bitrate")){
                    set_opus_bitrate(owner.source(), args);
                    return false; // done
                } else if (!strcmp(param, "complexity")){
                    set_opus_complexity(owner.source(), args);
                    return false; // done
                } else if (!strcmp(param, "signal")){
                    set_opus_signal(owner.source(), args);
                    return false; // done
                }
            }
        #endif

            LOG_ERROR("unknown parameter '" << param
                      << "' for codec '" << codec << "'");

            return false; // done
        });
}

void aoo_send_codec_get(AooSendUnit *unit, sc_msg_iter* args){
    auto cmd = UnitCmd::create(unit->mWorld, args);
    unit->delegate().doCmd(cmd,
        [](World *world, void *cmdData){
            auto data = (UnitCmd *)cmdData;
            auto& owner = static_cast<AooSend&>(*data->owner);

            sc_msg_iter args(data->size, data->data);
            skipUnitCmd(&args);

            auto replyID = args.geti();
            auto codec = args.gets();
            auto param = args.gets();

            char buf[256];
            osc::OutboundPacketStream msg(buf, sizeof(buf));
            owner.beginReply(msg, "/aoo/codec/get", replyID);
            msg << (int32_t)1 << codec << param;

            // TODO: what to send on failure?
        #if AOO_USE_OPUS
            if (!strcmp(codec, "opus")){
                if (!strcmp(param, "bitrate")){
                    if (get_opus_bitrate(owner.source(), msg)){
                        goto codec_sendit;
                    } else {
                        return false;
                    }
                } else if (!strcmp(param, "complexity")){
                    if (get_opus_complexity(owner.source(), msg)){
                        goto codec_sendit;
                    } else {
                        return false;
                    }
                } else if (!strcmp(param, "signal")){
                    if (get_opus_signal(owner.source(), msg)){
                        goto codec_sendit;
                    } else {
                        return false;
                    }
                }
            }
        #endif
            // failure
            msg.Clear();
            owner.beginReply(msg, "/aoo/codec/get", replyID);
            msg << (int32_t)0 << codec << param;
codec_sendit:
            owner.sendMsgNRT(msg);

            return false; // done
        });
}

void aoo_send_channel_offset(AooSendUnit *unit, sc_msg_iter* args) {
    aoo::ip_address addr;
    AooId id;
    if (unit->delegate().node()->getSinkArg(args, addr, id)) {
        auto offset = args->geti();
        AooEndpoint ep { addr.address(), (AooAddrSize)addr.length(), id };
        unit->delegate().source()->setSinkChannelOffset(ep, offset);
    }
}

void aoo_send_activate(AooSendUnit *unit, sc_msg_iter* args) {
    aoo::ip_address addr;
    AooId id;
    if (unit->delegate().node()->getSinkArg(args, addr, id)) {
        auto offset = args->geti();
        AooEndpoint ep { addr.address(), (AooAddrSize)addr.length(), id };
        unit->delegate().source()->activate(ep, offset);
    }
}

void aoo_send_packet_size(AooSendUnit *unit, sc_msg_iter* args){
    unit->delegate().source()->setPacketSize(args->geti());
}

void aoo_send_ping(AooSendUnit *unit, sc_msg_iter* args){
    unit->delegate().source()->setPingInterval(args->getf());
}

void aoo_send_resend(AooSendUnit *unit, sc_msg_iter* args){
    auto cmd = CmdData::create<OptionCmd>(unit->mWorld);
    cmd->f = args->getf();
    unit->delegate().doCmd(cmd,
        [](World *world, void *cmdData){
            auto data = (OptionCmd *)cmdData;
            auto& owner = static_cast<AooSend&>(*data->owner);
            owner.source()->setResendBufferSize(data->f);

            return false; // done
        });
}

void aoo_send_redundancy(AooSendUnit *unit, sc_msg_iter* args){
    unit->delegate().source()->setRedundancy(args->geti());
}

void aoo_send_dynamic_resampling(AooSendUnit *unit, sc_msg_iter* args){
    unit->delegate().source()->setDynamicResampling(args->geti());
}

void aoo_send_dll_bw(AooSendUnit *unit, sc_msg_iter* args){
    unit->delegate().source()->setDllBandwidth(args->getf());
}

using AooSendUnitCmdFunc = void (*)(AooSendUnit*, sc_msg_iter*);

// make sure that unit commands only run after the instance
// has been successfully initialized.
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
    AooUnitCmd(format);
    AooUnitCmd(start);
    AooUnitCmd(stop);
    AooUnitCmd(auto_invite);
    AooUnitCmd(codec_set);
    AooUnitCmd(codec_get);
    AooUnitCmd(activate);
    AooUnitCmd(channel_offset);
    AooUnitCmd(packet_size);
    AooUnitCmd(ping);
    AooUnitCmd(resend);
    AooUnitCmd(redundancy);
    AooUnitCmd(dynamic_resampling);
    AooUnitCmd(dll_bw);
}
