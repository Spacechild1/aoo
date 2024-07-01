add_library(aoo_oscpack OBJECT
    # only build required files in "osc" folder
    "osc/OscException.h"
    "osc/OscHostEndianness.h"
    "osc/OscOutboundPacketStream.cpp"
    "osc/OscOutboundPacketStream.h"
    "osc/OscReceivedElements.cpp"
    "osc/OscReceivedElements.h"
    "osc/OscTypes.cpp"
    "osc/OscTypes.h")

target_link_libraries(aoo_oscpack PRIVATE aoo_common)
