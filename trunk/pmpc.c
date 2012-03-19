/**
 * this is pmpc v1.0, a minimalist polyphonic music player for ATmega16. part
 * of the simple wave synthesis idea is based on work by François-Raymond Boyer
 * at École Polytechnique de Montréal.
 *
 * Copyright (c) 2012  Philippe Proulx <eepp.ca>
 *
 * This file is part of pmpc.
 *
 * pmpc is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * pmpc is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * pmpc. If not, see <http://www.gnu.org/licenses/>.
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <stdint.h>

/* audio info. */
#define SAMPLING_RATE	22050	/* increments LUT depends on this value */

/* track flags */
#define TFLAGS_PLAYING	1	/* track is currently being played/processed */
#define TFLAGS_LOOP	2	/* track loops */

/* special increments */
#define INCR_STOP	0

/* a track */
struct track {
	uint8_t* data;			/* track data (melody) */
	uint8_t (*gen)(struct track* t); /* wave generator callback */
	uint16_t data_pos;		/* position into track data */
	uint16_t data_sz;		/* track data size */
	uint16_t rem_samples;		/* remaining samples */
	uint16_t incr;			/* increment (specifies note/octave) */
	uint16_t wave_pos;		/* position in wave */
	uint16_t noise_word;		/* noise current value */
	uint16_t pcm_sz;		/* PCM data size */
	uint8_t* pcm_data;		/* when generator is PCM */
	uint8_t flags;			/* status */
	uint8_t note;			/* current note */
};

/* a couple of declarations */
/*static uint8_t gen_pcm(struct track* t);*/
static uint8_t gen_saw(struct track* t);
static uint8_t gen_tri(struct track* t);
static uint8_t gen_sq(struct track* t);
static uint8_t gen_noise32k(struct track* t);
static uint8_t gen_noise93(struct track* t);

/* output buffer (a single sample) */
static volatile uint8_t g_comp_next_sample = 0; /* should be mutexed */
static volatile uint8_t g_sample = 0x80;

/* increments for 16-bit period waves @ our sampling rate */
static uint16_t g_incr [] = {
	97, 103, 109, 116, 122, 130, 137, 146, 154, 163, 173, 183,
	194, 206, 218, 231, 245, 259, 275, 291, 309, 327, 346, 367,
	389, 412, 436, 462, 490, 519, 550, 582, 617, 654, 693, 734,
	778, 824, 873, 925, 980, 1038, 1100, 1165, 1234, 1308, 1385, 1468,
	1555, 1647, 1745, 1849, 1959, 2076, 2199, 2330, 2468, 2615, 2771, 2936,
	3110, 3295, 3491, 3699, 3918, 4151, 4398, 4660, 4937, 5230, 5542, 5871
};

/* tracks */
#include "song.pmpcb"
static struct track g_tracks [TRACKS_SZ];

/**
 * initializes all tracks.
 */
static void init_tracks(void) {
	uint8_t i;
	
	for (i = 0; i < TRACKS_SZ; ++i) {
		g_tracks[i].rem_samples = 0;
		g_tracks[i].incr = 0;
		g_tracks[i].flags = TFLAGS_PLAYING | TFLAGS_LOOP;
		g_tracks[i].data_pos = 0;
		g_tracks[i].data = g_tracks_datas[i];
		g_tracks[i].data_sz = g_tracks_sizes[i];
		g_tracks[i].gen = g_tracks_generators[i];
		g_tracks[i].note = 0;
		g_tracks[i].noise_word = 1;
	}
}

/**
 * outputs a sample to DAC.
 *
 * @param samp		8-bit sample value
 */
static __attribute__((always_inline)) void output(uint8_t samp) {
	/* use fast PWM as a DAC here; could be replaced by an external DAC
	and thus only the following line has to be changed for that. */
	OCR0 = samp;
}

/**
 * timer 1 output compare interrupt handler.
 */
ISR(TIMER1_COMPA_vect) {
	/* actual output */
	output(g_sample);
	
	/* ready to compute next sample */
	g_comp_next_sample = 1;
}

/**
 * configures hardware.
 */
static void config_hw(void) {
	/* just in case */
	cli();
	
	/* timer 0: fast PWM (our DAC) */
	TCNT0 = 0;
	TCCR0 |= _BV(WGM00) | _BV(WGM01) | _BV(COM01) | _BV(COM00) | _BV(CS00);
	DDRB |= _BV(PB3);
	OCR0 = 0;
	
	/* timer 1 @ sampling rate (CTC) for playback */
	TCCR1B |= _BV(WGM12) | _BV(CS10);;
	TCNT1 = 0;
	OCR1A = F_CPU / SAMPLING_RATE;
	TIMSK |= _BV(OCIE1A);
	
	/* start the machine */
	sei();
}

#if 0
/**
 * generates a PCM sample.
 *
 * @param t	track
 * @return	sample
 */
static uint8_t gen_pcm(struct track* t) {
	uint8_t s;
	
	if (t->wave_pos == t->pcm_sz) {
		return 0x80; /* no more data to play: silence */
	} else {
		s = t->pcm_data[t->wave_pos];
		++t->wave_pos;
		
		return s;
	}
}
#endif

/**
 * generates a forward sawtooth wave sample for given track.
 *
 * @param t	track
 * @return	sample
 */
static uint8_t gen_saw(struct track* t) {
	uint16_t w;
	uint8_t r;
	
	w = t->wave_pos;
	r = w >> 8;
	t->wave_pos += t->incr;
	
	return r;
}

/**
 * generates a triangular wave sample for given track.
 *
 * @param t	track
 * @return	sample
 */
static uint8_t gen_tri(struct track* t) {
	uint16_t w;
	uint8_t r;
	
	w = t->wave_pos;
	if (w < 0x8000) {
		r = (w * 2) >> 8;
	} else {
		r = (0x10000 - 2 * (w - 0x8000)) >> 8;
	}
	t->wave_pos += t->incr;
	
	return r;
}

/**
 * generates a square wave sample for given track.
 *
 * @param t	track
 * @return	sample
 */
static uint8_t gen_sq(struct track* t) {
	uint16_t w;
	uint8_t r = 0;
	
	w = t->wave_pos & 0x8000;
	
	/* following values are there to match the triangle/sawtooth RMS */
	if (w != 0) {
		r = 175;
	} else {
		r = 83;
	}
	t->wave_pos += t->incr;
	
	return r;
}

/**
 * generates a noise sample from a 93 b pool.
 *
 * @param t	track
 * @return	sample
 */
static uint8_t gen_noise93(struct track* t) {
	uint16_t nw = t->noise_word;
	uint8_t x;
	uint8_t bit;
	
	++t->wave_pos;
	if (t->wave_pos > t->note) {
		for (x = 0; x < 8; ++x) {
			bit = ((nw >> 14) ^ (nw >> 8)) & 1;
			nw = (nw << 1) | bit;
		}
		t->noise_word = nw;
		t->wave_pos = 0;
	}
	
	return nw & 0xff;
}

/**
 * generates a noise sample from a 32 kib pool.
 *
 * @param t	track
 * @return	sample
 */
static uint8_t gen_noise32k(struct track* t) {
	uint16_t nw = t->noise_word;
	uint8_t x;
	uint8_t bit;
	
	++t->wave_pos;
	if (t->wave_pos > t->note) {
		for (x = 0; x < 8; ++x) {
			bit = ((nw >> 14) ^ (nw >> 13)) & 1;
			nw = (nw << 1) | bit;
		}
		t->noise_word = nw;
		t->wave_pos = 0;
	}
	
	return nw & 0xff;
}

/**
 * rewinds a track.
 *
 * @param t	track
 */
static __attribute__((always_inline)) void rewind_track(struct track* t) {
	t->wave_pos = 0;
	t->data_pos = 0;
	t->rem_samples = 0;
}



/**
 * computes the next track sample.
 *
 * @param t	track
 * @return	computed sample
 */
static __attribute__((always_inline))
uint8_t track_next_sample(struct track* t) {
	uint8_t samp = 0x80;
	uint8_t note;
	uint16_t ticks;
	uint16_t tdp;
	
	if (t->flags & TFLAGS_PLAYING) {
		/* next note */
		if (t->rem_samples == 0) {
			tdp = t->data_pos;
			if (tdp < t->data_sz) {
				/* get next note */
				note = pgm_read_byte(&t->data[tdp]);
				++tdp;
				if (note & 0x80) {
					/* stop note */
					t->incr = INCR_STOP;
					ticks = (note & 0x7f);
					t->wave_pos = 0;
				} else {
					/* real note */
					t->incr = g_incr[note];
					t->note = note;
					ticks = pgm_read_byte(&t->data[tdp]);
					++tdp;
				}
				t->data_pos = tdp;
				t->rem_samples = ticks * SAMPS_PER_TICK;
			} else {
				/* track is finished: stop playing or loop */
				if (t->flags & TFLAGS_LOOP) {
					rewind_track(t);
					t->rem_samples = 1;
				} else {
					t->flags &= ~TFLAGS_PLAYING;
				}
			}
		}
		
		/* special increments here */
		switch (t->incr) {
			case INCR_STOP:
			break;
			
			default:
			/* call appropriate wave generator */
			samp = t->gen(t);
		}
		
		/* sample is played */
		--t->rem_samples;
	}

	return samp;
}

/**
 * computes the next sample.
 *
 * @return	computed sample
 */
static __attribute__((always_inline)) uint8_t compute_next_sample(void) {
	uint8_t x;
	uint8_t usamp;
	int16_t ssamp;
	int16_t acc = 0;
	
	for (x = 0; x < TRACKS_SZ; ++x) {
		usamp = track_next_sample(&g_tracks[x]);
		ssamp = (int16_t) (uint16_t) usamp;
		
		/* volume: do NOT divide by something else than a power of two;
		AVR has no division instruction and this is gonna be pain in
		the ass to emulate (read: slow has hell). just in case here, we
		right-shift to be sure, but a division by a power of two would
		be changed by a right-shift anyway by compiler optimization. */
		acc += (ssamp - 0x80) >> VOL_DIV_EXP;
	}
	
	return (uint8_t) (acc + 0x80);
}

int main(void) {
	/* initialize tracks */
	init_tracks();
	
	/* compute the first sample */
	g_sample = compute_next_sample();
	
	/* configure hardware */
	config_hw();
	
	/* main loop */
	for (;;) {
		if (g_comp_next_sample) {
			g_sample = compute_next_sample();
			g_comp_next_sample = 0;
		}
	}
	
	return 0;
}
