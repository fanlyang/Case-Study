#include "xacc.hpp"
#include "xacc_service.hpp"
#include "xacc_quantum_gate_api.hpp"

#include <cmath>
#include <functional>
#include <iostream>
#include <memory>
#include <vector>

using CompositePtr = std::shared_ptr<xacc::CompositeInstruction>;

// Controlled-U^{power} generator:
// power: 2^k
// control_qubit: index of eigenvalue qubit
// system_qubits: indices of system register (here 1 qubit for 2x2)
using ControlledUPowerFn =
    std::function<CompositePtr(int power,
                               std::size_t control_qubit,
                               const std::vector<std::size_t>& system_qubits)>;

// -----------------------------------------------------------------------------
// QFT / IQFT on n qubits starting at offset
// -----------------------------------------------------------------------------

CompositePtr create_qft(int n, std::size_t offset = 0) {
    auto provider = xacc::getIRProvider("quantum");
    auto qft = provider->createComposite("qft_" + std::to_string(n));

    for (int k = 0; k < n; ++k) {
        qft->addInstruction(
            provider->createInstruction("H", {offset + (std::size_t)k}));
        for (int j = k + 1; j < n; ++j) {
            double theta = M_PI / std::pow(2.0, j - k);
            qft->addInstruction(
                provider->createInstruction(
                    "CPhase",
                    {offset + (std::size_t)j, offset + (std::size_t)k},
                    {theta}));
        }
    }
    for (int i = 0; i < n / 2; ++i) {
        qft->addInstruction(
            provider->createInstruction(
                "Swap",
                {offset + (std::size_t)i,
                 offset + (std::size_t)(n - 1 - i)}));
    }
    return qft;
}

CompositePtr create_iqft(int n, std::size_t offset = 0) {
    auto provider = xacc::getIRProvider("quantum");
    auto iqft = provider->createComposite("iqft_" + std::to_string(n));

    for (int i = 0; i < n / 2; ++i) {
        iqft->addInstruction(
            provider->createInstruction(
                "Swap",
                {offset + (std::size_t)i,
                 offset + (std::size_t)(n - 1 - i)}));
    }

    for (int k = n - 1; k >= 0; --k) {
        for (int j = k - 1; j >= 0; --j) {
            double theta = -M_PI / std::pow(2.0, k - j);
            iqft->addInstruction(
                provider->createInstruction(
                    "CPhase",
                    {offset + (std::size_t)k, offset + (std::size_t)j},
                    {theta}));
        }
        iqft->addInstruction(
            provider->createInstruction("H", {offset + (std::size_t)k}));
    }
    return iqft;
}

// -----------------------------------------------------------------------------
// Generic QPE block
// -----------------------------------------------------------------------------

CompositePtr create_qpe(const std::string &name,
                        int n_sys,
                        int n_eig,
                        const ControlledUPowerFn &controlledU) {
    auto provider = xacc::getIRProvider("quantum");
    auto qpe = provider->createComposite(name);

    auto sys = [&](int i) { return (std::size_t)i; };
    auto eig = [&](int i) { return (std::size_t)(n_sys + i); };

    // Put eigenvalue register in |+>^n
    for (int i = 0; i < n_eig; ++i) {
        qpe->addInstruction(
            provider->createInstruction("H", {eig(i)}));
    }

    // Apply controlled-U^{2^k}
    std::vector<std::size_t> system_qubits;
    for (int i = 0; i < n_sys; ++i) system_qubits.push_back(sys(i));

    for (int k = 0; k < n_eig; ++k) {
        int power = 1 << k;
        auto U_k = controlledU(power, eig(k), system_qubits);
        qpe->addInstruction(U_k);
    }

    // IQFT on eigenvalue register
    auto iqft = create_iqft(n_eig, (std::size_t)n_sys);
    for (auto &inst : iqft->getInstructions()) {
        qpe->addInstruction(inst->clone());
    }

    return qpe;
}

// -----------------------------------------------------------------------------
// Inverse-eigenvalue rotation on ancilla, controlled by eigenvalue qubit
// -----------------------------------------------------------------------------

CompositePtr create_inverse_eigenvalue_rotation(std::size_t eigen_qubit,
                                                std::size_t ancilla_qubit,
                                                double lambda,
                                                double C = 0.5) {
    auto provider = xacc::getIRProvider("quantum");
    auto rot = provider->createComposite("inv_eig_rot");

    // HHL rotation: theta = 2 * arcsin(C / lambda)
    double theta = 2.0 * std::asin(C / lambda);

    rot->addInstruction(
        provider->createInstruction("RY", {ancilla_qubit}, {theta / 2.0}));

    rot->addInstruction(
        provider->createInstruction("CNOT",
                                    {eigen_qubit, ancilla_qubit}));

    rot->addInstruction(
        provider->createInstruction("RY", {ancilla_qubit}, {-theta / 2.0}));

    rot->addInstruction(
        provider->createInstruction("CNOT",
                                    {eigen_qubit, ancilla_qubit}));

    return rot;
}

// -----------------------------------------------------------------------------
// Example controlled-U for a 2x2 Hermitian with eigenvalues lambda0, lambda1
// (1 system qubit). This is where you’d generalize for larger matrices.
// -----------------------------------------------------------------------------

ControlledUPowerFn make_controlled_U_for_2x2(double lambda0,
                                             double lambda1,
                                             double t) {
    return [=](int power,
               std::size_t control_qubit,
               const std::vector<std::size_t>& system_qubits) -> CompositePtr {
        auto provider = xacc::getIRProvider("quantum");
        auto U = provider->createComposite("CU_power");

        // Toy model: effective phase proportional to (lambda1 - lambda0)
        double eff_lambda = lambda1 - lambda0;
        double theta = t * eff_lambda * power;

        U->addInstruction(
            provider->createInstruction(
                "CPhase",
                {control_qubit, system_qubits[0]},
                {theta}));

        return U;
    };
}

// -----------------------------------------------------------------------------
// General HHL skeleton (here instantiated for 2x2: n_sys = 1, n_eig = 1)
// -----------------------------------------------------------------------------

CompositePtr create_hhl_2x2(const std::vector<std::vector<double>>& A,
                            const std::vector<double>& eigenvalues,
                            const ControlledUPowerFn& controlledU,
                            double t = M_PI / 2.0) {
    auto provider = xacc::getIRProvider("quantum");

    int dim = (int)A.size();          // 2 for 2x2
    int n_sys = (int)std::log2(dim);  // 1 system qubit
    int n_eig = 1;                    // 1 eigenvalue qubit (toy precision)
    int anc = 1;                      // 1 ancilla
    int total_qubits = n_sys + n_eig + anc;
    (void)total_qubits;

    auto sys = [&](int i) { return (std::size_t)i; };
    auto eig = [&](int i) { return (std::size_t)(n_sys + i); };
    auto ancilla = [&]() { return (std::size_t)(n_sys + n_eig); };

    auto hhl = provider->createComposite("hhl_2x2_general");

    // 1. Prepare |b> on system qubit (example: |1>)
    hhl->addInstruction(provider->createInstruction("X", {sys(0)}));

    // 2. QPE
    auto qpe = create_qpe("qpe_block", n_sys, n_eig, controlledU);
    hhl->addInstruction(qpe);

    // 3. Inverse-eigenvalue rotation (use eigenvalues[1] as non-zero example)
    double lambda = eigenvalues[1];
    auto inv_rot = create_inverse_eigenvalue_rotation(eig(0), ancilla(),
                                                      lambda, 0.5);
    hhl->addInstruction(inv_rot);

    // 4. (Optional) uncompute QPE; omitted here for brevity

    // 5. Measure ancilla and system
    hhl->addInstruction(
        provider->createInstruction("Measure", {ancilla()}));
    hhl->addInstruction(
        provider->createInstruction("Measure", {sys(0)}));

    return hhl;
}

// -----------------------------------------------------------------------------
// Main: example usage for a 2x2 Hermitian
// -----------------------------------------------------------------------------

int main(int argc, char **argv) {
    xacc::Initialize(argc, argv);

    // Example 2x2 Hermitian A with eigenvalues lambda0, lambda1
    // (You’d compute these classically for now.)
    std::vector<std::vector<double>> A = {
        {1.0, 0.0},
        {0.0, 2.0}
    };
    std::vector<double> eigenvalues = {1.0, 2.0};
    double lambda0 = eigenvalues[0];
    double lambda1 = eigenvalues[1];
    double t = M_PI / 2.0;

    auto controlledU = make_controlled_U_for_2x2(lambda0, lambda1, t);

    auto acc = xacc::getAccelerator("aer");
    auto hhl = create_hhl_2x2(A, eigenvalues, controlledU, t);

    std::cout << "HHL circuit (modular 2x2 example):\n"
              << hhl->toString() << "\n";

    auto buffer = xacc::qalloc(hhl->nPhysicalBits());
    acc->execute(buffer, hhl);
    buffer->print();

    xacc::Finalize();
    return 0;
}
