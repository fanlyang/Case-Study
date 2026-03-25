#pragma once
#include <xacc.hpp>
#include <functional>
#include <memory>
#include <string>

namespace hhl {

using ControlledUPowerFn =
  std::function<void(std::shared_ptr<xacc::CompositeInstruction>, int k)>;

std::shared_ptr<xacc::CompositeInstruction>
createQPE(const std::string& name,
          int nSys,
          int nEig,
          const ControlledUPowerFn& applyControlledUPower);

} // namespace hhl