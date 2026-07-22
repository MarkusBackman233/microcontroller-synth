/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/drivers/gpio.h>
#include <string.h>
#include <math.h>
#include "osc.h"


#define I2S_TX_NODE  DT_NODELABEL(i2s0)

#define SAMPLE_FREQUENCY    48000
#define SAMPLE_BIT_WIDTH    16
#define BYTES_PER_SAMPLE    sizeof(int16_t)
#define NUMBER_OF_CHANNELS  2
#define SAMPLES_PER_BLOCK   (512 * NUMBER_OF_CHANNELS)
#define INITIAL_BLOCKS      2
#define TIMEOUT             4000

#define BLOCK_SIZE  (BYTES_PER_SAMPLE * SAMPLES_PER_BLOCK)
#define BLOCK_COUNT 4
K_MEM_SLAB_DEFINE_STATIC(mem_slab, BLOCK_SIZE, BLOCK_COUNT, 4);
#define BLOCK_DURATION_MS   ((SAMPLES_PER_BLOCK / NUMBER_OF_CHANNELS) * 1000 / SAMPLE_FREQUENCY)
#define BLOCK_BUDGET_US     (BLOCK_DURATION_MS * 1000)


/* ---- Onboard buttons -> notes -----------------------------------------
 * nRF5340 DK exposes button0..button3 as devicetree aliases sw0..sw3.
 * Each button drives its own oscillator voice so all four can sound
 * (and be released) independently and simultaneously.
 */
#define NUM_BUTTONS 4

static const struct gpio_dt_spec buttons[NUM_BUTTONS] = {
	GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios),
	GPIO_DT_SPEC_GET(DT_ALIAS(sw1), gpios),
	GPIO_DT_SPEC_GET(DT_ALIAS(sw2), gpios),
	GPIO_DT_SPEC_GET(DT_ALIAS(sw3), gpios),
};

static struct gpio_callback button_cb_data[NUM_BUTTONS];

/* One oscillator voice per button */
static Oscillator osc[NUM_BUTTONS];

/* Notes triggered by sw0..sw3 (A minor 7th-ish spread, adjust to taste) */
static const float note_freq[NUM_BUTTONS] = {
	440.00f / 4.0f, /* A4 - sw0 */
	523.25f / 4.0f, /* C5 - sw1 */
	659.25f / 4.0f, /* E5 - sw2 */
	783.99f / 4.0f, /* G5 - sw3 */
};

static const char note_name[NUM_BUTTONS] = { 'a', 'c', 'e', 'g' };

static void button_pressed(const struct device *dev, struct gpio_callback *cb,
			    uint32_t pins)
{
	for (int i = 0; i < NUM_BUTTONS; i++) {
		if (cb != &button_cb_data[i]) {
			continue;
		}

		/* Buttons on this board are active-low; gpio_pin_get_dt()
		 * already accounts for GPIO_ACTIVE_LOW in the DT flags,
		 * so a return of 1 means "currently pressed".
		 */
		int pressed = gpio_pin_get_dt(&buttons[i]);

		if (pressed) {
			oscillator_note_on(&osc[i], note_freq[i], note_name[i]);
		} else {
			oscillator_note_off(&osc[i]);
		}
		break;
	}
}

static bool init_buttons(void)
{
	int ret;

	for (int i = 0; i < NUM_BUTTONS; i++) {
		if (!gpio_is_ready_dt(&buttons[i])) {
			printk("Button %d device not ready\n", i);
			return false;
		}

		ret = gpio_pin_configure_dt(&buttons[i], GPIO_INPUT);
		if (ret < 0) {
			printk("Failed to configure button %d: %d\n", i, ret);
			return false;
		}

		/* BOTH edges: press starts the note, release triggers the
		 * envelope's release/off phase.
		 */
		ret = gpio_pin_interrupt_configure_dt(&buttons[i], GPIO_INT_EDGE_BOTH);
		if (ret < 0) {
			printk("Failed to configure button %d interrupt: %d\n", i, ret);
			return false;
		}

		gpio_init_callback(&button_cb_data[i], button_pressed, BIT(buttons[i].pin));
		gpio_add_callback(buttons[i].port, &button_cb_data[i]);
	}

	printk("Press button 1-4 to play a note (A4, C5, E5, G5)\n");
	return true;
}

static inline int16_t float_to_i16(float sample)
{
	return (int16_t)(sample * 32767.0f);
}

/* Mixes all active oscillator voices into the output buffer each block */
static void fill_audio(int16_t *buf, uint32_t frames)
{
	for (uint32_t i = 0; i < frames; i++) {

		buf[2 * i] = 0;
    	buf[2 * i + 1] = 0;

		for (size_t v = 0; v < NUM_BUTTONS; v++) {
			oscillator_next(&osc[v], &buf[2 * i], &buf[2 * i + 1]);
		}
	}
}

static bool prepare_transfer(const struct device *i2s_dev_tx)
{
	int ret;

	for (int i = 0; i < INITIAL_BLOCKS; ++i) {
		void *mem_block;

		ret = k_mem_slab_alloc(&mem_slab, &mem_block, K_NO_WAIT);
		if (ret < 0) {
			printk("Failed to allocate TX block %d: %d\n", i, ret);
			return false;
		}

		memset(mem_block, 0, BLOCK_SIZE);

		ret = i2s_write(i2s_dev_tx, mem_block, BLOCK_SIZE);
		if (ret < 0) {
			printk("Failed to write block %d: %d\n", i, ret);
			return false;
		}
	}

	return true;
}

int main(void)
{
	const struct device *const i2s_dev_tx = DEVICE_DT_GET(I2S_TX_NODE);
	struct i2s_config config;
	int ret;

	printk("I2S MAX98357A button-note sample\n");

	if (!init_buttons()) {
		return 0;
	}

	if (!device_is_ready(i2s_dev_tx)) {
		printk("%s is not ready\n", i2s_dev_tx->name);
		return 0;
	}

	config.word_size      = SAMPLE_BIT_WIDTH;
	config.channels       = NUMBER_OF_CHANNELS;
	config.format         = I2S_FMT_DATA_FORMAT_I2S | I2S_FMT_CLK_NF_NB;
	config.options        = I2S_OPT_BIT_CLK_CONT | I2S_OPT_BIT_CLK_CONTROLLER | I2S_OPT_FRAME_CLK_CONTROLLER;
	config.frame_clk_freq  = SAMPLE_FREQUENCY;
	config.mem_slab        = &mem_slab;
	config.block_size      = BLOCK_SIZE;
	config.timeout         = TIMEOUT;

	ret = i2s_configure(i2s_dev_tx, I2S_DIR_TX, &config);
	if (ret < 0) {
		printk("Failed to configure TX stream: %d\n", ret);
		return 0;
	}

	for (int i = 0; i < NUM_BUTTONS; i++) {
		osc[i].sample_rate_inv = 1.0f / SAMPLE_FREQUENCY;
		osc[i].amplitude       = 0.0f;
		osc[i].waveform        = OSC_SINE;
		osc[i].envelope_state  = ENV_Idle;
		osc[i].attack          = 1;
		osc[i].release         = 1;
	}
	oscillator_init_table();
	if (!prepare_transfer(i2s_dev_tx)) {
		return 0;
	}

	ret = i2s_trigger(i2s_dev_tx, I2S_DIR_TX, I2S_TRIGGER_START);
	if (ret < 0) {
		printk("Failed to start TX: %d\n", ret);
		return 0;
	}

	printk("I2S stream running, waiting for button presses\n");

	static uint32_t successful_writes = 0;

	while (1) {
		void *mem_block;

		ret = k_mem_slab_alloc(&mem_slab, &mem_block, K_MSEC(200));
		if (ret < 0) {
			printk("Failed to allocate block: %d\n", ret);
			continue;
		}
		uint32_t t0 = k_cycle_get_32();
		fill_audio((int16_t *)mem_block, SAMPLES_PER_BLOCK / NUMBER_OF_CHANNELS);
		uint32_t t1 = k_cycle_get_32();
		uint32_t elapsed_us = k_cyc_to_us_floor32(t1 - t0);

		/* Block budget: SAMPLES_PER_BLOCK/NUMBER_OF_CHANNELS frames at
		* SAMPLE_FREQUENCY = 100000us (100ms) for your current block size */
		printk("fill_audio: %u us (budget ~%u us) | budget: %f \n",
			elapsed_us,
			BLOCK_BUDGET_US,
			(float)elapsed_us / (float)BLOCK_BUDGET_US );

		ret = i2s_write(i2s_dev_tx, mem_block, BLOCK_SIZE);
		if (ret < 0) {
			printk("Failed to write data: %d (after %u successful writes)\n", ret, successful_writes);
			break;
		}
		successful_writes++;
	}
}