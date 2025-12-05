# Case-Study

Although quantum computing offers certain advantages over classical computing in specific scenarios, it is not yet well-suited for general-purpose computation. Therefore, a practical approach to leveraging quantum computing (QC) is to treat quantum processing units (QPUs) as specialized accelerators, utilized for particular computational tasks. In this project, I will discuss a hybrid high-performance computing (HPC) approach that integrates both classical and quantum resources.[3]

When considering the hardware interface between the HPC host system and the quantum accelerators, many strategies are possible, including remote access, on-premises (or co-located) integration, and on-node integration. Each of these brings distinct advantages and disadvantages and will suit different needs. In this project, I will choose the remote access strategy, which means IBM Quantum Computing (IBM Quantum Backend) will be used to simulate this process.

In our exercise sessions, we have been using Python and Qiskit to explore quantum circuits. For this project, I plan to:

1. Replace Qiskit with XACC, which is particularly well-suited for parallel computing and provides a more flexible framework for integrating HPC and QC. 

2. Implement and compare the HHL algorithm and the VQLS algorithm to solve a specific linear equation. The HHL algorithm is one we have studied in class, and I will also implement the VQLS algorithm as described in reference [1].

This project aims to explore the full integration of quantum computing with classical HPC (High-Performance Computing). The scope includes leveraging the XACC framework to orchestrate heterogeneous workflows, integrating quantum hardware/simulators into classical computation pipelines, and conducting low-level benchmarking and compilation to optimize execution. The expected results will offer new perspectives for guiding future development strategies for quantum-classical integration.
For background information, an introduction and overview of the development of quantum algorithms can be found in reference [2], while a detailed description of the VQLS algorithm is provided in reference [1].Additionally, for the parallel computing aspects, the XACC framework is discussed in reference [4], and a comprehensive description of hybrid HPCQC approaches can be found in reference [3][5][6].




# Reference :
[1] CAPPANERA, Enrico. [*Variational quantum linear solver for finite element problems: A Poisson equation test case*](https://scholar.google.com/scholar_lookup?title=Variational%20quantum%20linear%20solver%20for%20finite%20element%20problems%3A%20A%20Poisson%20equation%20test%20case&author=E.%20Cappanera&publication_year=2021). Delft University of Technology, 2021.

[2] RAO, Xiang. Performance study of variational quantum linear solver with an improved ansatz for reservoir flow equations. Physics of Fluids, 2024, 36. Jg., Nr. 4.

[3] RUEFENACHT, Martin, et al. Bringing quantum acceleration to supercomputers. IQM/LRZ Technical Report, https://www. quantu m. lrz. de/fileadmin/QIC/Downloads/IQM HPC-QC-Integration-White paper. pdf, 2022.

[4] MCCASKEY, Alexander J., et al. XACC: a system-level software infrastructure for heterogeneous quantum–classical computing. Quantum Science and Technology, 2020, 5. Jg., Nr. 2, S. 024002.

[5] Elsharkawy, A., Guo, X. and Schulz, M., 2024, September. Integration of quantum accelerators into hpc: Toward a unified quantum platform. In 2024 IEEE International Conference on Quantum Computing and Engineering (QCE) (Vol. 1, pp. 774-783). IEEE.

[6] Elsharkawy, A., To, X.T.M., Seitz, P., Chen, Y., Stade, Y., Geiger, M., Huang, Q., Guo, X., Ansari, M.A., Mendl, C.B. and Kranzlmüller, D., 2025. Integration of quantum accelerators with high performance computing—a review of quantum programming tools. ACM Transactions on Quantum Computing, 6(3), pp.1-46.

