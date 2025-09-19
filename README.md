# Privacy Preserving Quantum Secure Federated Learning Framework based on Multikey CKKS Homomorphic Encryption



## Overview 

Large-scale systems and distributed environments produce vast amounts of data from various sources such as devices, applications, and monitoring components. Directly sharing this raw information across different locations raises significant privacy, security, and regulatory challenges, making conventional centralized data processing or machine learning approaches less suitable. To address these challenges, this project integrates Federated Learning (FL), Homomorphic Encryption (HE), and High-Performance Computing (HPC) into a unified framework.


This project implements a **Privacy Preserving Quantum Secure Federated Learning Framework based on Multikey CKKS Homomorphic Encryption** that leverages:  

- **Federated Learning (FL):** Collaborative training across distributed clients without sharing raw data.  
- **Homomorphic Encryption (HE) – Multikey CKKS:** Encrypts local model updates and enables secure aggregation without decryption.  
- **Proxy Re-Encryption (PRE):** Allows ciphertexts from multiple clients to be aggregated in a common key domain.  
- **High-Performance Computing (HPC):** Provides scalability and multi-node orchestration for large workloads.  
- **Kafka-based Ingestion Pipeline:** Streams telemetry data securely into client-local storage.  

The result is a **quantum-secure, scalable, and auditable federated learning framework** designed for data center environments and extendable to domains like healthcare and IoT.  

---

## Key Features  

- **End-to-End Privacy**: Raw data never leaves the client. Only encrypted updates are shared.  
- **Quantum-Secure Encryption**: Based on lattice cryptography (RLWE) with Multikey CKKS and PRE.  
- **Scalable Execution**: Orchestrated across HPC nodes using MPI/SSH.  
- **Round-Based Federated Training**: Secure lifecycle with logging and reproducibility.  
- **Real-Time Data Ingestion**: Kafka producers/consumers for telemetry streaming.  
- **Modular Scripts**: Python, C++, and shell scripts for training, encryption, orchestration, and aggregation.  

---

## System Architecture  

### High-Level Flow  
1. **Data Ingestion**: Telemetry is streamed via Kafka producers and consumed into client-local storage.  
2. **Local Training**: Each client trains GRU/LSTM models using TensorFlow/Keras.  
3. **Encryption**: Model weights are encrypted with OpenFHE (Multikey CKKS).  
4. **Aggregation**: Server applies Proxy Re-Encryption and performs homomorphic aggregation.  
5. **Distribution**: Aggregated encrypted model sent back to clients.  
6. **Decryption & Update**: Clients decrypt and update their models.  
7. **Logging**: Round-specific logs and artifacts stored per client for auditability.  

### Components  
- **Client Module**: Training script (`c_trainAndUpdate.py`), Kafka consumers, HE encrypt/decrypt tools.  
- **Server Module**: Aggregation (`aggregateEncryptedWeights`) and PRE (`changeCipherDomain`) functions, Mongoose HTTP endpoints.  
- **Orchestrator**: Round manager (`run.sh`, helper scripts, `oConfig.json`).  
- **Kafka Broker**: Handles publish/subscribe ingestion pipeline.  
- **HPC Cluster**: Runs clients, server, and orchestrator across nodes. 

## Technologies Used  

- **OpenFHE (C++):** Homomorphic encryption primitives (CKKS, PRE).  
- **TensorFlow/Keras (Python):** GRU/LSTM time-series training.  
- **Apache Kafka:** Pub/sub telemetry ingestion.  
- **Mongoose HTTP Server (C):** Lightweight server for encrypted weight transfer.  
- **Python + Shell Scripts:** Orchestration, automation, configuration handling.  
- **HPC Cluster (MPI/SSH):** Parallel execution across multiple nodes.  

---

## Installation & Setup  

### Prerequisites  
- Ubuntu 20.04 or higher  
- Python 3.8+  
- OpenJDK 11  
- OpenFHE libraries  
- Kafka 3.x  
- SSH/MPI enabled HPC environment 

## Quick Setup
To run the project, you’ll need:
- Kafka broker (topics configured per client)
- Producers & consumers (publish/consume telemetry data)
- Client scripts (training + encryption)
- Orchestrator (`run.sh` and `oConfig.json`)

For full step-by-step installation, see [Installation Guide](documentation/Installation_and_Dependencies_Guide_PPQSFL.docx).


This structure ensures clear separation between client data, server aggregation, and orchestration logs for reproducibility.

---

## Usage – Run a Full Federated Learning Round

1. **Start Infrastructure**  
   - Kafka broker with client-specific topics  
   - Producers and consumers  

2. **Orchestration**  
   - Run `./run.sh` (uses `oConfig.json`)  

3. **Training Workflow**  
   - Clients train → encrypt weights → send to server  
   - Server aggregates encrypted weights  
   - Clients decrypt and update local models  

4. **Auditability**  
   - Logs and round artifacts are saved in client-private directories  

---

## Example Applications
- **Data Center Energy Forecasting** – PUE prediction, anomaly detection  
- **Healthcare Federated AI** – secure training on patient data  
- **IoT/Smart Grid Analytics** – privacy-preserving collaboration across devices  

---

## Project Walkthrough Videos
- **see the [Video Resources Folder]** – https://1drv.ms/f/c/e0ca0a8fe3e07c60/EgcFwh0YsiVKsN3vtobRircBz514uRZGRY-EUMFO_x5fsg?e=9bDkut 

---

## Authors & Acknowledgements
- Developed at **C-DAC Pune**  
- Contributors: *Pranali Nikam*,*Prabhat Karlekar*, *Siddhant Bopche* , *Dr. Puneet Bakshi*, *Mr. Vinodh Kumar M*
- Acknowledgements: **HPC resources at CDAC Pune**  


