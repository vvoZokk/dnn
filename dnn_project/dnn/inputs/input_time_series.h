#pragma once

#include "input.h"

#include <dnn/protos/input_time_series.pb.h>
#include <dnn/io/stream.h>

namespace dnn {

/*@GENERATE_PROTO@*/
struct InputTimeSeriesC : public Serializable<Protos::InputTimeSeriesC> {
    InputTimeSeriesC() : dt(1.0) {}

    double dt;

    void serial_process() {
        begin() << "dt: " << dt << Self::end;
    }
};

/*@GENERATE_PROTO@*/
struct InputTimeSeriesState : public Serializable<Protos::InputTimeSeriesState> {
    InputTimeSeriesState() : index(0), _t(0) {}
    size_t index;
    double _t;
    void serial_process() {
        begin() << "index: " << index << Self::end;
    }
};


class InputTimeSeries : public Input<InputTimeSeriesC, InputTimeSeriesState> {
public:
    typedef Input<InputTimeSeriesC, InputTimeSeriesState> Parent;

    const string name() const {
        return "InputTimeSeries";
    }

	const double& getValue(const Time &t) {
        s._t += t.dt;
        if(fmod(s._t, c.dt) > 0.0001) return InputBase::def_value;
        size_t id = localId();
        if(ts->dim() == 1) return ts.ref().data[0].values[s.index++];;
        if(id>=ts->dim()) {
            throw dnnException() << "Got input spike sequence less than neuron count\n";
        }
        return ts.ref().data[id].values[s.index++];;
	}

    void setAsInput(Ptr<SerializableBase> b) {
        s.index = 0;
        if(ts.ptr()) {
            throw dnnException() << "Trying to set input time series two times\n";
        }
        ts.set(b.as<TimeSeries>().ptr());
    }

    double getSimDuration() {
        if(ts.isSet()) {
            return ts.ref().length() * c.dt;
        }
        return 0.0;
    }
private:
    vector<double> ts;
    // InterfacedPtr<TimeSeries> ts;
};



}
