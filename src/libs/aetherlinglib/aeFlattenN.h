#include "coreir/libs/aetherlinglib.h"
#include "coreir/libs/commonlib.h"
#include <stdio.h>
#include <iostream>

using namespace std;
using namespace CoreIR;

uint getFlattenedSize(Context* c, Type* containerType, Type* innerType) {
    string errorString = "The type you are trying to flatten from doesn't contain the type you are trying to flatten to, container: " + containerType->toString() + ", inner: " + innerType->toString();
    // strip off dimensions until you get to the type equal to the innerType
    uint flattenedSize = 1;
    // do this for comparing the two
    Type* outContainerType = c->Out(containerType);
    ArrayType* curAsArray;
    printf("1\n");
    for (
        // must do this cast later as curType may not always be an arrayType
        Type* curType = outContainerType;
        curType != innerType;
        curType = curAsArray->getElemType()) {
        // If this cast fails, its probably because containerType wasn't an array (to some degree) of innerType
        printf("a\n");
        curType->print();
        innerType->print();
        if (curType->isBaseType()) {
            cerr << errorString << endl;
            assert(!curType->isBaseType()); //"The type you are trying to flatten from doesn't contain the type you are trying to flatten to");
        }
        curAsArray = dynamic_cast<ArrayType*>(curType);
        printf("b\n");
        flattenedSize *= curAsArray->getLen();
        printf("c\n");
    }
    printf("2\n");
    return flattenedSize;
}

void Aetherling_createFlattenGenerator(Context* c) {

    Namespace* aetherlinglib = c->getNamespace(AETHERLING_NAMESPACE);
    /*
     * inputType - the type that you want to flatten
     * outputType - what you want to flatten it into, inputType must be some set of arrays of this
     */
    Params flattenNparams = Params({
            {"inputType", CoreIRType::make(c)},
            {"singleElementOutputType", CoreIRType::make(c)}
        });

    aetherlinglib->newTypeGen(
        "flattenN_type", // name for typegen
        flattenNparams, // generator parameters
        [](Context* c, Values genargs) { //Function to compute type
            Type* inputType = genargs.at("inputType")->get<Type*>();
            Type* singleElementOutputType = genargs.at("singleElementOutputType")->get<Type*>();
            uint outputSize = getFlattenedSize(c, inputType, singleElementOutputType);
            return c->Record({
                    {"in", inputType},
                    {"out", singleElementOutputType->Arr(outputSize)}
                });
        });

    Generator* flattenN =
        aetherlinglib->newGeneratorDecl("flattenN", aetherlinglib->getTypeGen("flattenN_type"), flattenNparams);

    flattenN->setGeneratorDefFromFun([](Context* c, Values genargs, ModuleDef* def) {
            printf("running generator\n");
            ArrayType* inputType = dyn_cast<ArrayType>(genargs.at("inputType")->get<Type*>());
            Type* singleElementOutputType = genargs.at("singleElementOutputType")->get<Type*>();
            ArrayType* outputType = dyn_cast<ArrayType>(def->sel("self.out")->getType());
            // recursion:
            // base case: the input type is same as the full output type (array, not single element)
            // in this case, just iterate through all the inputs and flatten them
            if (c->In(outputType) == inputType) {
                for (uint i = 0; i < inputType->getLen(); i++) {
                    string iStr = to_string(i);
                    def->connect("self.in." + iStr, "self.out." + iStr);
                }
            }
            // recursion case:
            // for each element in top level array of the inputType, create a flattener and
            // then wire each of its outputs to this flattener's output
            else {
                uint inputTypeLen = inputType->getLen();
                uint flattenInnerOutputLen = getFlattenedSize(c, inputType->getElemType(), singleElementOutputType);
                for (uint i = 0; i < inputTypeLen; i++) {
                    string iStr = to_string(i);
                    cout << "flattenNInner_" + iStr << " has input type " << inputType->getElemType()->toString() << endl;
                    Instance* fl = def->addInstance("flattenNInner_" + iStr, "aetherlinglib.flattenN", {
                            {"inputType", Const::make(c, inputType->getElemType())},
                            {"singleElementOutputType", Const::make(c, singleElementOutputType)}
                        });
                    cout << "single el output: " << singleElementOutputType->toString() << endl;
                    cout << "it has to_string " << fl->getModuleRef()->toString() << endl;
                    def->connect("self.in." + iStr, "flattenNInner_" + iStr + ".in");
                    // wire up each output from the inner to the output
                    for (uint j = 0; j < flattenInnerOutputLen; j++) {
                        string jStr = to_string(j);
                        string outIdxStr = to_string(i*flattenInnerOutputLen + j);
                        def->connect("flattenNInner_" + iStr + ".out." + jStr, "self.out." + outIdxStr);
                    }
                }
            }
        });
}
