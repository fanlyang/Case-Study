// hhl_demo.cpp
#include "xacc.hpp"
#include <cmath>
#include <fstream>
#include <iostream>

// Helper: convert double to xacc::InstructionParameter vector if needed
std::vector<xacc::InstructionParameter> pvec(double v) {
    return std::vector<xacc::InstructionParameter>{v};
}

int main(int argc, char** argv) {
    xacc::Initialize(argc, argv);

    // ---- choose backend ----
    // Use tnqvm or aer depending on your installation:
    auto accelerator = xacc::getAccelerator("tnqvm"); // or "aer"
    auto provider = xacc::getIRProvider("quantum");

    // ---- registers: m=2 phase qubits, 1 system qubit, 1 ancilla qubit ----
    // We'll number them: phase qubits: p1 (msb) -> index 3, p0 -> index 2
    // system qubit s -> index 1
    // ancilla a -> index 0
    // (You can choose any indexing; here we put ancilla at 0 for convenience.)
    //
    // total qubits = 4
    int total_qubits = 4;

    // Create a top-level composite (program)
    auto program = provider->createComposite("hhl_demo");

    // ========== 1) prepare phase register in |0>^m and ancilla |0>, system in |b> ==========
    // prepare |b> on system qubit (index 1)
    // For demonstration assume |b> = cos(theta_b)|0> + sin(theta_b)|1>
    double theta_b = M_PI/4.0; // choose b-state amplitude; change as desired
    // prepare system qubit state by Ry on qubit 1
    auto prep_b = provider->createInstruction("Ry", {1}, std::vector<xacc::InstructionParameter>{2*theta_b});
    program->addInstruction(prep_b);

    // ancilla left in |0> (index 0)
    // phase qubits p1 (index 3) and p0 (index 2) initially |0>

    // ========== 2) QPE: create superposition on phase register ==========
    // apply H to p1 and p0
    program->addInstruction(provider->createInstruction("H", {3}));
    program->addInstruction(provider->createInstruction("H", {2}));

    // ========== 3) define U such that U |u_j> = e^{2π i φ_j} |u_j>
    // For simplicity we assume the system computational basis {|0>,|1>} is the eigenbasis:
    // choose two eigenvalues lambda0, lambda1 > 0 and map to phases phi_j = lambda_j / L
    double lambda0 = 1.0;
    double lambda1 = 2.0;
    double L = 4.0;            // scaling factor so phi in [0,1)
    double phi0 = lambda0 / L; // e.g., 0.25
    double phi1 = lambda1 / L; // e.g., 0.5

    // we implement U = diag(e^{2π i phi0}, e^{2π i phi1})
    // but we can shift global phase to have U = diag(1, e^{2π i (phi1-phi0)}) for simpler controlled phase:
    double phi_rel = phi1 - phi0; // relative phase
    // We'll implement controlled-U^2^k by controlled phase on system qubit's |1> with angle 2π * phi_rel * 2^k
    //
    // (If you want exact mapping including phi0, you can add an overall single-qubit phase on system.)

    // Apply overall single-qubit phase on system to absorb phi0: rotate system by global phase exp(2π i phi0)
    // (global phases don't matter physically, but to keep phase register values intuitive, we'll ignore phi0)
    // So treat basis |0> eigenphase = 0, |1> eigenphase = phi = phi_rel

    double phi = phi_rel; // the phase difference we encode: phi = (lambda1-lambda0)/L

    // ========== 4) QPE controlled-U^{2^k} operations ==========
    // For k = 0...m-1 (m=2). We use control = phase qubit, target = system qubit.
    // Controlled-U^{2^k} is controlled phase on system-target: CPhase(angle = 2π * phi * 2^k)
    // Note: many IR providers accept "CPhase" with args {control, target, angle}
    for (int k = 0; k < 2; ++k) {
        int phase_qubit = 3 - k; // order: k=0 -> msb? choose convention: here p1 (index3) is MSB, p0 (index2) is LSB
        double angle = 2.0 * M_PI * phi * std::pow(2.0, k);
        // Use CPhase(control, target, angle)
        program->addInstruction(provider->createInstruction("CPhase", {phase_qubit, 1},
                                                            std::vector<xacc::InstructionParameter>{angle}));
    }

    // ========== 5) inverse QFT on phase register (2 qubits) ==========
    // implement inverse QFT on {p1 (3), p0 (2)}:
    // QFT^-1:  swap not needed if you interpret output bits accordingly.
    // For 2-qubit inverse QFT:
    // - apply H on p0 (LSB)
    // - apply CPhase(-pi/2) with control p0 target p1 (note: order depends on your QFT convention)
    // - apply H on p1 (MSB)
    //
    // Here we implement the standard inverse sequence consistent with earlier QFT ordering:
    program->addInstruction(provider->createInstruction("H", {2}));
    program->addInstruction(provider->createInstruction("CPhase", {2,3}, std::vector<xacc::InstructionParameter>{-M_PI/2.0}));
    program->addInstruction(provider->createInstruction("H", {3}));

    // Now the phase register should contain approximate binary of phi (for system |1>)
    // if system was |0>, phase register will be |00> (since phi=0 for that eigenstate)

    // ========== 6) Conditional rotation on ancilla using phase register bits ==========
    // We want to rotate ancilla (qubit 0) by theta_j such that sin(theta_j) = C / lambda_j.
    // We have lambda_j = lambda0 or lambda1; in our mapping lambda = lambda0 + (binary->value)*L? but for our simple
    // mapping, the phase register value tells us if system was eigenstate 0 or 1.
    //
    // For demonstration we'll treat:
    // - phase register = |00> -> corresponds to eigenvalue lambda0
    // - phase register = |10> (binary 2) -> corresponds to eigenvalue lambda1
    //
    // Precompute required rotation angles:
    double C = 0.4; // scaling constant chosen so C/lambda_j <= 1
    double theta_lambda0 = std::asin(std::min(1.0, C / lambda0)); // ancilla rotation when lambda0
    double theta_lambda1 = std::asin(std::min(1.0, C / lambda1)); // ancilla rotation when lambda1

    // We need to perform: if phase==00 -> Ry(theta_lambda0) on ancilla;
    //                     if phase==10 -> Ry(theta_lambda1) on ancilla;
    //
    // Two implementations:
    // A) If backend supports multi-control-rotation (e.g., CCRy or multi-control createInstruction):
    //    you would do createInstruction("CRy", {p1,p0, ancilla}, {theta}) or similar.
    // B) Otherwise decompose: use Toffoli/ancilla flag:
    //    - allocate an extra classical ancilla bit? (we only have 4 qubits total; we can reuse system qubit as helper or
    //      use temporary CNOT/Toffoli decompositions).
    //
    // Here I provide a direct (conceptual) way using two controlled rotations for simplicity:
    //
    // We will implement an angle-by-bits linear combination:
    //   theta( b1, b0 ) = a0 * b0 + a1 * b1 + a00 * (b1 & b0) + const
    // For our two used patterns we can choose constants so that:
    //   - for 00 -> theta_lambda0
    //   - for 10 -> theta_lambda1
    //
    // Simplify: choose to implement explicit checks using Toffoli-like control:
    //
    // (1) Implement control on (p1==1 && p0==0) -> rotate ancilla by theta_lambda1
    // (2) Implement control on (p1==0 && p0==0) -> rotate ancilla by theta_lambda0
    //
    // To implement (p1==1 && p0==0) we can: apply X on p0 (so p0==0 becomes p0==1), then apply CCX (Toffoli) to a helper qubit,
    // but we didn't reserve a helper. Instead we can do a multi-control rotation directly if supported.
    //
    // For clarity: I'll try to add multi-controlled rotation instructions; if your backend rejects them,
    // you'll need to replace them with decompositions (Toffoli + single-control rotations), which I will explain in comments.

    // Try direct multi-controlled rotations (may need to be adapted to your XACC install):
    try {
        // control pattern for lambda1: p1=1,p0=0 -> we implement control on p1 and control-on-zero on p0 by flipping p0 temporarily
        // flip p0 so that we control on p0==1 (we will flip back)
        program->addInstruction(provider->createInstruction("X", {2})); // flip p0

        // Now control on {p1 (3), p0 (2)} meaning both ones -> corresponds to original p1==1 && p0==0
        // attempt to call a multi-controlled Ry; many backends support "CRy" with two control bits; if not, you'll need decomposition.
        // Here I show a generic call - adapt if your provider expects different name/signature.
        program->addInstruction(provider->createInstruction("MCry", {3,2,0}, std::vector<xacc::InstructionParameter>{theta_lambda1}));
        // flip p0 back
        program->addInstruction(provider->createInstruction("X", {2}));
    } catch (...) {
        // If MCry not supported, fall back to comment in code below on decomposition
        std::cerr << "[WARN] Multi-control Ry (MCry) not supported in this XACC backend; you must decompose it.\n";
    }

    // For lambda0: pattern p1=0,p0=0 -> both zeros. Implement by flipping p1 and p0, then do MCry, then flip back.
    try {
        program->addInstruction(provider->createInstruction("X", {3}));
        program->addInstruction(provider->createInstruction("X", {2}));
        program->addInstruction(provider->createInstruction("MCry", {3,2,0}, std::vector<xacc::InstructionParameter>{theta_lambda0}));
        program->addInstruction(provider->createInstruction("X", {2}));
        program->addInstruction(provider->createInstruction("X", {3}));
    } catch (...) {
        std::cerr << "[WARN] Multi-control Ry (MCry) for lambda0 not supported; decompose manually.\n";
    }

    // NOTE: if your backend doesn't provide MCry, you must implement MCry via
    // (a) an ancilla flag qubit (set by a Toffoli/CCX from the two control bits), then single-control Ry on ancilla,
    // (b) uncompute the ancilla flag.
    //
    // That decomposition would look like:
    //    X (to flip bits where needed)
    //    CCNOT(control1, control2, helper)   // helper is some spare qubit you reserved
    //    CRy(helper, ancilla, theta)         // rotate ancilla controlled on helper
    //    CCNOT(control1, control2, helper)   // uncompute helper
    //    X (flip bits back)
    //
    // Because we chose total_qubits=4, we don't have an extra helper qubit reserved in this template.
    // If you need to decompose MCry you should increase total_qubits by 1 to reserve a helper qubit.

    // ========== 7) Uncompute QPE: run QPE inverse (we already ran inverse QFT earlier),
    // but previously we applied inverse QFT after controlled-U; that is correct ordering.
    // If we used a different QPE ordering ensure to uncompute accordingly.
    //
    // In our code above we already executed the inverse QFT; if you used a different QPE routine, do uncompute here.

    // ========== 8) Optionally measure ancilla or post-select ancilla=1 etc. Here we just print buffer results.
    // Allocate and execute the program (already assembled)
    std::cout << "Program IR:\n" << program->toString() << std::endl;

    // Execute
    auto buffer = xacc::qalloc(total_qubits);
    accelerator->execute(buffer, program);
    buffer->print();

    xacc::Finalize();
    return 0;
}
