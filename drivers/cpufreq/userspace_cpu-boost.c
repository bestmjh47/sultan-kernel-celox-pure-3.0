/*
 * Copyright (c) 2014, Sultanxda <sultanxda@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

/* CPU boost freq in KHz. All online CPUs
 * are boosted. */
static int boost_freq_khz = 1134000;
module_param(boost_freq_khz, int, 0644);

/* Duration to boost CPUs in millisecs. */
static int boost_duration_ms = 3000;
module_param(boost_duration_ms, int, 0644);

/* File for userspace to write to in
 * order to request boost. */
static int boost_now = 0;
module_param(boost_now, int, 0644);

/* Time in ms to poll for boost request from
 * userspace. */
#define BOOST_CHECK_MS 20

/* CPU's default minfreq in KHz. */
#define DEFAULT_MINFREQ_KHZ 192000

static struct delayed_work check_input_boost;

static void input_boost_main(struct work_struct *work)
{
	struct cpufreq_policy *cpu_policy = NULL;
	static int cpu_boosted = 0;
	unsigned int minfreq = DEFAULT_MINFREQ_KHZ;
	unsigned int wait_ms = BOOST_CHECK_MS;
	int cpu = 0;

	if (cpu_boosted) {
		/* restore original minfreq */
		get_online_cpus();
		for_each_online_cpu(cpu) {
			cpu_policy = cpufreq_cpu_get(cpu);
			cpufreq_verify_within_limits(cpu_policy, minfreq, UINT_MAX);
			cpu_policy->user_policy.min = minfreq;
			cpufreq_update_policy(cpu);
			cpufreq_cpu_put(cpu_policy);
		}
		put_online_cpus();
		cpu_boosted = 0;
		wait_ms = 0;
		boost_now = 0;
		goto end;
	}

	if (boost_now) {
		minfreq =  boost_freq_khz;

		/* boost online CPUs */
		get_online_cpus();
		for_each_online_cpu(cpu) {
			cpu_policy = cpufreq_cpu_get(cpu);
			cpufreq_verify_within_limits(cpu_policy, minfreq, UINT_MAX);
			cpu_policy->user_policy.min = minfreq;
			cpufreq_update_policy(cpu);
			cpufreq_cpu_put(cpu_policy);
		}
		put_online_cpus();
		cpu_boosted = 1;
		wait_ms = boost_duration_ms;
	}

end:
	schedule_delayed_work(&check_input_boost,
				msecs_to_jiffies(wait_ms));
}

static int userspace_boost_init(void)
{
	INIT_DELAYED_WORK(&check_input_boost, input_boost_main);
	schedule_delayed_work(&check_input_boost, 0);

	return 0;
}
late_initcall(userspace_boost_init);

MODULE_AUTHOR("Sultanxda <sultanxda@gmail.com>");
MODULE_DESCRIPTION("Userspace CPU-Boost Driver");
MODULE_LICENSE("GPLv2");
