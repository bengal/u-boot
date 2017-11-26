/*
 * (C) Copyright 2018 Beniamino Galvani <b.galvani@gmail.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 *
 * Clock rate measuring.
 */

#include <common.h>
#include <asm/arch/clock.h>
#include <asm/io.h>

/**
 * meson_measure_clk_rate() - Measure current clock rate.
 * @clk: The clock index
 * @return the clock rate in Hz, or -ve error code;
 */
ulong meson_measure_clk_rate(unsigned int clk)
{
	ulong start;
	ulong mhz;

	writel(0, MSR_CLK_REG0);

	clrsetbits_le32(MSR_CLK_REG0, MSR_CLK_REG0_DURATION_M, 64 - 1);
	clrbits_le32(MSR_CLK_REG0, MSR_CLK_REG0_CONT | MSR_CLK_REG0_EN_INT);
	clrsetbits_le32(MSR_CLK_REG0,
			MSR_CLK_REG0_CLOCK_ID_M,
			clk << MSR_CLK_REG0_CLOCK_ID_S);
	setbits_le32(MSR_CLK_REG0,
		     MSR_CLK_REG0_MEASUR_EN | MSR_CLK_REG0_CLOCK_EN);

	start = get_timer(0);
	while (readl(MSR_CLK_REG0) & MSR_CLK_REG0_BUSY) {
		if (get_timer(start) > 1) {
			debug("could not measure clk %u rate\n", clk);
			return -ETIMEDOUT;
		}
	}

	clrbits_le32(MSR_CLK_REG0, MSR_CLK_REG0_MEASUR_EN);

	mhz = ((readl(MSR_CLK_REG2) + 31) & 0xfffff) >> 6;
	return mhz * 1000000;
}
