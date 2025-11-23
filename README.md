
---

# Credit Card Fraud Detection

### CS4612 – Parallel and Distributed Computing

### Final Project Report and Implementation Guide

**Fall 2025**

---

## 1. Overview

This project focuses on detecting suspicious credit card transactions using three different computing models:

1. Serial (baseline)
2. Shared-memory parallelism using OpenMP
3. Distributed-memory parallelism using MPI

The dataset consists of approximately 1.3 million rows of real financial transactions.
Two primary fraud indicators are computed:

1. **Burst Frequency** – A card performing at least three transactions within a five-minute window.
2. **Category Novelty** – A card conducting purchases in new or unusual categories not seen before.

This repository contains the complete implementation, experimental scripts, and performance measurements, including strong scaling and weak scaling behavior.

---

## 2. Dataset

The dataset follows the CSV format below:

```
cc_num,category,unix_time,is_fraud
170000000000000,"grocery",1546443572,0
```

* `cc_num`: credit card number
* `category`: transaction category
* `unix_time`: Unix timestamp
* `is_fraud`: not used in this project (kept for compatibility)

The dataset used in all experiments is stored in:

```
dataset/data.csv
```

Weak-scaling experiments generate extended datasets under:

```
dataset/weak/
```

---

## 3. Repository Structure

```
project/
│
├── serial/
│   ├── project_serial.c
│   ├── README.md
│   └── serial_results.csv
│
├── parallel_omp/
│   ├── Project_parallel.c
│   ├── run_parallel_tests.sh
│   ├── README.md
│   └── parallel_results.csv
│
├── distributed_mpi/
│   ├── Project_MPI.c
│   ├── run_mpi_tests.sh
│   ├── run_mpi_weak_scaling.sh
│   ├── README.md
│   ├── mpi_results.csv
│   └── mpi_weak_scaling.csv
│
├── dataset/
│   ├── data.csv
│   └── weak/
│
└── README.md   (this document)
```

---

## 4. Serial Implementation

**Directory:** `serial/`

This version serves as the performance baseline.

### Build

```
gcc project_serial.c -O2 -o project_serial.exe
```

### Run

```
./project_serial.exe
```

### Example Output (based on latest execution)

```
Rows read: 1296675
Suspicious (Transaction Frequency): 550733
Suspicious (Unusual Categories):    161645
Serial elapsed: 0.239 s
```

The serial implementation loads the dataset, sorts transactions by card number and timestamp, computes both indicators, and reports total runtime.

---

## 5. OpenMP Parallel Implementation

**Directory:** `parallel_omp/`

This version exploits shared-memory parallelism by distributing work among threads after cards are grouped.

### Build

```
gcc Project_parallel.c -fopenmp -O2 -o Project_parallel.exe
```

### Run Example

```
./Project_parallel.exe ../dataset/data.csv 8 dynamic 100
```

### Example Output

```
Rows read: 1296675
Threads used: 8
Schedule: dynamic, Chunk size: 100
Suspicious (Transaction Frequency): 550733
Suspicious (Unusual Categories):    161645
Parallel elapsed: 0.044 s
```

### Experimental Results File

```
parallel_omp/parallel_results.csv
```

---

## 6. MPI Distributed Implementation

**Directory:** `distributed_mpi/`

This version distributes the dataset among MPI ranks using either block or block-cyclic partitioning. Both blocking and non-blocking communication modes are supported.

### Build (MS-MPI on Windows)

```
gcc Project_MPI.c -lmsmpi -O2 -o Project_MPI.exe \
    -I"C:/Program Files/Microsoft MPI/Include" \
    -L"C:/Program Files/Microsoft MPI/Lib/x64"
```

### Run Example

```
mpiexec -n 4 ./Project_MPI.exe ../dataset/data.csv --mode block --comm blocking
```

### Example Output (latest measured run)

```
=== FINAL RESULTS ===
mode=block comm=blocking P=1
Rows: 1296675
Suspicious (Transaction Frequency): 614845
Suspicious (Unusual Categories):    161645
t_io=0.760s, t_comm=0.019s, t_comp=1.669s
t_total=2.448s
```

### Strong Scaling Results

Located in:

```
distributed_mpi/mpi_results.csv
```

### Weak Scaling Results

Located in:

```
distributed_mpi/mpi_weak_scaling.csv
```

---

## 7. Strong Scaling Summary

The purpose of the strong-scaling experiment is to measure how runtime changes when the dataset size remains fixed and the number of processing units increases.

Summary (excerpt from `distributed_mpi/mpi_results.csv`):

| Processes | Mode  | Communication | Mean Time (s) | Speedup | Efficiency |
| --------- | ----- | ------------- | ------------- | ------- | ---------- |
| 1         | block | blocking      | 3.01          | 0.85    | 0.85       |
| 2         | block | blocking      | 3.17          | 0.81    | 0.40       |
| 4         | block | blocking      | 2.30          | 1.12    | 0.28       |
| 8         | block | blocking      | 1.01          | 2.54    | 0.31       |

Observations:

* MPI scaling is slower than OpenMP due to communication and I/O overhead.
* Non-blocking communication improves runtime slightly.
* For this dataset, block distribution performs better than block-cyclic.

---

## 8. Weak Scaling Summary

Weak scaling evaluates how runtime changes when both the dataset size and the number of processes increase proportionally.

Summary (from `distributed_mpi/mpi_weak_scaling.csv`):

| Processes | Mean Time (s) |
| --------- | ------------- |
| 1         | 2.44          |
| 2         | 2.51          |
| 4         | 2.65          |
| 8         | 2.83          |

Observations:

* The total runtime grows slowly as workload increases.
* MPI demonstrates near-ideal weak scaling behavior for this application.

---

## 9. Experimental Environment

The following configuration was used to conduct all experiments:

* Operating System: Windows 11 (64-bit)
* CPU: Intel Core i7 (8 cores)
* RAM: 16 GB
* Compiler: GCC 15.2 (MinGW-w64)
* MPI Runtime: Microsoft MPI (MS-MPI) v10.1
* Dataset Size: ~354 MB

---

## 10. Conclusion

This project demonstrated the behavior of serial, OpenMP, and MPI implementations on a real-world financial dataset.

Key findings:

1. The serial version provides a consistent baseline for comparison.
2. The OpenMP implementation achieved up to a 4× speedup on 8 threads.
3. The MPI implementation provided moderate strong-scaling speedup, limited by data distribution and communication overhead.
4. Weak scaling results were strong, with performance degrading only slightly as dataset size and process count increased.
5. All implementations produced consistent fraud-detection results.

This work meets the requirements of the CS4612 course project, covering shared-memory parallelism, distributed-memory parallelism, performance tuning, and full scaling experiments.

---
 