

#include "spike_neuron.h"


namespace dnn {


size_t global_neuron_index = 0;

const size_t& SpikeNeuronBase::id() const {
		return _id;
	}
void SpikeNeuronBase::setCoordinates(size_t xi, size_t yi, size_t colSize) {
	_xi = xi;
	_yi = yi;
	_colSize = colSize;
}
const size_t SpikeNeuronBase::localId() const {
	return xi() + colSize()*yi();
}
const size_t& SpikeNeuronBase::xi() const {
	return _xi;
}
const size_t& SpikeNeuronBase::yi() const {
	return _yi;
}
const size_t& SpikeNeuronBase::colSize() const {
	return _colSize;
}
const double& SpikeNeuronBase::axonDelay() const {
	return _axonDelay;
}
double& SpikeNeuronBase::mutAxonDelay() {
	return _axonDelay;
}
const bool& SpikeNeuronBase::fired() const {
	return _fired;
}

void SpikeNeuronBase::setFired(const bool& f) {
	_fired = f;
}

void SpikeNeuronBase::resetInternal() {
	reset();
	if(lrule.isSet()) {
		lrule.ref().reset();
	}
	if(input.isSet()) {
		input.ref().reset();
	}
	for(auto &s: syns) {
		s.ref().reset();
	}
}


// runtime
void SpikeNeuronBase::propagateSynapseSpike(const SynSpike &sp) {
    syns[ sp.syn_id ].ifc().propagateSpike();
    lrule.ifc().propagateSynapseSpike(sp);
}

void SpikeNeuronBase::setLearningRule(Ptr<LearningRuleBase> _lrule) {
	lrule = _lrule;
	lrule.ref().linkWithNeuron(*this);
}
InterfacedPtr<LearningRuleBase>& SpikeNeuronBase::getLearningRule() {
	return lrule;
}

void SpikeNeuronBase::setActFunction(Ptr<ActFunctionBase> _act_f) {
	act_f = _act_f;
}
const InterfacedPtr<ActFunctionBase>& SpikeNeuronBase::getActFunction() const {
	return act_f;
}

void SpikeNeuronBase::setInput(Ptr<InputBase> _input) {
	input = _input;
}
bool SpikeNeuronBase::inputIsSet() {
	return input.isSet();
}
InputBase& SpikeNeuronBase::getInput() {
	if(!input.isSet()) {
		throw dnnException() << "Trying to get input which is not set\n";
	}
	return input.ref();
}

void SpikeNeuronBase::addSynapse(InterfacedPtr<SynapseBase> syn) {
	syns.push_back(syn);
}

const ActVector<InterfacedPtr<SynapseBase>>& SpikeNeuronBase::getSynapses() const {
	return syns;
}
ActVector<InterfacedPtr<SynapseBase>>& SpikeNeuronBase::getMutSynapses() {
	return syns;
}
Statistics SpikeNeuronBase::getStat() {
	Statistics statc = stat;
	auto& rstat = statc.getStats();
	auto it = rstat.begin();
	while(it != rstat.end()) {
		if(!strStartsWith(it->first, name())) {
			rstat[name() + "_" + it->first ] = it->second;
			it = rstat.erase(it);
		} else {
			++it;
		}
	}

	size_t syn_id = 0;
	for(auto &s: syns) {
		if(s.ref().getStat().on()) {
			Statistics& syn_st = s.ref().getStat();
			for(auto it=syn_st.getStats().begin(); it != syn_st.getStats().end(); ++it) {
				rstat[s.ref().name() + "_" +  it->first + "_" + std::to_string(syn_id)] = it->second;
			}
		}
		++syn_id;
	}
	if((lrule.isSet())&&(lrule.ref().getStat().on())) {
		Statistics &lrule_st = lrule.ref().getStat();
		for(auto it=lrule_st.getStats().begin(); it != lrule_st.getStats().end(); ++it) {
			rstat[ lrule.ref().name() + "_" + it->first ] = it->second;
		}

	}
	if((lrule.isSet())&&(lrule.ref().getWeightNormalization().isSet())&&(lrule.ref().getWeightNormalization().ref().getStat().on())) {
		Statistics &lrule_st = lrule.ref().getWeightNormalization().ref().getStat();
		for(auto it=lrule_st.getStats().begin(); it != lrule_st.getStats().end(); ++it) {
			rstat[ lrule.ref().getWeightNormalization().ref().name() + "_" + it->first ] = it->second;
		}
	}
	return statc;
}

void SpikeNeuronBase::enqueueSpike(const SynSpike && sp) {
	while (input_queue_lock.test_and_set(std::memory_order_acquire));
	input_spikes.push(sp);
	input_queue_lock.clear(std::memory_order_release);
}

void SpikeNeuronBase::readInputSpikes(const Time &t) {
	while (input_queue_lock.test_and_set(std::memory_order_acquire)) {}
    while(!input_spikes.empty()) {
        const SynSpike& sp = input_spikes.top();
        if(sp.t >= t.t) break;
        auto &s = syns[sp.syn_id];
        s.ref().setFired(true);
        // cout << s.ref().idPre() << " -> " << id() << " spiked at " << t.t << "\n";
        ifc.propagateSynapseSpike(sp);
        input_spikes.pop();
    }
    input_queue_lock.clear(std::memory_order_release);
}
void SpikeNeuronBase::calculateDynamicsInternal(const Time &t) {
    readInputSpikes(t);

    const double& Iinput = input.ifc().getValue(t);

	double Isyn = 0.0;
    auto syn_id_it = syns.ibegin();
    while(syn_id_it != syns.iend()) {
        auto &s = syns[syn_id_it];
        double x = s.ref().getWeightedPotential();
        if(fabs(x) < 0.0001) {
        	syns.setInactive(syn_id_it);
        } else {
        	Isyn += x;
        	++syn_id_it;
        }
    }
    ifc.calculateDynamics(t, Iinput, Isyn);

    lrule.ifc().calculateDynamicsInternal(t);

    if(stat.on()) {
   		for(auto &s: syns) {
   			s.ifc().calculateDynamics(t);
        	s.ref().setFired(false);
   		}
    } else {
        for(auto syn_id_it = syns.ibegin(); syn_id_it != syns.iend(); ++syn_id_it) {
            auto &s = syns[syn_id_it];
            s.ifc().calculateDynamics(t);
            s.ref().setFired(false);
        }
    }
}

double SpikeNeuronBase::getSimDuration() {
	if(input.isSet()) {
		return input.ref().getSimDuration();
	}
	return 0.0;
}



}