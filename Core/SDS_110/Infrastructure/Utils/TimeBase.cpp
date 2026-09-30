/*
 * TimeBase.cpp  (Infrastructure/Utils)
 */
#include "TimeBase.hpp"
#include "DWT.hpp"

namespace sds110 {

uint64_t TimeBase::nowUs()
{
    (void)DWTTimer::instance();                 // CYCCNT einschalten (einmalig, setzt ihn dabei auf 0)
    static CycleExtender ext(SystemCoreClock / 1000U);
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();
    const uint64_t cyc = ext.update(DWT->CYCCNT, HAL_GetTick());
    __set_PRIMASK(primask);
    return cyc / (SystemCoreClock / 1000000U);
}

} // namespace sds110
