#ifndef HHL_CIRCUIT_UTILS_HPP
#define HHL_CIRCUIT_UTILS_HPP
#include <xacc.hpp>
#include <memory>
#include <vector>
#include <string>

namespace hhl {

/// Invert a composite: reverse order + negate parameters
// Genuinely useful for QPE† without manually building it again
inline std::shared_ptr<xacc::CompositeInstruction>
inverseComposite(const std::shared_ptr<xacc::CompositeInstruction>& circ,
                 const std::string& suffix = "_dagger") {
  auto provider = xacc::getIRProvider("quantum");
  auto inv = provider->createComposite(circ->name() + suffix);

  auto insts = circ->getInstructions(); // get all instructions in the circuit

  //Loop backwards → reverse order
  for (int i = static_cast<int>(insts.size()) - 1; i >= 0; --i) {
    auto inst = insts[i]->clone(); //Copy the gate (don't modify original)
    const auto nParams = inst->nParameters();
    if (nParams > 0) {
      auto params = inst->getParameters();
      for (size_t j = 0; j < params.size(); ++j) {
        try {
          double v = params[j].as<double>();
          // Use setParameter (singular) with index j
          inst->setParameter(j, xacc::InstructionParameter(-v));
        } catch (...) {}
      }
    }
    inv->addInstruction(inst);
  }
  return inv;
}

// Remap qubits: given a composite, shift all qubit indices by offset
// Standard pattern when appending to larger circuit
inline std::shared_ptr<xacc::CompositeInstruction>
remapQubits(const std::shared_ptr<xacc::CompositeInstruction>& circ, int offset) {
  if (offset == 0) return circ; // no-op

  //provider is a factory object that creates quantum program elements
  auto provider = xacc::getIRProvider("quantum");
  auto remapped = provider->createComposite(circ->name());
  
  for (auto& inst : circ->getInstructions()) {
    auto cloned = inst->clone();
    std::vector<std::size_t> newBits;
    //For each qubit the gate acts on
    for (auto b : inst->bits()) {
      newBits.push_back(static_cast<std::size_t>(b + offset));
    }
    cloned->setBits(newBits);
    remapped->addInstruction(cloned);
  }
  return remapped;
}

inline int getTotalQubits(int nSys, int nEig) {
  return nSys + nEig + 1; // +1 for ancilla
}

} // namespace hhl
#endif