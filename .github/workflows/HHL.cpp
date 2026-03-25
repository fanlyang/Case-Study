#include "hhl/HHL.hpp"
#include "hhl/QPE.hpp"
#include "hhl/CircuitUtils.hpp"
#include <xacc_service.hpp>

namespace hhl {

std::shared_ptr<xacc::CompositeInstruction>
createHHL(const std::string& name,
          const HHLSpec& spec,
          const std::shared_ptr<xacc::CompositeInstruction>& bPrep,
          const ControlledUPowerFn& applyControlledUPower,
          const EigenvalueRotationFn& applyEigRotation) {

  auto provider = xacc::getIRProvider("quantum");
  auto hhl = provider->createComposite(name);

  // 1. Prepare |b>
  if (bPrep) {
    for (auto& inst : bPrep->getInstructions()) {
      hhl->addInstruction(inst);
    }
  }

  // 2. QPE
  auto qpe = createQPE(name + "_qpe", spec.nSys, spec.nEig, applyControlledUPower);
  hhl->addInstruction(qpe);

  // 3. Controlled rotation
  applyEigRotation(hhl);

  // 4. QPE†
  auto qpeDagger = inverseComposite(qpe);
  hhl->addInstruction(qpeDagger);

  return hhl;
}

} // namespace hhl