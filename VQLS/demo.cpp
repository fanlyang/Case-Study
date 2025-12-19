#include "xacc/xacc.hpp"
#include "quantum/gate/xacc_observable.hpp"

 int main(int argc, char **argv) {
   xacc::Initialize(argc, argv);

   // Get reference to the Accelerator
   // specified by --accelerator argument
   auto accelerator = xacc::getAccelerator("qpp", {std::make_pair("shots", 10000)});

   // Create the N=2 deuteron Hamiltonian
   auto H_N_2 = xacc::quantum::getObservable(
       "pauli", std::string("5.907 - 2.1433 X0X1 "
                         "- 2.1433 Y0Y1"
                         "+ .21829 Z0 - 6.125 Z1"));

   auto optimizer = xacc::getOptimizer("nlopt",
                       {std::make_pair("initial-parameters", std::vector<double>{0.1})});


   // JIT map Quil QASM Ansatz to IR
   xacc::qasm(R"(
  .compiler xasm
  .circuit deuteron_ansatz
  .parameters theta
  .qbit q
  X(q[0]);
  Ry(q[1], theta);
  CNOT(q[1],q[0]);
  )");
  auto ansatz = xacc::getCompiled("deuteron_ansatz");

  // Get the VQE Algorithm and initialize it
  auto vqe = xacc::getAlgorithm("vqe");
  vqe->initialize({std::make_pair("ansatz", ansatz),
                 std::make_pair("observable", H_N_2),
                 std::make_pair("accelerator", accelerator),
                 std::make_pair("optimizer", optimizer)});

  // Allocate some qubits and execute
  auto buffer = xacc::qalloc(2);
  vqe->execute(buffer);

  auto ground_energy = (*buffer)["opt-val"].as<double>();
  auto params = (*buffer)["opt-params"].as<std::vector<double>>();
  std::cout << "Energy: " << (*buffer)["opt-val"].as<double>() << std::endl;

}