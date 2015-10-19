#pragma once


#include "act_function.h"

#include <dnn/protos/sigmoid.pb.h>
#include <dnn/util/fastapprox/fastexp.h>
#include <dnn/util/fastapprox/fastlog.h>

namespace dnn {

/*@GENERATE_PROTO@*/
struct SigmoidC : public Serializable<Protos::SigmoidC> {
    SigmoidC()
    : threshold(0.0)
    , slope(1.0)
    {
    }

    void serial_process() {
        begin() << "slope: " << slope << ", "
                << "threshold: " << threshold << Self::end;
    }


    double threshold;
    double slope;
};


class Sigmoid : public ActFunction<SigmoidC> {
public:
    const string name() const { return "Sigmoid"; }

    double prob(const double &u) {
        return 1.0/(1.0+exp( - c.slope * (u - c.threshold) ));
    }

    double probDeriv(const double &u) {
        double p = prob(u);
        return p*(1-p);
    }
};



}
