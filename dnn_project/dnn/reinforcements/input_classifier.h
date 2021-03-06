#pragma once


#include "reinforcement.h"

#include <dnn/protos/input_classifier.pb.h>

namespace dnn {

/*@GENERATE_PROTO@*/
struct InputClassifierC : public Serializable<Protos::InputClassifierC> {
    InputClassifierC()
    : ltp(1.0)
    , ltd(-1.0)
    , reaction_to_noise(-1.0)
    {
    }

    void serial_process() {
        begin() << "ltp: " << ltp << ", "
                << "ltd: " << ltd << ", "
                << "reaction_to_noise: " << reaction_to_noise << Self::end;
    }

    double ltp;
    double ltd;
    double reaction_to_noise;
};


class InputClassifier : public Reinforcement<InputClassifierC> {
public:
    void modulateReward() {
        if(n->fired()) {
            Maybe<size_t> currentClassId = GlobalCtx::inst().getCurrentClassId();
            if (currentClassId) {
                if (n->localId() == currentClassId.getRef()) {
                    GlobalCtx::inst().propagateReward(c.ltp);
                } else {
                    GlobalCtx::inst().propagateReward(c.ltd);
                }
            } else {
                // unlabeled sample
                GlobalCtx::inst().propagateReward(c.reaction_to_noise);
            }
        }
    }
};



}