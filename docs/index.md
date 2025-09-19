# Privacy Preserving Quantum Secure Federated Learning Framework (PPQSFLHE)

Welcome to the documentation hub for our R&D project.  
This repository contains design documents, testing guides, and explanatory videos to help you understand the architecture and workflow of the project.

---

## 📚 Documentation

- [High-Level Design (HLD)](High-Level%20Design%20Document.docx)
- [Low-Level Design (LLD)](Low-Level%20Design%20Document.docx)
- [Testing Guide](Testing_Documentation_PPQSFL.docx)
- [Installation & Dependencies Guide](Installation_and_Dependencies_Guide_PPQSFL.docx)

---

## 🎥 Videos

- [PPQSFLHE_1 (OneDrive link)](https://1drv.ms/v/c/e0ca0a8fe3e07c60/EVSkR8qV4ZlJtUocf6Ao26YBvmMa0icgalFI7sLlM-EesA?e=hKAZ5K)  
- [PPQSFLHE_2 (OneDrive link)](https://1drv.ms/v/c/e0ca0a8fe3e07c60/Ebhu1mu6XFVDhgLtBBmMqgQBwPtHzcQHK6TCX04Jxvg10Q?e=SJNdwB)

---

## 🚀 Repository Structure

The project is organized as follows:
PPQSFLHE/
├── client/ # Client-side encryption, key generation, training
├── server/ # Server-side aggregation & crypto context
├── orchestration/ # Scripts to orchestrate FL pipeline
├── lib/ # External libraries (OpenFHE, Mongoose, etc.)
├── utils/ # Helper libraries (cereal, json, etc.)
├── test/ # Testing framework and scripts
├── docs/ # Documentation and resources
└── release/ # Release binaries
