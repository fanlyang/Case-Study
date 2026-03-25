#pragma once
#include <xacc.hpp>
#include <functional>
#include <memory>
#include <string>

namespace hhl {

struct HHLSpec {
  int nSys = 0;
  int nEig = 0;
};

using ControlledUPowerFn =
  std::function<void(std::shared_ptr<xacc::CompositeInstruction>, int k)>;

using EigenvalueRotationFn =
  std::function<void(std::shared_ptr<xacc::CompositeInstruction>)>;

std::shared_ptr<xacc::CompositeInstruction>
createHHL(const std::string& name,
          const HHLSpec& spec,
          const std::shared_ptr<xacc::CompositeInstruction>& bPrep,
          const ControlledUPowerFn& applyControlledUPower,
          const EigenvalueRotationFn& applyEigRotation);

} // namespace hhl