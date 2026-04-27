#include<stdio.h>
#include<stdint.h>

#define BIT(n) (1U<<(n))
#define MASK(msb,lsb) (((1U<<(msb+lsb-1))-1) << (lsb))
#define FIELD_GET(r,msb,lsb) (((r) & MASK(msb,lsb)) >> (lsb))
#define FIELD_PREP(val,msb,lsb) (((val)<<lsb) & (MASK(msb,lsb)))

typedef struct CTRL{
    volatile uint32_t EN;
    volatile uint32_t STATUS;
    volatile uint32_t READY;
}CTRL;

static CTRL fake_hw = {0};
#define CTRL_EN BIT(0)
#define CTRSL_STATUS BIT(2)
#define CTRL_READY BIT(0)

#define CTRL_REG (&fake_hw)

static inline uint32_t reg_read32(volatile uint32_t* reg){
    return *reg;
}

static inline void reg_write32(volatile uint32_t* reg, int val){
    *reg = val;
}

static inline void reg_rmw32(volatile uint32_t *reg, uint32_t msb,uint32_t lsb, int val){
    uint32_t tmp = *reg & ~(MASK(msb,lsb));
    tmp |= FIELD_PREP(val,msb,lsb);
    *reg = tmp;    
}

static inline uint8_t poll_ready(volatile uint32_t *reg,uint32_t mask, uint32_t timeout_us){
    while(timeout_us--){
        if(reg_read32(reg)&mask)
            return 1;
    }
    return 0;
}

int main(){
    CTRL *ctrl = CTRL_REG;
    
    reg_write32(&ctrl->EN, CTRL_EN);
    if(poll_ready(&ctrl->READY, CTRL_READY, 10000)){
        printf("Device Ready");
    } else {
        printf("Timed out waiting for Device");
    }
    return 0;
}