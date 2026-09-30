/*
 * PrintfDriver.h
 *
 *  Created on: Aug 13, 2026
 *      Author: 310004
 */

#pragma once

void PrintfDriver() {

  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  ITM->LAR = 0xC5ACCE55;  // unlock
  ITM->TCR = ITM_TCR_ITMENA_Msk | ITM_TCR_TSENA_Msk | ITM_TCR_SWOENA_Msk;
  ITM->TER = 1;           // enable stimulus port 0

}
