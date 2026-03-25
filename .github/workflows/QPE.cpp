#include "hhl/QPE.hpp"
#include "hhl/QFT.hpp"
#include "hhl/CircuitUtils.hpp"
#include <xacc_service.hpp>

namespace hhl {

std::shared_ptr<xacc::CompositeInstruction>
createQPE(const std::string& name,
          int nSys,
          int nEig,
          const ControlledUPowerFn& applyControlledUPower) {

  auto provider = xacc::getIRProvider("quantum");
  auto qpe = provider->createComposite(name);

  // Hadamards on eig register
  for (int i = 0; i < nEig; ++i) {
    qpe->addInstruction(provider->createInstruction("H", 
      {static_cast<std::size_t>(nSys + i)}));
  }

  // Controlled U^(2^k)
  for (int k = 0; k < nEig; ++k) {
    applyControlledUPower(qpe, k);
  }

  // IQFT on eig register
  auto iqft = createIQFT(nEig);
  auto iqft_shifted = remapQubits(iqft, nSys);
  for (auto& inst : iqft_shifted->getInstructions()) {
    qpe->addInstruction(inst);
  }

  return qpe;
}

} // namespace hhl