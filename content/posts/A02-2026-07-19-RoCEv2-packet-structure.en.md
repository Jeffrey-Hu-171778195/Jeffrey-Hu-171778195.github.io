+++
title = 'A02 | RoCEv2 Packet Structure and RDMA Core Objects'
date = 2026-07-19T22:35:00+08:00
draft = false
series = 'From AP to AI Network'
weight = 2
tags = ['RDMA', 'RoCEv2', 'Packet Structure', 'QP', 'MR', 'CQ']
description = 'Deep dive into RDMA core objects (PD/MR/QP/CQ/WQ/WC) and RoCEv2 packet structure, including BTH, ICRC, UDP port 4791 and other key fields.'
+++

## 1. RDMA Core Objects

RDMA programming involves a set of core objects that together form the foundation of RDMA communication.

### 1.1 PD — Protection Domain

**A PD is an isolation boundary.** A PD groups together QPs, MRs, and CQs. Objects in different PDs cannot access each other. Multiple QPs and MRs can belong to the same PD, not a one-to-one relationship.

Structure:

```
PD
├── QP1
├── QP2
├── MR A
├── MR B
└── CQ
```

### 1.2 MR — Memory Region

Before RDMA can access memory directly, the memory must be "registered."

```
Allocate memory → Register MR → Get two keys
                                 ├── lkey (local key): validates local operations
                                 └── rkey (remote key): validates remote access
```

**Why is MR needed?**
- The RNIC (RDMA NIC) directly DMA-reads/writes host memory
- The RNIC must know which memory regions are "legal"
- The rkey is sent to the remote side, which uses it to read/write your memory

### 1.3 QP — Queue Pair

Each QP contains two queues:

```
┌─────────────────────────────────────┐
│              QP                     │
│  ┌──────────────┐                   │
│  │  SQ (Send Q) │  ← post send WQEs│
│  └──────────────┘                   │
│  ┌──────────────┐                   │
│  │  RQ (Recv Q) │  ← post recv WQEs│
│  └──────────────┘                   │
└─────────────────────────────────────┘
```

Each QP has a unique **QPN (QP Number)**. When sending a packet, the QPN is placed in the BTH to tell the receiver: "This packet is for this QP."

**Note:** A QP does not correspond to a specific MR. You can use different MRs with the same QP, as long as they belong to the same PD.

### 1.4 CQ — Completion Queue

The CQ holds the status of completed operations.

```
SQ ──→ complete ──→  CQ
RQ ──→ complete ──→  CQ ← poll to get WC
```

- SQ and RQ can be bound to the **same CQ** (simple, common in pingpong)
- Or to **different CQs** (performance tuning, independent polling)
- The completion notification taken from CQ is called **WC (Work Completion)**

### 1.5 WQ / WQE / WC

```
WQ  (Work Queue)         = the queue itself (SQ or RQ structure)
WQE (Work Queue Element) = each task in the queue
     Calling ibv_post_send() → submits a WQE into SQ
WC  (Work Completion)    = completion notification, polled from CQ
```

### 1.6 Object Relationship Summary

```
Application
    │
    ├── ibv_open_device() → Context (ctx)
    │
    ├── ibv_alloc_pd() → PD
    │   ├── ibv_reg_mr() → MR (lkey + rkey)
    │   ├── ibv_create_qp() → QP
    │   │   ├── SQ (send queue)
    │   │   └── RQ (recv queue)
    │   └── ibv_create_cq() → CQ
    │
    └── Communication flow:
        ibv_post_send(QP, WQE)  → WQE into SQ
        ibv_poll_cq(CQ)         → retrieve WC
```

---

## 2. RoCEv2 Packet Structure

### 2.1 Packet Encapsulation Layers

```
┌────────────────────────────────────────────────────────────────┐
│                     Ethernet Header (14B)                      │
│        MAC dst (6B) | MAC src (6B) | EtherType (2B)           │
└────────────────────────────────────────────────────────────────┘
┌────────────────────────────────────────────────────────────────┐
│                      IP Header (20B)                           │
│  Version | TOS | Total Len | ID | Flags | TTL | Proto=UDP     │
│                   src IP | dst IP                              │
└────────────────────────────────────────────────────────────────┘
┌────────────────────────────────────────────────────────────────┐
│                      UDP Header (8B)                           │
│    src port (random)  |  dst port (4791)  |  Len  | Checksum  │
└────────────────────────────────────────────────────────────────┘
┌────────────────────────────────────────────────────────────────┐
│                   BTH — Base Transport Header (12B)            │
│ Opcode(1B) | Flags+Pad(1B) | PSN(4B) | ... | QP dst(4B)      │
└────────────────────────────────────────────────────────────────┘
┌─────────────────┬──────────────────────────────────────────────┐
│  Ext Header     │                 Payload                      │
│  RETH(16B)      │         Actual data being transferred        │
│  AETH(4B)       │                                               │
│  ...            │                                               │
└─────────────────┴──────────────────────────────────────────────┘
┌────────────────────────────────────────────────────────────────┐
│     ICRC (4B) — Invariant CRC                                  │
│     Calculated by sender RNIC hardware, verified by receiver   │
│     Covers BTH through Payload entirely                        │
└────────────────────────────────────────────────────────────────┘
┌────────────────────────────────────────────────────────────────┐
│      FCS (4B) — Frame Check Sequence                           │
│      Calculated by each switch, link-level only                │
└────────────────────────────────────────────────────────────────┘
```

### 2.2 BTH Detail (Base Transport Header)

BTH is **12 bytes** and is mandatory for every RoCEv2 message.

```
Offset  Byte      Field                  Description
─────── ──────    ─────────────────────  ───────────────────
0x00    B0        Opcode (8 bits)        Top 3 bits: QP type (RC=0)
                                         Bottom 5 bits: operation
                                         - 0x00: RC SEND First
                                         - 0x01: RC SEND Middle
                                         - 0x02: RC SEND Last
                                         - 0x08: RDMA WRITE First
                                         - 0x09: RDMA WRITE Middle
                                         - 0x0A: RDMA WRITE Last
                                         - 0x0C: RDMA READ Request
                                         - 0x0D: RDMA READ Response First
                                         - 0x0E: RDMA READ Response Last
                                         - 0x10: Acknowledge (ACK)
0x01    B1        Flags + Pad (8 bits)   S/M/L flags + pad byte count
0x02-05 B2-B5     PSN (32 bits)          Packet Sequence Number, RC guarantees ordering
0x06-07 B6-B7     A-inv+A-Key (16 bits)  Used for atomic operations
0x08-0B B8-B11    QPN (32 bits)          Destination QP Number
```

**Opcode 8-bit structure:**

```
bit 7 6 5 4 3 2 1 0
    └───┬──┘ └───┬──┘
    Top 3 bits    Bottom 5 bits
    (QP type)    (Operation)
    
Top 3 bits:
    000 = RC (Reliable Connection)
    001 = UC (Unreliable Connection)
    100 = UD (Unreliable Datagram)

Common bottom 5 bits:
    00000 = SEND First
    00100 = RDMA WRITE First
    01100 = RDMA READ Request
    10000 = ACK
```

### 2.3 Extended Headers

Different operations carry different headers after BTH:

| Operation | Ext Header | Length | Content |
|-----------|-----------|:------:|---------|
| SEND | None | 0B | Payload only |
| RDMA WRITE | RETH | 16B | Virtual addr + rkey + DMA length |
| RDMA READ Request | RETH | 16B | Remote addr + rkey + length |
| RDMA READ Response | AETH | 4B | ACK status + MSN |
| ACK / NAK | AETH | 4B | Confirm/reject status |

**RETH (RDMA Extended Transport Header) structure:**

```
Offset  Field        Description
0x00    VA (8B)     Remote virtual address
0x08    rkey (4B)   Remote key
0x0C    DMA Len     Transfer length
```

### 2.4 UDP Port 4791

```
Destination port: 4791 (fixed, IANA assigned for RoCEv2)
Source port:      random
```

**Why random source port?**

Switches use ECMP (Equal Cost Multi-Path) load balancing by computing a hash over the 5-tuple `(src_ip, dst_ip, src_port, dst_port, proto)`. If all flows had the same source port, the hash would concentrate on a single link. Random source ports allow different QP flows to **spread across different physical paths**, improving overall bandwidth utilization.

### 2.5 ICRC vs FCS

| Checksum | Calculated by | Scope | Purpose |
|----------|-------------|-------|---------|
| **ICRC** | End-node RNIC hardware | BTH ~ Payload | **End-to-end** data integrity |
| **FCS** | Each switch | Entire Ethernet frame | **Per-link** integrity |

**Why ICRC matters:**

Even if a switch introduces an error while recalculating FCS, the receiver RNIC's ICRC check will detect it. This is one of the key guarantees of RoCEv2 lossless networking.

---

## 3. RDMA Read vs Write

### RDMA Read (One-sided Read)

```
Initiator                               Target
┌──────────┐                        ┌──────────┐
│  Initiate│ ── RDMA READ Request → │  No CPU  │
│  Read    │                        │  needed  │
│          │ ← RDMA READ Response ── │          │
│          │    (carries data)      │          │
└──────────┘                        └──────────┘
```
- Initiator sends a Read Request, target RNIC **reads memory directly** and returns data
- Target CPU is **completely unaware** of the data transfer
- Requires target's **rkey + virtual address**

### RDMA Write (One-sided Write)

```
Initiator                               Target
┌──────────┐                        ┌──────────┐
│  Initiate│ ── RDMA WRITE ────────→ │  No CPU  │
│  Write   │    (data + rkey)        │  needed  │
└──────────┘                        └──────────┘
```
- Initiator writes data directly into the target's memory
- Target CPU is **completely unaware** of the write
- Requires target's **rkey + virtual address**

### SEND/RECV (Two-sided)

```
Initiator                               Target
┌──────────┐                        ┌──────────┐
│ post_send│ ──── SEND ───────────→ │post_recv │
│   data   │                        │ receive  │
└──────────┘                        └──────────┘
```
- Both sides participate: target must post RECV WQE in advance
- No rkey needed, **no need to know remote memory address**
- Safer, but requires CPU involvement

### Comparison

| Operation | CPU Involvement | Needs rkey | Typical Use Case |
|-----------|:--------------:|:----------:|------------------|
| **SEND/RECV** | Both sides | ❌ | Small messages, control plane |
| **RDMA WRITE** | Initiator only | ✅ | Gradient sync (NCCL) |
| **RDMA READ** | Initiator only | ✅ | Parameter fetching |

---

## 4. Local Practice Notes

```c
#include <infiniband/verbs.h>

// 1. Create Context
struct ibv_device *ib_dev = /* ... get device */;
struct ibv_context *ctx = ibv_open_device(ib_dev);  // using rxe0

// 2. Create PD
struct ibv_pd *pd = ibv_alloc_pd(ctx);

// 3. Register MR (get lkey + rkey)
char *buf = malloc(BUF_SIZE);
struct ibv_mr *mr = ibv_reg_mr(pd, buf, BUF_SIZE,
    IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ);

// 4. Create CQ
struct ibv_cq *cq = ibv_create_cq(ctx, 128, NULL, NULL, 0);

// 5. Create QP (current state: RESET)
struct ibv_qp_init_attr qp_attr;
memset(&qp_attr, 0, sizeof(qp_attr));
qp_attr.send_cq = cq;
qp_attr.recv_cq = cq;  // can be a different CQ
qp_attr.qp_type = IBV_QPT_RC;
// ... other init params
struct ibv_qp *qp = ibv_create_qp(pd, &qp_attr);
// qp number = qp->qp_num
```

Completed:
1. ✅ Create Context (`ibv_open_device`)
2. ✅ Create PD (`ibv_alloc_pd`)
3. ✅ Register MR (`ibv_reg_mr`，get lkey + rkey)
4. ✅ Create CQ (`ibv_create_cq`)
5. ✅ Create QP (`ibv_create_qp`，current state: **RESET**)

Next step: **QP state machine — Reset → Init → RTR → RTS**

---

*Series home: [From AP to AI Network](/en/series/from-ap-to-ai-network/)*  
*Previous: [A01 | Why LLM Training Needs RDMA](/en/posts/a01-2026-07-05-why-llm-needs-rdma/)*  
*Next: [A03 | QP State Machine and Pingpong Communication →]()*