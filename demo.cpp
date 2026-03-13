#include "xacc.hpp"
#include "xacc_service.hpp"
#include "xacc_quantum_gate_api.hpp"
#include "Utils.hpp"
#include <cmath>
#include <iostream>
#include <functional>

std::shared_ptr<xacc::CompositeInstruction> create_qft(int n) {
    auto provider = xacc::getIRProvider("quantum");
    auto qft = provider->createComposite("qft_" + std::to_string(n));

    for (int k = 0; k < n; ++k) {
        qft->addInstruction(
            provider->createInstruction("H", {static_cast<std::size_t>(k)}));
        for (int j = k + 1; j < n; ++j) {
            double theta = M_PI / std::pow(2.0, j - k);
            qft->addInstruction(
                provider->createInstruction("CPhase",
                    {static_cast<std::size_t>(j), static_cast<std::size_t>(k)},
                    {theta}));
        }
    }
    for (int i = 0; i < n / 2; ++i) {
        qft->addInstruction(
            provider->createInstruction("Swap",
                {static_cast<std::size_t>(i),
                 static_cast<std::size_t>(n - 1 - i)}));
    }
    return qft;
}

std::shared_ptr<xacc::CompositeInstruction> create_iqft(int n) {
    auto provider = xacc::getIRProvider("quantum");
    auto iqft = provider->createComposite("iqft_" + std::to_string(n));

    for (int i = 0; i < n / 2; ++i) {
        iqft->addInstruction(
            provider->createInstruction("Swap",
                {static_cast<std::size_t>(i),
                 static_cast<std::size_t>(n - 1 - i)}));
    }
    for (int k = n - 1; k >= 0; --k) {
        for (int j = k - 1; j >= 0; --j) {
            double theta = -M_PI / std::pow(2.0, k - j);
            iqft->addInstruction(
                provider->createInstruction("CPhase",
                    {static_cast<std::size_t>(k),
                     static_cast<std::size_t>(j)},
                    {theta}));
        }
        iqft->addInstruction(
            provider->createInstruction("H",
                {static_cast<std::size_t>(k)}));
    }
    return iqft;
}

std::shared_ptr<xacc::CompositeInstruction> create_qpe(
    const std::string &name,
    int n_sys,
    int n_eig,
    const std::function<void(std::shared_ptr<xacc::CompositeInstruction>, int eig_index)> &apply_controlled_U_power) {

    auto provider = xacc::getIRProvider("quantum");
    auto qpe = provider->createComposite(name);

    auto sys = [&](int i) { return static_cast<std::size_t>(i); };
    auto eig = [&](int i) { return static_cast<std::size_t>(n_sys + i); };

    for (int i = 0; i < n_eig; ++i) {
        qpe->addInstruction(
            provider->createInstruction("H", {eig(i)}));
    }

    for (int k = 0; k < n_eig; ++k) {
        apply_controlled_U_power(qpe, k);
    }

    auto iqft = create_iqft(n_eig);
    for (auto &inst : iqft->getInstructions()) {
        auto cloned = inst->clone();

        std::vector<std::size_t> newBits;
        for (auto b : inst->bits()) {
            newBits.push_back(static_cast<std::size_t>(b + n_sys));
        }

        cloned->setBits(newBits);
        qpe->addInstruction(cloned);
    }

    return qpe;
}

std::shared_ptr<xacc::CompositeInstruction> create_hhl_2x2() {
    auto provider = xacc::getIRProvider("quantum");

    const int n_sys = 1;
    const int n_eig = 2;
    const int anc = 1;
    const int total_qubits = n_sys + n_eig + anc;
    (void)total_qubits;

    auto sys = [&](int i) { return static_cast<std::size_t>(i); };
    auto eig = [&](int i) { return static_cast<std::size_t>(n_sys + i); };
    auto ancilla = [&]() { return static_cast<std::size_t>(n_sys + n_eig); };

    auto hhl = provider->createComposite("hhl_2x2");

    hhl->addInstruction(provider->createInstruction("X", {sys(0)}));

    double t = M_PI / 2.0;

    auto apply_controlled_U_power =
        [&](std::shared_ptr<xacc::CompositeInstruction> circ, int eig_index) {
            auto prov = xacc::getIRProvider("quantum");
            std::size_t ctrl = eig(eig_index);

            double theta = t * std::pow(2.0, eig_index);

            circ->addInstruction(
                prov->createInstruction("CPhase",
                    {ctrl, sys(0)}, {theta}));
        };

    auto qpe = create_qpe("qpe_block", n_sys, n_eig, apply_controlled_U_power);
    hhl->addInstruction(qpe);

    double theta_rot = M_PI / 4.0;
    // Decompose CRY(eig(0) → ancilla, theta_rot)
    hhl->addInstruction(provider->createInstruction(
        "Ry", {ancilla()}, {theta_rot / 2}));

    hhl->addInstruction(provider->createInstruction(
        "CNOT", {eig(0), ancilla()}));

    hhl->addInstruction(provider->createInstruction(
        "Ry", {ancilla()}, {-theta_rot / 2}));

    hhl->addInstruction(provider->createInstruction(
        "CNOT", {eig(0), ancilla()}));


    hhl->addInstruction(
        provider->createInstruction("Measure", {ancilla()}));
    hhl->addInstruction(
        provider->createInstruction("Measure", {sys(0)}));

    return hhl;
}

int main(int argc, char **argv) {
    xacc::Initialize(argc, argv);



    auto acc = xacc::getAccelerator("qpp");
    auto hhl = create_hhl_2x2();

    // --- Print QFT(2) and IQFT(2) for demonstration ---
    auto qft2 = create_qft(3);
    std::cout << "QFT(2) circuit:\n" << qft2->toString() << "\n\n";
    std::cout <<"--------------------------------------" <<std::endl;
    auto iqft2 = create_iqft(3);
    std::cout << "IQFT(2) circuit:\n" << iqft2->toString() << "\n\n";
    std::cout <<"--------------------------------------" <<std::endl;
    // --- Build HHL and extract QPE block ---
    auto inst = hhl->getInstruction(1);
    auto qpe = std::dynamic_pointer_cast<xacc::CompositeInstruction>(inst);

    if (qpe) {
        std::cout << "QPE circuit:\n" << qpe->toString() << "\n\n";
    } else {
        std::cout << "Instruction 1 is not a composite instruction.\n";
    }
    std::cout <<"--------------------------------------" <<std::endl;
    std::cout << "HHL circuit:\n" << hhl->toString() << "\n";

    auto buffer = xacc::qalloc(hhl->nPhysicalBits());
    acc->execute(buffer, hhl);
    buffer->print();

    xacc::Finalize();
    return 0;
}
