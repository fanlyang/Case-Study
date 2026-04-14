#include <xacc.hpp>
#include <xacc_service.hpp>
#include <iostream>
#include <cmath>
#include "CircuitUtils.hpp"
#include "HHL.hpp"

int main(int argc, char** argv) {
  xacc::Initialize(argc, argv);

  hhl::HHLSpec spec;
  spec.nSys = 1;
  spec.nEig = 2;

  auto provider = xacc::getIRProvider("quantum");

  auto bPrep = provider->createComposite("bprep");
  bPrep->addInstruction(provider->createInstruction("X", {std::size_t(0)}));

  auto applyControlledUPower = 
    [&](std::shared_ptr<xacc::CompositeInstruction> circ, int k) {
      auto prov = xacc::getIRProvider("quantum");
      const std::size_t ctrl = static_cast<std::size_t>(spec.nSys + k);
      const double theta = (M_PI / 4.0) * std::pow(2.0, k);
      for (int q = 0; q < spec.nSys; ++q) {
        circ->addInstruction(prov->createInstruction("CPhase",
          {ctrl, static_cast<std::size_t>(q)}, {theta}));
      }
    };

  auto applyEigRotation = [&](std::shared_ptr<xacc::CompositeInstruction> circ, 
                               const std::vector<double>& eigenvalues) {
    auto prov = xacc::getIRProvider("quantum");
    const std::size_t anc = static_cast<std::size_t>(hhl::getTotalQubits(spec.nSys, spec.nEig) - 1);
    
    // For each eigenvalue, apply the corresponding rotation
    for (size_t j = 0; j < eigenvalues.size(); ++j) {
      double lambda_j = eigenvalues[j];
      
      // Calculate theta from the formula: theta = 2*arcsin(1/lambda_j)
      double theta = 2.0 * std::asin(1.0 / lambda_j);
      
      std::cout << "Eigenvalue " << j << ": λ̄ = " << lambda_j 
                << ", θ = " << theta << "\n";
      
      // Apply the controlled rotation (controlled by eigenvalue register)
      const std::size_t ctrl = static_cast<std::size_t>(spec.nSys + j);  // Control: eigenvalue qubit j
      circ->addInstruction(prov->createInstruction("Ry", {anc}, {theta / 2.0}));
      circ->addInstruction(prov->createInstruction("CNOT", {ctrl, anc}));
      circ->addInstruction(prov->createInstruction("Ry", {anc}, {-theta / 2.0}));
      circ->addInstruction(prov->createInstruction("CNOT", {ctrl, anc}));
    }
  };

  // Calculate scaled eigenvalues
  std::vector<double> eigenvalues_raw = {2.0/3, 4.0/3};
  const int n = spec.nEig;
  double max_lambda_bar = std::pow(2.0, n) - 1;
  
  double t = ( M_PI * max_lambda_bar) / 4;
  
  std::cout << "n = " << n << ", max λ̄ = " << max_lambda_bar << "\n";
  std::cout << "t = " << t << "\n";
  
  std::vector<double> eigenvalues_scaled;
  for (double lambda : eigenvalues_raw) {
    double lambda_bar = std::pow(2.0, n) * (lambda * t) / (2.0 * M_PI);
    eigenvalues_scaled.push_back(lambda_bar);
    std::cout << "λ = " << lambda << " → λ̄ = " << lambda_bar << "\n";
  }

  // Create HHL circuit
  auto hhlCirc = hhl::createHHL("hhl", spec, bPrep, 
                                 applyControlledUPower, 
                                 [&](auto c) { applyEigRotation(c, eigenvalues_scaled); });

  std::cout << "\nCircuit without measurements:\n" << hhlCirc->toString() << "\n";

  // Add measurements at the end
  int nTotalQubits = hhl::getTotalQubits(spec.nSys, spec.nEig);
  for (int i = 0; i < nTotalQubits; ++i) {
    auto measure = provider->createInstruction("Measure", std::vector<std::size_t>{static_cast<std::size_t>(i)});
    hhlCirc->addInstruction(measure);
  }

  std::cout << "Circuit with measurements:\n" << hhlCirc->toString() << "\n";

  auto accelerator = xacc::getAccelerator("qpp", {{"shots", 10000}});
  auto buffer = xacc::qalloc(nTotalQubits);
  accelerator->execute(buffer, hhlCirc);
  buffer->print();
  xacc::Finalize();
  return 0;
}