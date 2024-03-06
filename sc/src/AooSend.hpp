#include "Aoo.hpp"

#include "aoo_source.hpp"

// for hardware buffer sizes up to 2048 @ 44.1 kHz
#define DEFBUFSIZE 0.05

using OpenCmd = OpenCmd_<AooSource>;

/*////////////////// AooSend ////////////////*/

class AooSendUnit;

class AooSend final : public AooDelegate {
public:
    using AooDelegate::AooDelegate;

    void init(int32_t port, AooId id);

    void onDetach() override;

    void handleEvent(const AooEvent *event);

    AooSource * source() { return source_.get(); }

    bool addSink(const aoo::ip_address& addr, AooId id, bool active);
    bool removeSink(const aoo::ip_address& addr, AooId id);
    void removeAll();

    void setAutoInvite(bool b){
        autoInvite_ = b;
    }
private:
    AooSource::Ptr source_;
    bool autoInvite_ = true;
};

/*////////////////// AooSendUnit ////////////////*/

class AooSendUnit final : public AooUnit {
public:
    AooSendUnit();

    void next(int numSamples);

    AooSend& delegate() {
        return static_cast<AooSend&>(*delegate_);
    }

    int numChannels() const {
        return numInputs() - channelOnset_;
    }
private:
    static const int channelOnset_ = 3;

    int playing_ = -1;
};

