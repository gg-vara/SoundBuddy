#include "ShoutPayloadConsumer.h"

ShoutConsumeResult ShoutPayloadConsumer::consume(const ShoutPayload &incomingPayload) {
    auto output = ShoutConsumeResult{};

    //
    const auto currHash = incomingPayload.hasChangedHash(); // hash blueprint    
    output.hasMeaningfulChange = lastHasChangedHash != currHash; // check if strings are identical
    lastHasChangedHash = currHash; // replace old hash with new

    //
    output.parsingResult = incomingPayload.toJSON();

    //
    return output;
}
