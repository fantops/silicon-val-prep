#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#define BIT(n)              (UINT32_C(1) << (n))
#define MASK(msb, lsb)      ((UINT32_MAX >> (31 - ((msb) - (lsb)))) << (lsb))
#define FIELD_GET(reg, msb, lsb)   (((reg) & MASK(msb, lsb)) >> (lsb))

struct csr_block {
    volatile uint32_t CTRL;
    volatile uint32_t STATUS;
    volatile uint32_t DATA_LO;
    volatile uint32_t DATA_HI;
};

#define CTRL_EN             BIT(0)
#define STATUS_READY        BIT(0)
#define STATUS_ERROR        BIT(1)

#define CTRL_RESERVED_BIT   BIT(8)

int csr_init(struct csr_block *csr);
int pm_step(int state, int event);

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

static void test_csr_init_success(void)
{
    struct csr_block csr = {
        .CTRL = CTRL_RESERVED_BIT,
        .STATUS = STATUS_READY,
    };

    assert(csr_init(&csr) == 0);
    assert((csr.CTRL & CTRL_EN) == CTRL_EN);
    assert(FIELD_GET(csr.CTRL, 3, 1) == 2U);
    assert((csr.CTRL & CTRL_RESERVED_BIT) == CTRL_RESERVED_BIT);
}

static void test_csr_init_timeout(void)
{
    struct csr_block csr = {
        .CTRL = 0,
        .STATUS = STATUS_ERROR,
    };

    assert(csr_init(&csr) == -1);
    assert((csr.CTRL & CTRL_EN) == CTRL_EN);
    assert(FIELD_GET(csr.CTRL, 3, 1) == 2U);
    assert((csr.STATUS & STATUS_READY) == 0);
}

static void test_pm_step(void)
{
    assert(pm_step(PM_IDLE, EV_DONE) == PM_IDLE);
    assert(pm_step(PM_IDLE, EV_REQ) == PM_REQ);
    assert(pm_step(PM_REQ, EV_DONE) == PM_REQ);
    assert(pm_step(PM_REQ, EV_ACK) == PM_ACK);
    assert(pm_step(PM_ACK, EV_REQ) == PM_ACK);
    assert(pm_step(PM_ACK, EV_DONE) == PM_IDLE);
    assert(pm_step(PM_REQ, EV_TIMEOUT) == PM_IDLE);
    assert(pm_step(99, EV_REQ) == PM_IDLE);
}

int main(void)
{
    test_csr_init_success();
    test_csr_init_timeout();
    test_pm_step();

    puts("bench: all assertions passed");
    return 0;
}
