#include "coreir/libs/aetherlinglib.h"
#include "coreir/libs/commonlib.h"
#include <stdio.h>

using namespace std;
using namespace CoreIR;

void Aetherling_createZipGenerator(Context* c) {

    Namespace* aetherlinglib = c->getNamespace(AETHERLING_NAMESPACE);
    
    /*
     * width - the width in bits of each input
     * parallelOperatrs - how many operators to have in parallel
     * operator - the operator to parallelize. Note that it must have one input known as "in" and 
     * one output known as "out"
     */
    Params zip2params = Params({
            //{"width", c->Int()},
            {"numInputs", c->Int()},
            {"input0Type", CoreIRType::make(c)},
            {"input1Type", CoreIRType::make(c)}
        });

    aetherlinglib->newTypeGen(
        "zip2_type", // name for typegen
        zip2params, // generator parameters
        [](Context* c, Values genargs) { //Function to compute type
            //uint width = genargs.at("width")->get<int>();
            uint numInputs = genargs.at("numInputs")->get<int>();
            Type* input0Type = genargs.at("input0Type")->get<Type*>();
            Type* input1Type = genargs.at("input1Type")->get<Type*>();
            return c->Record({
                    {"in0", input0Type->Arr(numInputs)},
                    {"in1", input1Type->Arr(numInputs)},
                    {"out", c->Record({
                                {"el0", input0Type->getFlipped()},
                                {"el1", input1Type->getFlipped()}
                            })->Arr(numInputs)}
                });
        });

    Generator* zip2 =
        aetherlinglib->newGeneratorDecl("zip2", aetherlinglib->getTypeGen("zip2_type"), zip2params);

    zip2->setGeneratorDefFromFun([](Context* c, Values genargs, ModuleDef* def) {
            uint numInputs = genargs.at("numInputs")->get<int>();
            /*
            Type* input0Type = genargs.at("input0Type")->get<Type*>();
            Type* input1Type = genargs.at("input1Type")->get<Type*>();
            assert(numInputs>0);
            RecordType* input0TypeRecord = dynamic_cast<RecordType*>(input0Type);
            assert(input0TypeRecord != 0); // 0 if cast failed, so input type wasn't RecordType
            RecordType* input1TypeRecord = dynamic_cast<RecordType*>(input1Type);
            assert(input1TypeRecord != 0); // 0 if cast failed, so input type wasn't RecordType
            */
            // now create each op and wire the inputs and outputs to it
            for (uint i = 0; i < numInputs; i++) {
                string idxStr = to_string(i);
                def->connect("self.in0." + idxStr, "self.out." + idxStr + ".el0");
                def->connect("self.in1." + idxStr, "self.out." + idxStr + ".el1");
            }
        });
}
