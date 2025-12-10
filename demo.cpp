#include "xacc.hpp"
#include <fstream>
#include <iostream>
#include "xacc_service.hpp"
int main(int argc, char **argv) {
    xacc::Initialize(argc, argv);

    auto accelerator = xacc::getAccelerator("aer", {{"shots", 4096}});
    auto compiler = xacc::getCompiler("staq");

    const std::string QASM_SRC_FILE = "/home/dev/casestudy/src/QFT2.txt";

    // read QASM file
    std::ifstream inFile(QASM_SRC_FILE);
    std::stringstream strStream;
    strStream << inFile.rdbuf();
    const std::string qasmSrcStr = strStream.str();

    // compile QFT circuit
    xacc::ScopeTimer timer("compile", false);
    auto IR = compiler->compile(qasmSrcStr);
    auto program = IR->getComposites()[0];
    std::cout << "Number of instructions: " << program->nInstructions() << "\n";
    std::cout << "Elapsed time: " << timer.getDurationMs() << " ms.\n";
    /*
    auto provider = xacc::getIRProvider("quantum");

    // 创建 QFT 逆电路
    auto qft_dagger = provider->createComposite("qft_dagger_2qubit");
    // Step 1: H q[0]
    auto h0 = provider->createInstruction("H", {0});
    qft_dagger->addInstruction(h0);

    // Step 2: CU1(-pi/2) q[1], q[0] -> 用 CPhase
    double lambda = -M_PI / 2.0;
    auto cu1 = provider->createInstruction("CPhase", {1,0}, {lambda}); // {control,target}
    qft_dagger->addInstruction(cu1);

    // Step 3: H q[1]
    auto h1 = provider->createInstruction("H", {1});
    qft_dagger->addInstruction(h1);

    // 将逆 QFT 添加到原 QFT 电路后面
    program->addInstruction(qft_dagger);

    // Step 4: 测量所有 qubit
    auto m0 = provider->createInstruction("Measure", {0});
    auto m1 = provider->createInstruction("Measure", {1});
    program->addInstructions({m0, m1});
    */
    // print the IR circuit
    std::cout << "Program IR:\n" << program->toString() << std::endl;

    // execute
    auto buffer = xacc::qalloc(2);
    accelerator->execute(buffer, program);

    buffer->print();
    xacc::Finalize();
    return 0;
}
