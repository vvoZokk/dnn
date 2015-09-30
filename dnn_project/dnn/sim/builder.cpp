#include "builder.h"

#include <dnn/base/factory.h>
#include <dnn/neurons/spike_neuron.h>

#include <dnn/util/log/log.h>


namespace dnn {

vector<InterfacedPtr<SpikeNeuronBase>> Builder::buildNeurons() {
    vector<Layer> layers;
    for (const string &lc : c.sim_conf.layers) {
        Layer layer;
        Document layer_conf = Json::parseStringC(lc);
        size_t layer_size = Json::getUintVal(layer_conf, "size");
        size_t col_size = ceil(sqrt(layer_size));
        size_t xi = 0;
        size_t yi = 0;
        for (size_t ni = 0; ni < layer_size; ++ni) {
            InterfacedPtr<SpikeNeuronBase> n;

            if (input_stream) {
                n = input_stream->read<SpikeNeuronBase>();
            } else {
                n = buildObjectFromConstants<SpikeNeuronBase>(Json::getStringVal(layer_conf, "neuron"), c.neurons);
                n.ref().mutAxonDelay() = Json::getDoubleValDef(layer_conf, "axon_delay", 0.0);

                const string act_function = Json::getStringValDef(layer_conf, "act_function", "");
                if (!act_function.empty()) {
                    n.ref().setActFunction(buildObjectFromConstants<ActFunctionBase>(act_function, c.act_functions));
                }
                const string learning_rule = Json::getStringValDef(layer_conf, "learning_rule", "");
                if (!learning_rule.empty()) {
                    n.ref().setLearningRule(buildObjectFromConstants<LearningRuleBase>(learning_rule, c.learning_rules));
                    const string weight_normalization = Json::getStringValDef(layer_conf, "weight_normalization", "");
                    if(!weight_normalization.empty()) {
                        n.ref().getLearningRule().ref().setWeightNormalization(buildObjectFromConstants<WeightNormalizationBase>(weight_normalization, c.weight_normalizations));
                    }
                }
                const string input = Json::getStringValDef(layer_conf, "input", "");
                if (!input.empty()) {
                    n.ref().setInput(buildObjectFromConstants<InputBase>(input, c.inputs));
                }
                n.ref().setCoordinates(xi, yi, col_size);
                xi++;
                if(xi % col_size == 0) {
                    yi++;
                    xi = 0;
                }
                if(n.ref().inputIsSet()) {
                    n.ref().getInput().setLocalId(n.ref().localId());
                }
            }
            layer.neurons.push_back(n);
        }
        layers.push_back(layer);
    }

    for (auto it = c.sim_conf.files.begin(); it != c.sim_conf.files.end(); ++it) {
        const string &obj_name = it->first;
        Document file_conf = Json::parseStringC(it->second);

        string fname = Json::getStringVal(file_conf, "filename");
        if(strStartsWith(fname, "@")) {
            continue;
        }
        // L_DEBUG << "Builder, found input " << fname << " file for " << obj_name;
        Ptr<SerializableBase> obj = Factory::inst().getCachedObject(fname);

        auto slice = Factory::inst().getObjectsSlice(obj_name);
        for(auto it=slice.first; it != slice.second; ++it) {
            Ptr<SerializableBase> o = Factory::inst().getObject(it);
            // L_DEBUG << "Builder, providing input for " << o->name();
            o->setAsInput(obj);
        }
    }


    if (!input_stream) {
        for (auto it = c.sim_conf.conn_map.begin(); it != c.sim_conf.conn_map.end(); ++it) {
            size_t l_id_pre = it->first.first;
            size_t l_id_post = it->first.second;
            const string conn_conf = it->second;
            if (l_id_pre >= layers.size()) {
                throw dnnException()<< "Can't find layer with id " << l_id_pre << "\n";
            }
            if (l_id_post >= layers.size()) {
                throw dnnException()<< "Can't find layer with id " << l_id_post << "\n";
            }
            connectLayers(layers[l_id_pre], layers[l_id_post], conn_conf);
        }
    }

    vector<InterfacedPtr<SpikeNeuronBase>> neurons;
    for (auto &l : layers) {
        neurons.insert(neurons.end(), l.neurons.begin(), l.neurons.end());
    }
    return neurons;
}


}