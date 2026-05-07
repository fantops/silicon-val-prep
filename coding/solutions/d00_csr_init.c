#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

#define BIT(n) (UINT32_C(1) << (n))
#define MASK(msb,lsb) ((UINT32_MAX >> (31 - ((msb) - (lsb)))) << (lsb))
#define FIELD_GET(r,msb,lsb) (((r) & MASK(msb,lsb)) >> (lsb))
#define FIELD_PREP(val,msb,lsb) (((val) << (lsb)) & MASK(msb,lsb))


struct csr_block{
    volatile uint32_t CTRL;
    volatile uint32_t STATUS;
    volatile uint32_t DATA_LO;
    volatile uint32_t DATA_HI;
};

#define CTRL_EN BIT(0)
#define CTRL_MODE MASK(3,1)
#define STATUS_READY BIT(0)
#define STATUS_ERROR BIT(1)


static inline void reg_field_write32(volatile uint32_t *reg, uint32_t mask, uint32_t val){
    uint32_t tmp = *reg;
    tmp = tmp & (~mask);
    tmp = tmp | (val & mask);
    *reg = tmp;
}

static inline int wait_for_bit_set(volatile uint32_t *reg, uint32_t mask, int timeout_us){
    while(timeout_us-- > 0){
        if (*reg & mask) return 0;
        usleep(1); 
        if (timeout_us == 7){
            reg_field_write32(reg, CTRL_EN, 1);
        }
    }
    return -1;
}

int csr_init(struct csr_block *r){
    reg_field_write32(&r->CTRL, CTRL_EN, 0);
    reg_field_write32(&r->CTRL, CTRL_MODE, FIELD_PREP(2,3,1));

    while(wait_for_bit_set(&r->STATUS, STATUS_READY, 10) != 0){
        printf("Error Timed out\n");
        return -1;
    }
    return 0;
}

enum pm_state {PM_IDLE, PM_REQ, PM_ACK};
enum pm_event {EV_REQ, EV_DONE, EV_ACK, EV_TIMEOUT};

int pm_step(int state, int event)
{
    switch (state) {
    case PM_IDLE:   return (event == EV_REQ)     ? PM_REQ  : PM_IDLE;
    case PM_REQ:    return (event == EV_ACK)      ? PM_ACK  :
                           (event == EV_TIMEOUT)  ? PM_IDLE : PM_REQ;
    case PM_ACK:    return (event == EV_DONE)     ? PM_IDLE : PM_ACK;
    default:        return PM_IDLE;
    }
}
