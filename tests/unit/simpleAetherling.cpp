#include "coreir.h"
#include "coreir/libs/aetherlinglib.h"
#include <execinfo.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>



using namespace std;
using namespace CoreIR;

void handler(int sig) {
  void *array[10];
  size_t size;

  // get void*'s for all entries on the stack
  size = backtrace(array, 10);

  // print out all the frames to stderr
  fprintf(stderr, "Error: signal %d:\n", sig);
  backtrace_symbols_fd(array, size, STDERR_FILENO);
  exit(1);
}

int main() {
    signal(SIGSEGV, handler);   // install our handler
    Context* c = newContext();
    CoreIRLoadLibrary_aetherlinglib(c);

    uint parallelInputs = 4;
    uint width = 16;
    
    //Type of module 
    Type* oneInManyOutGenType = c->Record({
            {"in",c->BitIn()->Arr(width)->Arr(parallelInputs)},
            {"outMap",c->Bit()->Arr(width)->Arr(parallelInputs)},
            {"outReduce",c->Bit()->Arr(width)}
        });
    Module* testModule = c->getGlobal()->newModuleDecl("testModule",oneInManyOutGenType);
    ModuleDef* testDef = testModule->newModuleDef();

    //Type of module
    RecordType* zippedType = c->Record({
                        {"el0", c->BitIn()->Arr(width)},
                        {"el1", c->BitIn()->Arr(width)}
        });
    Type* twoInZippedOneOutGenType = c->Record({
            {"in",zippedType},
            {"out",c->Bit()->Arr(width)}
        });

    /* creating the mulBy2 that the mapN will parallelize */
    Module* mul2Inputs = c->getGlobal()->newModuleDecl("mul2Inputs", twoInZippedOneOutGenType);
    ModuleDef* mul2InputsDef = mul2Inputs->newModuleDef();
        

    string constModule = Aetherling_addCoreIRConstantModule(c, testDef, width, Const::make(c, width, 4));
    mul2InputsDef->addInstance("mul", "coreir.mul", {{"width", Const::make(c, width)}});
    mul2InputsDef->connect("self.in.el0", "mul.in0");
    mul2InputsDef->connect("self.in.el1", "mul.in1");
    mul2InputsDef->connect("mul.out", "self.out");
    mul2Inputs->setDef(mul2InputsDef);

    Values zip2Params({
            {"numInputs", Const::make(c, parallelInputs)},
            {"input0Type", Const::make(c, c->BitIn()->Arr(width))},
            {"input1Type", Const::make(c, c->BitIn()->Arr(width))}
        });
    
    Values mapNParams({
            {"parallelOperators", Const::make(c, parallelInputs)},
            {"inputType", Const::make(c, zippedType)},
            {"outputType", Const::make(c, c->Bit()->Arr(width))},
            {"operator", Const::make(c, mul2Inputs)}
        });
                      
    // ignoring last argumen to addIstance as no module parameters
    testDef->addInstance("zip2", "aetherlinglib.zip2", zip2Params);
    testDef->addInstance("mapMul", "aetherlinglib.mapN", mapNParams);

    Module* add = c->getGenerator("coreir.add")->getModule({{"width", Const::make(c, width)}});

    Values reduceNParams({
            {"numLayers", Const::make(c, int(log2(parallelInputs)))},
            {"operator", Const::make(c, add)}
        });

    testDef->addInstance("reduceAdd", "aetherlinglib.reduceN", reduceNParams);
    
    testDef->connect("self.in","zip2.in0");
    for (uint i = 0; i <parallelInputs; i++) {
        testDef->connect(constModule + ".out", "zip2.in1." + to_string(i));
    }
    testDef->connect("zip2.out", "mapMul.in");
    testDef->connect("mapMul.out","self.outMap");
    testDef->connect("self.in", "reduceAdd.in");
    testDef->connect("reduceAdd.out", "self.outReduce");

    testModule->setDef(testDef);
    testModule->print();
    c->runPasses({"rungenerators", "verifyconnectivity"});
  
    testModule->print();
    cout << testModule->toString() << endl;
  
    deleteContext(c);
    return 0;
}
