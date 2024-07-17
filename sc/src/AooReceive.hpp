#include "Aoo.hpp"

#include "aoo_sink.hpp"

#define DEFAULT_LATENCY 0.050

using OpenCmd = OpenCmd_<AooSink>;

/*////////////////// AooReceive ////////////////*/

class AooReceive final : public AooDelegate {
public:
    using AooDelegate::AooDelegate;

    void init(int32_t port, AooId id, AooSeconds latency);

    void onDetach() override;

    void handleEvent(const AooEvent *event);

    AooSink* sink() { return sink_.get(); }
private:
    AooSink::Ptr sink_;
};

/*////////////////// AooReceiveUnit ////////////////*/

class AooReceiveUnit final : public AooUnit {
public:
    AooReceiveUnit();

    void next(int numSamples);

    AooReceive& delegate() {
        return static_cast<AooReceive&>(*delegate_);
    }

    int numChannels() const { return numChannels_; }
private:
    static const int portIndex = 0;
    static const int idIndex = 1;
    static const int latencyIndex = 2;
    static const int channelIndex = 3;

    int numChannels_ = 0;
};
