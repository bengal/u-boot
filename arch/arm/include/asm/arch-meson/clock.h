/*
 * Copyright 2018 - Beniamino Galvani <b.galvani@gmail.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#ifndef _MESON_CLOCK_H_
#define _MESON_CLOCK_H_

/* Clock gates */
#define HHI_GCLK_MPEG0		0x140
#define HHI_GCLK_MPEG1		0x144
#define HHI_GCLK_MPEG2		0x148
#define HHI_GCLK_OTHER		0x150
#define HHI_GCLK_AO		0x154

#define HHI_MPEG_CLK_CNTL	0x174

#define HHI_MPLL_CNTL		0x280
#define HHI_MPLL_CNTL2		0x284
#define HHI_MPLL_CNTL3		0x288
#define HHI_MPLL_CNTL4		0x28C
#define HHI_MPLL_CNTL5		0x290
#define HHI_MPLL_CNTL6		0x294
#define HHI_MPLL_CNTL7		0x298
#define HHI_MPLL_CNTL8		0x29C
#define HHI_MPLL_CNTL9		0x2A0
#define HHI_MPLL_CNTL10		0x2A4

#define HHI_SYS_PLL_CNTL	0x300
#define HHI_SYS_PLL_CNTL2	0x304
#define HHI_SYS_PLL_CNTL3	0x308
#define HHI_SYS_PLL_CNTL4	0x30c
#define HHI_SYS_PLL_CNTL5	0x310

ulong meson_measure_clk_rate(unsigned int clk);

#endif
