+++
title = "Why Large Language Model Training Can't Live Without RDMA"
date = 2026-07-05T15:22:13+08:00
draft = false
series = 'From AP to AI Network'
weight = 1
tags = ['RDMA', 'AI Networking', 'LLM']
description = 'Why does large model training require RDMA? This article starts with the communication bottleneck, analyzes the communication patterns of data parallelism, tensor parallelism, pipeline parallelism and MoE parallelism, and explains why TCP falls short while RDMA becomes the backbone of AI networks.'
+++

## 1. The Core Contradiction of AI Training

Large model training has one core contradiction: **the more compute power you have, the longer you wait for data.**

Take a GPT-4 class model: training requires thousands of GPUs working together. A single H100 delivers 2000 TFLOPS, but data must travel across the network from GPU A to GPU B. Network transfer speeds are far slower than GPU compute speeds.

```
GPU compute:   2000 TFLOPS (2×10¹⁵ operations/second)
NIC bandwidth: 400 Gbps   (5×10¹⁰ bytes/second)
```

This means GPUs spend most of their time **waiting for data**.

---

## 2. Four Parallelism Strategies and Their Communication Patterns

Large model training doesn't simply run a model on a single GPU; it splits the model and data across multiple GPUs. Different splitting strategies result in different communication patterns.

### 2.1 Data Parallelism

Each GPU holds a complete model replica but processes different data batches.

**Communication pattern:** AllReduce gradient synchronization after forward/backward passes

**Communication pressure:** Grows linearly with model size

### 2.2 Tensor Parallelism

Splits individual matrix operations across multiple GPUs.

**Communication pattern:** AllReduce required for every forward/backward operation

**Communication pressure:** **Extremely high** — each computation step involves communication

### 2.3 Pipeline Parallelism

Places different model layers on different GPUs, executing sequentially.

**Communication pattern:** P2P Send/Recv, only at layer boundaries

**Communication pressure:** Relatively low, but latency-sensitive

### 2.4 MoE Parallelism (Mixture of Experts)

Only a subset of "expert" networks are activated; routing determines which experts process which data.

**Communication pattern:** All-to-All communication

**Communication pressure:** **Extremely high** — frequent cross-expert communication

### Summary of Communication Patterns

| Strategy | Communication Pattern | Volume | Latency Sensitivity |
|---------|---------------------|:------:|:------------------:|
| Data Parallelism | AllReduce | High | Medium |
| Tensor Parallelism | AllReduce | **Very High** | **Very High** |
| Pipeline Parallelism | P2P Send/Recv | Low | High |
| MoE Parallelism | All-to-All | **Very High** | Medium |

> In practice, these four strategies are often **combined**: Data Parallelism × Tensor Parallelism × Pipeline Parallelism × MoE. This means communication pressure multiplies.

---

## 3. Communication Bottleneck Numbers

For a 100B parameter model:

```
AllReduce one gradient sync ≈ 400 GB data
With 200 Gbps network: transfer time ≈ 16 seconds
With 400 Gbps RDMA:  transfer time ≈ 8 seconds
But one H100 iteration may take only 5 seconds
```

This means: **if the network isn't fast enough, GPUs spend more time communicating than computing.**

---

## 4. Why TCP Doesn't Work

Traditional data centers run TCP/IP, but TCP has fatal issues in large-scale AI training:

### Problem 1: High CPU Overhead

TCP requires CPU involvement for every data transfer:

```
NIC → CPU memory (DMA) → Kernel TCP stack →
Socket buffer → Application buffer → Destination
```

Data is **copied at least 4 times** in memory, each copy consuming CPU time and memory bandwidth.

### Problem 2: High Latency

TCP's kernel protocol stack adds tens of microseconds of latency. This is acceptable for web services but unacceptable for AI training that requires millisecond-level AllReduce completion.

### Problem 3: Limited Throughput

In large-scale clusters, TCP's congestion control (slow start, congestion avoidance) performs poorly in complex network topologies, often achieving only 60-80% throughput utilization.

---

## 5. How RDMA Solves These Problems

RDMA (Remote Direct Memory Access) has a simple core idea: **move data directly from GPU A's memory to GPU B's memory, bypassing the CPU.**

```
Traditional TCP:
GPU A memory → A CPU → A Kernel → Wire → B Kernel → B CPU → GPU B memory
               ↑     ↑                   ↑     ↑
               Four copies, CPU fully involved

RDMA:
GPU A memory → A RNIC → Wire → B RNIC → GPU B memory
              ↑                   ↑
              Zero copy, CPU not involved
```

### RDMA's Three Key Advantages

| Metric | TCP | RDMA |
|--------|:---:|:----:|
| Data copies | 4 | 0 (zero-copy) |
| CPU offload | No | Yes (handled by RNIC) |
| Latency (cross-node) | 50-100 μs | 1-3 μs |
| Throughput utilization | 60-80% | 95%+ |

### Three RDMA Implementations

The most dominant in AI networks is **RoCEv2** (RDMA over Converged Ethernet v2):

| Implementation | Characteristics | AI Network Adoption |
|---------------|----------------|:------------------:|
| **InfiniBand** | Dedicated network, best performance, highest cost | NVIDIA DGX standard |
| **RoCEv2** | Ethernet-based, low cost, good ecosystem | **Mainstream AI clusters** |
| iWARP | TCP-based, moderate performance | Rarely used for AI |

---

## 6. Industry Practice: RoCEv2 + NCCL

NVIDIA's NCCL (NVIDIA Collective Communications Library) is the **de facto communication library standard** in AI training. It uses RDMA under the hood:

```
User space: NCCL library
   ↓
System space: libibverbs (RDMA verbs API)
   ↓
Hardware: RoCEv2 / InfiniBand RNIC
```

Typical AI cluster topology:

```
┌─────────────┐    400 Gbps RoCE
│  GPU Server ├─────────────────┐
│  (8× H100)  │                 │
│  NCCL+RDMA  │                 │
└──────┬──────┘                 │
       │ PCIe Gen5              │
       │                        │
  ┌────┴────┐            ┌──────┴──────┐
  │ ConnectX│◄───────────│  ToR Switch │
  │  7 NIC  │  400Gbps   │  (Lossless) │
  └─────────┘            └─────────────┘
```

---

## 7. Summary

**One sentence answer to the title question:**

> Large model training can't live without RDMA because its parallelization strategies (data/tensor/pipeline/MoE) generate **massive communication demands**. Traditional TCP cannot meet the CPU overhead, latency, and throughput requirements, while **RDMA's zero-copy, CPU offload, and ultra-low latency** make it the inevitable choice for AI networks.

### Next Post Preview

The next post will guide you through **getting started with RDMA at zero cost**, running your first RDMA program using soft-RoCE on a regular Linux machine.

---

*Series home: [From AP to AI Network](/en/)*  
*Next: [Getting Started with RDMA: Your First Program Using soft-RoCE →]()*