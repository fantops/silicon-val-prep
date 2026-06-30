# Day 4 — Bit Manipulation Mastery

## Concept

A reflex-level bit toolkit so register/MMIO/mask work and "trick" questions come instantly: **power-of-2** detection, **floor / ceil / next** power of two (the missed Broadcom question), **set / clear / toggle / test**, **popcount**, **find-first/last-set**, masks and fields, **rotations**, **byte-swap / endianness**, **bit reversal**, **XOR tricks**, **sign extension**, and **saturating arithmetic**. Coding is already a strength (8/10) — this day plugs the *specific* holes and makes the one-liners automatic.

## Why this day in the flow

This consolidates the bit tricks scattered through [Day 3](day03_bits_mmio_volatile_polling.md) (MMIO field RMW, the `1U<<31` and widen-before-shift traps) into a dedicated reflex bank, and connects to Phase 1: these operations **are** combinational logic — `popcount` is an adder tree, **find-first-set is a priority encoder** ([Day 5](day05_boolean_combinational_logic.md)), parity/ECC is an XOR tree ([Day 15](day15_memory_systems_ecc.md)). Broadcom R2 missed **floor power of two**; this is the targeted fix, and it sharpens the strength you want to keep.

**Broadcom R2 gap this closes:** floor-power-of-2 (missed), bit manipulation / binary thinking (self-rated 4/10).

---

## Deep Study Notes

### 1. The four register patterns + the golden identities

```c
reg |=  (1U << b);        // SET   bit b
reg &= ~(1U << b);        // CLEAR bit b
reg ^=  (1U << b);        // TOGGLE bit b
if (reg & (1U << b)) ...  // TEST  bit b
```
Always `1U` — `1 << 31` on a signed `int` is **undefined behaviour**. The three identities everything else is built from:

```c
x & (x - 1)     // clears the LOWEST set bit      → popcount, isPow2
x & (-x)        // isolates the LOWEST set bit     → ffs, "lowest one"
x | (x - 1)     // sets all bits BELOW lowest set  → fill trailing zeros
```

### 2. Power of two — detect, and the floor/ceil/next family

```c
bool is_pow2(uint32_t x) { return x && !(x & (x - 1)); }   // exactly one bit set
```
`x & (x-1)` clears the lowest set bit; if that yields 0 *and* `x≠0`, there was exactly one bit → power of two.

**Floor power of two — the missed question** (largest `2^k ≤ x`, for `x>0`):
```c
// Method A — bit-smear, then strip all but the top bit
uint32_t floor_pow2(uint32_t x) {
    x |= x >> 1;  x |= x >> 2;  x |= x >> 4;
    x |= x >> 8;  x |= x >> 16;          // smear top bit down → 000111..1
    return x - (x >> 1);                 // 0001111 - 0000111 = 0001000 = top bit
}
// Method B — via leading zeros (single instruction on real HW)
uint32_t floor_pow2_clz(uint32_t x) { return 1U << (31 - __builtin_clz(x)); }
```

**Ceil / next power of two** (smallest `2^k ≥ x`):
```c
uint32_t ceil_pow2(uint32_t x) {
    if (x <= 1) return 1;
    x--;                                  // so exact powers map to themselves
    x |= x>>1; x|=x>>2; x|=x>>4; x|=x>>8; x|=x>>16;
    return x + 1;
}
```
The **smear** (propagate the top set bit into every lower position) is the universal move: `floor` = smear then `x-(x>>1)`; `ceil` = `x-1`, smear, `+1`. Say *"I smear the high bit down, then either strip to the top bit for floor or add one for ceil."* Watch `x==0` (undefined floor) and `x==1` edges.

### 3. Population count (popcount) — four ways

```c
int pc_naive(uint32_t x){ int n=0; while(x){ n+=x&1; x>>=1;} return n; }     // O(bits)
int pc_kern (uint32_t x){ int n=0; while(x){ x&=x-1; n++; } return n; }      // O(set bits)
// SWAR parallel (no loop): pairwise sums up the tree
int pc_swar(uint32_t x){
  x = x - ((x>>1)&0x55555555);
  x = (x&0x33333333) + ((x>>2)&0x33333333);
  x = (x + (x>>4)) & 0x0F0F0F0F;
  return (x * 0x01010101) >> 24;
}
// or just __builtin_popcount(x)  →  POPCNT instruction
```
**Brian Kernighan** (`x &= x-1`) loops once per *set* bit — the elegant answer. SWAR is the branch-free, constant-time version (an adder tree, the [Day 5](day05_boolean_combinational_logic.md) hardware popcount in software). Uses: Hamming distance, ECC syndrome weight, sparse-set sizes.

### 4. Find first / last set, count leading / trailing zeros

```c
__builtin_ffs(x)   // 1 + index of lowest set bit (0 if x==0)
__builtin_ctz(x)   // count trailing zeros = index of lowest set bit  (x!=0)
__builtin_clz(x)   // count leading zeros  → floor(log2) = 31 - clz   (x!=0)
```
`ctz` = position of the lowest one (`x & -x` isolates it); `clz` gives **`floor(log2(x)) = 31 - clz(x)`** — the integer log. In hardware, **find-first-set is exactly a priority encoder** ([Day 5](day05_boolean_combinational_logic.md) §6): same operation, used for interrupt arbitration and allocator free-lists. Guard `x==0` (the builtins are undefined there).

### 5. Masks and fields (the MMIO core, recap [Day 3](day03_bits_mmio_volatile_polling.md))

```c
uint32_t mask_w(int w)      { return (w==32) ? ~0U : ((1U<<w)-1); }   // width-w mask
uint32_t get_field(uint32_t r,int lo,int w){ return (r>>lo) & mask_w(w); }
uint32_t put_field(uint32_t r,int lo,int w,uint32_t v){
    uint32_t m=mask_w(w)<<lo; return (r & ~m) | ((v<<lo)&m); }         // RMW: clear then set
```
**Two UB traps to say aloud:** `1U<<w` when `w==32` is **undefined** (shift ≥ width) — hence the `w==32` special case; and forgetting the `& ~m` clear leaves stale field bits (the #1 RMW bug). On **W1C** status registers, a read-modify-write silently clears every set status bit ([Day 3](day03_bits_mmio_volatile_polling.md)).

### 6. Rotations — and why C has no rotate operator

```c
uint32_t rol(uint32_t x,int n){ n&=31; return (x<<n) | (x>>((32-n)&31)); }
uint32_t ror(uint32_t x,int n){ n&=31; return (x>>n) | (x<<((32-n)&31)); }
```
C only has *shifts*; a rotate is two shifts OR'd. The `n&=31` guard avoids the **shift-by-≥width UB** when `n` is 0 or 32. Rotations appear in CRC, hashing, crypto, and SerDes bit alignment.

### 7. Byte swap / endianness

```c
uint16_t bswap16(uint16_t x){ return (x<<8)|(x>>8); }
uint32_t bswap32(uint32_t x){
    return  (x<<24) | ((x&0xFF00)<<8) | ((x>>8)&0xFF00) | (x>>24); }
// or __builtin_bswap32(x)
```
Endianness = byte order of a multi-byte value in memory; **little-endian** stores the LSB first. Parsing on-wire headers (TLP/Ethernet/NVMe — [Day 3](day03_bits_mmio_volatile_polling.md)) often needs a swap. *Bit* order (within a byte) is separate — that's reversal.

### 8. Bit reversal — divide-and-conquer

```c
uint32_t reverse32(uint32_t x){
    x = ((x&0x55555555)<<1)|((x>>1)&0x55555555);   // swap adjacent bits
    x = ((x&0x33333333)<<2)|((x>>2)&0x33333333);   // swap pairs
    x = ((x&0x0F0F0F0F)<<4)|((x>>4)&0x0F0F0F0F);   // swap nibbles
    return __builtin_bswap32(x);                    // swap bytes → fully reversed
}
```
The SWAR pattern: swap adjacent, then pairs, then nibbles, then bytes — `log2(width)` steps. Uses: FFT index reversal, some CRC/LFSR conventions, SerDes lane ordering.

### 9. XOR tricks — the high-value identities

`A^A=0`, `A^0=A`, XOR is associative/commutative. Therefore:
```c
// find the unique element where every other value appears twice:
uint32_t unique(const uint32_t*a,int n){ uint32_t x=0; for(int i=0;i<n;i++) x^=a[i]; return x; }
// find the missing number in 0..n with one absent:
uint32_t missing(const uint32_t*a,int n){ uint32_t x=0;
    for(int i=0;i<n;i++) x^=a[i]; for(uint32_t i=0;i<=(uint32_t)n;i++) x^=i; return x; }
```
XOR-all cancels every pair, leaving the singleton; XOR the array against `0..n` cancels all present values, leaving the missing one — **O(n) time, O(1) space, no overflow** (unlike the sum trick). The classic **swap-without-temp** (`a^=b; b^=a; a^=b;`) is a fun identity but *don't* ship it (fails when the two are the same object; not faster than a temp).

### 10. Sign extension + the widen-before-shift trap

```c
// sign-extend a w-bit field (bit w-1 is the sign):
int32_t sext(uint32_t v,int w){ int32_t m=1<<(w-1); return (int32_t)((v ^ m) - m); }
// combine two 32-bit register reads into a 64-bit value (the QC trap):
uint64_t tick(uint32_t hi,uint32_t lo){ return ((uint64_t)hi << 32) | lo; }  // WIDEN FIRST
```
**Arithmetic** right shift (`>>` on signed) replicates the sign bit; **logical** shift (`>>` on unsigned) fills zero — know which your type gives. The recurring hardware trap ([Day 3](day03_bits_mmio_volatile_polling.md), QC post-Si): `hi << 32` where `hi` is 32-bit is **undefined / zero** — **cast to 64-bit before shifting**.

### 11. Saturating arithmetic — clamp, don't wrap

```c
uint8_t sat_add8(uint8_t a,uint8_t b){ uint16_t s=a+b; return s>0xFF?0xFF:(uint8_t)s; }
```
Counters and DSP/switch statistics often **saturate** (stick at max) instead of wrapping to 0 — a wrapped packet/error counter lies. Know the clamp form and when wrap-vs-saturate matters (a free-running timer wraps; an error counter usually saturates or has W1C semantics).

### The reflex table (one-liners to know cold)

| Goal | Expression |
|---|---|
| is power of 2 | `x && !(x & (x-1))` |
| clear lowest set bit | `x & (x-1)` |
| isolate lowest set bit | `x & -x` |
| floor log2 | `31 - __builtin_clz(x)` |
| floor pow2 | smear then `x - (x>>1)` |
| ceil/next pow2 | `x--`, smear, `+1` |
| popcount | `x &= x-1` per set bit (Kernighan) |
| width-w mask | `(1u<<w)-1` (guard `w==32`) |
| set/clear/toggle/test | `\|(1u<<b)` / `&~(1u<<b)` / `^(1u<<b)` / `&(1u<<b)` |
| combine 32+32→64 | `((uint64_t)hi<<32)\|lo` |

---

## Bugs in this Topic

| Bug | Symptom | Root cause | Fix |
|---|---|---|---|
| **`1<<31` signed** | UB / wrong sign | shifted a signed `1` | use `1U` / `1UL` |
| **shift ≥ width** | UB, garbage | `1u<<32`, `>>32` | guard / special-case width |
| **`x&(x-1)` on 0** | underflow in unsigned | forgot `x!=0` | check `x` first (isPow2 does) |
| **floor_pow2(0)** | wrong / 0 | undefined for 0 | document/guard `x>0` |
| **missing field clear** | stale bits OR'd in | RMW without `& ~mask` | clear then set |
| **W1C RMW** | clears live status bits | read-modify-write a W1C reg | write only the target bit |
| **`hi<<32` not widened** | high half lost | 32-bit shift by 32 | cast to 64-bit first |
| **assumed arithmetic `>>`** | sign fill where zero wanted | signedness of operand | pick `int`/`unsigned` deliberately |

---

## Interview Explanation (60–90 sec)

> "Most bit questions reduce to three identities: `x & (x-1)` clears the lowest set bit, `x & -x` isolates it, and smearing the top bit down with a cascade of shift-ORs lets me build power-of-two operations. Power of two is one set bit, so `x && !(x & (x-1))`. For floor power of two — the largest power not exceeding x — I smear the high bit down to get a run of ones and subtract the right-shift, leaving just the top bit; or on real hardware it's one-shifted-left by thirty-one minus count-leading-zeros, since clz gives me floor-log-two directly. Next power of two is decrement, smear, increment. Popcount I do with Brian Kernighan's `x &= x-1` once per set bit, or a branch-free SWAR adder tree. Find-first-set is count-trailing-zeros, which in hardware is literally a priority encoder. For registers it's mask-and-shift with a read-modify-write that clears the field before OR-ing the new value, and I'm careful with the traps: `1` must be unsigned before shifting by thirty-one, shifting by the full width is undefined so I guard a width-32 mask, and to combine two 32-bit register reads into a 64-bit value I cast to 64-bit before shifting. And XOR-ing a whole array cancels every pair, so it finds a unique or missing element in linear time with no extra space and no overflow."

---

## Architect Drills (derive, no answer key)

1. Implement `floor_pow2` two ways (smear, and via `clz`) and `ceil_pow2`. Handle `x==0` and `x==1`. Trace `floor_pow2(100)`.
2. Why is find-first-set the same primitive as a hardware priority encoder ([Day 5](day05_boolean_combinational_logic.md))? Where in an OS/allocator is it used?
3. Write `popcount` three ways and rank by cycles for dense vs sparse inputs.
4. Combine `CTR_HI` and `CTR_LO` 32-bit reads into a 64-bit counter; show the bug if you don't widen first (the QC trap).
5. Reverse the bits of a 32-bit word in `log2(32)` steps; explain each mask.
6. Find the missing number in `0..n` and the single unique value among pairs — both with XOR. Why is XOR safer than the sum method?
7. Sign-extend a 12-bit signed field to 32 bits without a branch. When does arithmetic vs logical shift bite you?

---

## Coding A — `d04_bitops`

Build a **bit-ops library in C** with: `is_pow2`, `floor_pow2`, `ceil_pow2`, `popcount` (Kernighan + SWAR), `ffs`/`clz`/`ctz` wrappers, `get_field`/`put_field`, `rol`/`ror` (width-guarded), `bswap16/32/64`, `reverse32`, `unique`/`missing` (XOR), `sext`, and `sat_add8` — each validated against a brute-force oracle, with explicit tests for the UB traps (`1U` shift, width-32 mask, widen-before-shift). Full spec in [coding/EXERCISES.md](../coding/EXERCISES.md#day-4--bit-manipulation-mastery).

**Interview prompts:** Floor vs ceil pow2 — what's the shared move? Why widen before `<<32`? Why is `x&(x-1)` popcount faster on sparse inputs?

---

## Bug Anchor / cross-links

- **Consolidates:** [Day 3](day03_bits_mmio_volatile_polling.md) (MMIO masks, field RMW, W1C, `1U<<31`, widen-before-shift).
- **Hardware duals:** [Day 5](day05_boolean_combinational_logic.md) (find-first-set = priority encoder; popcount = adder tree), [Day 15](day15_memory_systems_ecc.md) (parity/ECC = XOR; Hamming distance = popcount of XOR), [Day 7](day07_sequential_logic.md) (Gray ↔ binary uses XOR-scan).
- **Interview traps shared with:** QC post-Si (32+32→64), Cerebras/Amazon (edge-case discipline).

---

## Exit Check

- [ ] Write `is_pow2`, `floor_pow2`, `ceil_pow2`, `next_pow2` from memory, handling `0`/`1`.
- [ ] State the three golden identities and what each does.
- [ ] Implement popcount via Kernighan and SWAR; explain when each wins.
- [ ] Relate `ffs`/`clz`/`ctz` to `log2` and to a priority encoder.
- [ ] Do a field extract/insert RMW and name the two UB traps (`1U` shift, width-32).
- [ ] Combine two 32-bit reads into 64 bits correctly (widen first).
- [ ] Find unique/missing via XOR and explain why it beats the sum method.
