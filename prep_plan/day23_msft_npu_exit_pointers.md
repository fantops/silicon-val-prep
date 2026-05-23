# Day 23 — Microsoft NPU Exit Pointers + Retained QNN Bug Story

## Concept

You're leaving the Microsoft Surface NPU/ORT EP role for a silicon-validation role. **One day is enough** — extract the highest-signal pointers from the current work so you can answer "tell me about your last role" credibly without making it the centre of the interview narrative. Two things to walk away with: **(a) a probe-list of things to learn before you leave** (vendor escalation contacts, internal docs, version assumptions) and **(b) one retained bug story** (the QNN page-fault) that you can re-use as a "tell me about a hard bug" anchor whenever the interview drifts to general debug skill.

## Why this day in the flow

The plan is **for a silicon-validation role**, not for MSFT continuation. You spent Phases 1-3 building the silicon-val story (CPU arch + CPU PM + PCIe). Today is the small, deliberate sweep that extracts what's worth carrying from current work — *without* getting drawn back into NPU-as-identity.

In the Layer map this is one of two **wrap days** before the final mock (D24). Short, dense, list-like.

---

## Probe-Before-Leaving Checklist

Things to learn *now* while you still have access:

### 1. Vendor stack versions in your current stack

- Hexagon HTP version(s) shipping on which Surface SKU.
- QNN SDK version(s); the diff between the QNN you're testing and the QNN your test failure was reported against.
- ORT version + EP plugin matrix that maps to each QNN version.
- EdgeOptimizer version + the model-conversion knobs it exposes.

Why: any future debug conversation with an ex-colleague hinges on naming the exact version triplet (QNN/ORT/HTP). Document one matrix.

### 2. Escalation contacts

- Qualcomm AE / FAE for HTP issues (name, email, escalation tier).
- ORT EP maintainer (Microsoft).
- QNN SDK contact (Qualcomm).
- Internal triage rotations + the doc that lists them.

Why: these contacts disappear from your address book the moment your badge expires. Save them externally.

### 3. Internal docs to read once

- The QNN compilation flow doc (model → HTP graph).
- The runtime tensor-allocation policy doc.
- The page-table layout for Hexagon's secure context.
- The Surface PMC ↔ HTP wake-handshake spec (if you can access).
- Any internal post-mortem docs on the marquee bugs of the last 12 months.

Why: pattern-recognition. Even though the next role is unrelated, the *style* of vendor-IP-on-host bring-up is universal.

### 4. Open bug status (yours)

For every CR you own:
- Current state, owner, blocking dependency.
- Hand-off plan: who picks it up.
- Repro steps written to be readable by someone who never sat with the issue.

Why: leaving without a clean hand-off burns bridges. Plus, "I left a clean hand-off" is a behavioural answer.

### 5. Surface-area shrink

- Identify one technique from current work that maps directly to IP val:
  - **Multi-version regression triage** — same skill as silicon-stepping regression.
  - **Cross-vendor debug** (host SW + vendor IP) — same skill as cross-IP in silicon val.
  - **Long-soak monitoring + telemetry attribution** — same skill as PM soak.

Have one sentence ready: "I used to do X at Microsoft; the IP-Val equivalent is Y."

---

## Retained QNN Bug Story (90 sec)

This is the bug you'll re-use as "tell me about a hard bug you debugged."

### Setup (10 sec)
> "We had a Surface NPU running a vision model through QNN's HTP backend. Production telemetry started reporting a 0.01% rate of inference failures — a sub-second hang followed by a recovery. Only on one specific model and only after the device had been idle for >5 minutes."

### Reproduction effort (15 sec)
> "Local repro took a week. The trigger was a cold path: HTP power-gated by the platform PMC during idle; on wake, the first inference issued a tensor read to a page that hadn't been re-faulted into HTP's MMU. The page-fault path existed, but the platform's wake-handshake had a race — the IOMMU stream-table was restored *after* the HTP issued its first DMA. Result: HTP saw an unmapped IOVA, took a fault, the QNN runtime didn't have a fault-recovery path that could resume from that state, so it timed out and the application saw a hang."

### Telemetry path (10 sec)
> "Surfaced from production telemetry; isolated by correlating fault events with idle-state-exit events in PMC traces; ground-truth came from a Hexagon-side debug dump that showed the IOVA at the moment of fault."

### Root cause (10 sec)
> "Two bugs stacked: a wake-handshake race in the platform PMC and a missing fault-recovery in the QNN runtime. The platform fix was a stream-table-restore-before-first-DMA barrier. The runtime fix was a fault-aware retry path."

### Lesson (15 sec)
> "The pre-Si vplan for the platform should have had a coverpoint on `(NPU-wake × IOMMU-stream-state)`. It didn't. We added one. The runtime side added a fault-retry test. The escape happened because each team validated their own surface in isolation; the cross-IP scenario fell through the gap."

### Transition to next role (10 sec)
> "That's exactly why I want to move to IP Functional Validation — the cross-IP coverage gap is the single highest-value place to spend validation effort, and I've now seen the field debug experience that motivates it."

---

## How to Use This Story in Interview

| Asked | Response strategy |
|---|---|
| "Tell me about a hard bug" | Use this story. End on the cross-IP coverage gap → tie to the IP val role. |
| "Tell me about your current role" | One-sentence summary (NPU + ORT EP val for Surface), then immediately pivot: "the most transferable skill I've built is cross-vendor cross-IP debug, which is why I want IP Functional Val." |
| "Why are you leaving?" | "I want to validate at the IP boundary itself, not at the SW-on-top-of-IP boundary. The bug above taught me where the highest-leverage gap lives, and that's at the IP val team." |
| "What did you learn that you'll bring to the new role?" | (1) Multi-version regression triage. (2) Telemetry-driven bug attribution. (3) Cross-team escalation. (4) The discipline of writing a coverage row from a field bug. |

**Do not let NPU/QNN take more than 90 sec of any answer.** The role is IP Functional Val. NPU is the past chapter, not the headline.

---

## What's Required

- Personal copy of the bug story (above) written out in your own words.
- Version matrix (QNN/ORT/HTP/Surface SKU).
- Three external contacts saved.
- Linkedin updated to reflect the transition reason in one line.
- A one-page "what I'm leaving behind" doc for the next owner of each open CR.

---

## Tools (Recap, Light)

- ORT (Onnx Runtime), QNN SDK, Hexagon HTP toolchain, EdgeOptimizer.
- Internal MSFT debug + telemetry stack.
- Hexagon SDK / debugger if you've used it.
- Qualcomm support portal account.

(Not deep — this is exit-mode reading, not a new study area.)

---

## Bugs in this Topic

Only the one bug story above. Keep it sharp; don't accumulate more.

---

## Exit Check

- [ ] Wrote the 90-sec QNN bug story end-to-end in your own words.
- [ ] Captured version matrix + escalation contacts externally.
- [ ] Hand-off doc written for each open CR you own.
- [ ] Practised the "why are you leaving?" answer in one breath.
- [ ] Practised the "tell me about your current role" answer in 60 sec or less.
- [ ] Identified the one sentence connecting NPU work to IP Val.
- [ ] **Did not** expand this beyond one day. Move on to D24.
