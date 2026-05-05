#include <stdint.h>

#define BIT(n)              (UINT32_C(1) << (n))
#define MASK(msb, lsb)      ((UINT32_MAX >> (31 - ((msb) - (lsb)))) << (lsb))
#define FIELD_GET(reg, msb, lsb)   (((reg) & MASK(msb, lsb)) >> (lsb))
#define FIELD_PREP(val, msb, lsb)  (((val) << (lsb)) & MASK(msb, lsb))

struct csr_block {
    volatile uint32_t CTRL;
    volatile uint32_t STATUS;
    volatile uint32_t DATA_LO;
    volatile uint32_t DATA_HI;
};

#define CTRL_EN             BIT(0)
#define CTRL_MODE_MASK      MASK(3, 1)
#define CTRL_MODE_INIT      FIELD_PREP(2U, 3, 1)

#define STATUS_READY        BIT(0)
#define STATUS_ERROR        BIT(1)

static inline uint32_t reg_read32(volatile uint32_t *reg)
{
    return *reg;
}

static inline void reg_write32(volatile uint32_t *reg, uint32_t val)
{
    *reg = val;
}

static inline void reg_field_write32(volatile uint32_t *reg,
                                     uint32_t mask,
                                     uint32_t prepared_val)
{
    uint32_t cur = reg_read32(reg);

    cur &= ~mask;
    cur |= prepared_val & mask;
    reg_write32(reg, cur);
}

static int wait_for_bit_set(volatile uint32_t *reg, uint32_t mask, int timeout_us)
{
    while (timeout_us-- > 0) {
        if ((reg_read32(reg) & mask) == mask) {
            return 0;
        }
    }

    return -1;
}

int csr_init(struct csr_block *csr)
{
    reg_field_write32(&csr->CTRL, CTRL_EN, CTRL_EN);
    reg_field_write32(&csr->CTRL, CTRL_MODE_MASK, CTRL_MODE_INIT);

    return wait_for_bit_set(&csr->STATUS, STATUS_READY, 10);
}

enum pm_state {
    PM_IDLE,
    PM_REQ,
    PM_ACK,
};

enum pm_event {
    EV_REQ,
    EV_DONE,
    EV_ACK,
    EV_TIMEOUT,
};

int pm_step(int state, int event)
{
    switch (state) {
    case PM_IDLE:
        return event == EV_REQ ? PM_REQ : PM_IDLE;
    case PM_REQ:
        if (event == EV_ACK) {
            return PM_ACK;
        }
        return event == EV_TIMEOUT ? PM_IDLE : PM_REQ;
    case PM_ACK:
        return event == EV_DONE ? PM_IDLE : PM_ACK;
    default:
        return PM_IDLE;
    }
}
