#include <xacc.hpp>
#include <xacc_service.hpp>
#include <iostream>
#include <cmath>

#include "hhl/HHL.hpp"

int main(int argc, char** argv) {
  xacc::Initialize(argc, argv);

  hhl::HHLSpec spec;
  spec.nSys = 2;
  spec.nEig = 3;

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

  auto applyEigRotation = [&](std::shared_ptr<xacc::CompositeInstruction> circ) {
    auto prov = xacc::getIRProvider("quantum");
    const std::size_t anc = static_cast<std::size_t>(hhl::getTotalQubits(spec.nSys, spec.nEig) - 1);
    const double theta = M_PI / 6.0;
    circ->addInstruction(prov->createInstruction("Ry", {anc}, {theta / 2.0}));
    circ->addInstruction(prov->createInstruction("CNOT", {static_cast<std::size_t>(spec.nSys), anc}));
    circ->addInstruction(prov->createInstruction("Ry", {anc}, {-theta / 2.0}));
    circ->addInstruction(prov->createInstruction("CNOT", {static_cast<std::size_t>(spec.nSys), anc}));
  };

  auto hhlCirc = hhl::createHHL("hhl", spec, bPrep, applyControlledUPower, applyEigRotation);

  std::cout << hhlCirc->toString() << "\n";

  auto acc = xacc::getAccelerator("qpp");
  auto buffer = xacc::qalloc(hhl::getTotalQubits(spec.nSys, spec.nEig));
  acc->execute(buffer, hhlCirc);
  buffer->print();

  xacc::Finalize();
  return 0;
}