**pmpc** (Phil's Module Player (written in C)) is a basic polyphonic module player for AVR microcontrollers. it also comes with a script which translates a pmpc score (a "score compiler") to its binary representation in order to embed a song into a chip.

current features are:
  * 8-bit square/triangle/sawtooth waveforms
  * multiple tracks
  * 22.05 kHz with 4 tracks @ 16 MHz

to do:
  * PCM samples (for drums)
  * noise synthesis (should work for drums too)
  * jump/loop instructions in tracks
  * speed instructions in tracks
  * volume control instructions in tracks
  * noise shaping for quantization error


## pmpc score ##

a pmpc score looks like this (forget automagic colors):

```
# zelda's theme.
# author:	Philippe Proulx <eepp.ca>
# date:		2012/02/05

> samps_per_tick: 240
> generators: saw, sq, sq
> ticks_per_whole: 192
> cut_ticks: 2

# part 1
a0=8 /=8 a0=4,3+ a0=4,3+ g0=4,3+ a0=8 /=8 a0=8 /=8
a3=4     e3=4                    /=8  a3=8+ a3=16 b3=16 C4=16 d4=16
C3=8 /=8 C3=4,3+ C3=4,3+ b2=4,3+ C3=8+  C3=8+ C3=16 d3=16 e3=16 F3=16

g0=8 /=8 g0=4,3+ g0=4,3+ f0=4,3+ g0=8 /=8 g0=8 /=8
e4=2 /=2,3 e4=8,3+ e4=8,3+ e4=4,3 f4=4,3 g4=4,3
g3=4 a3=16 b3=16 C4=16 d4=16 e4=4 g3=4,3 a3=4,3 b3=4,3

f0=8 /=8 f0=4,3+ f0=4,3+ D0=4,3+ f0=8 /=8 f0=8 /=8
a4=2 a4=4,3 a4=4,3+ a4=4,3+ a4=4,3 g4=4,3 f4=4,3 
c4=4 f3=16 g3=16 a3=16 b3=16 c4=4,3 /=4,3 c4=4,3+ c4=4,3 b3=4,3 a3=4,3

c1=8 /=8 c1=4,3+ c1=4,3+ b0=4,3+ c1=8 /=8 c1=8 /=8
g4=4,3+ g4=4,3 f4=4,3 e4=2+ e4=4
c4=4,3 c4=4,3 g3=4,3+  g3=4,3+ g3=4,3 f3=4,3   g3=4,3+ /=4,3 g3=4,3+  g3=4,3 f3=4,3 g3=4,3

A0=8 /=8 A0=4,3+ A0=4,3+ a0=4,3+ A0=8 /=8 A0=8 /=8
d4=8+ d4=16 e4=16 f4=2+ e4=8 d4=8
f3=8+ f3=16 e3=16 f3=8+ f3=16 g3=16 a3=4 g3=8 f3=8

a0=8 /=8 a0=4,3+ a0=4,3+ g0=4,3+ a0=8 /=8 a0=8 /=8
c4=8+ c4=16 d4=16 e4=2 /=4
e3=8+ e3=16 d3=16 e3=8+ e3=16 f3=16 g3=4 f3=8 e3=8

b0=8 /=8 b0=4,3+ b0=4,3+ a0=4,3+ b0=8 /=8 b0=8 /=8
b3=8+ b3=16 C4=16 D4=2 F4=4
D3=4+   D3=8 e3=8  F3=8+ F3=16 G3=16  a3=16 /=16 b3=16 /=16

e1=4 d1=4 c1=4 b0=4
e4=8 e3=16+ e3=16+ e3=16+ /=16  e3=16+ e3=16+ e3=16+ /=16  e3=16+ e3=16+ e3=16+ /=16 e3=8
G3=8 G2=16+ G2=16+ G2=16+ /=16  G2=16+ G2=16+ G2=16+ /=16  G2=16+ G2=16+ G2=16+ /=16 G2=8
```

and is converted to this with a single call to the translator script:

```

#define TRACKS_SZ	3
#define SAMPS_PER_TICK	240
#define VOL_DIV_EXP	2

static prog_uint8_t g_track_data0 [] = {
	9, 24, 152, 9, 14, 130, 9, 14, 130, 7, 14, 130, 9, 24, 152, 9, 
	24, 152, 7, 24, 152, 7, 14, 130, 7, 14, 130, 5, 14, 130, 7, 24, 
	152, 7, 24, 152, 5, 24, 152, 5, 14, 130, 5, 14, 130, 3, 14, 130, 
	5, 24, 152, 5, 24, 152, 12, 24, 152, 12, 14, 130, 12, 14, 130, 11, 
	14, 130, 12, 24, 152, 12, 24, 152, 10, 24, 152, 10, 14, 130, 10, 14, 
	130, 9, 14, 130, 10, 24, 152, 10, 24, 152, 9, 24, 152, 9, 14, 130, 
	9, 14, 130, 7, 14, 130, 9, 24, 152, 9, 24, 152, 11, 24, 152, 11, 
	14, 130, 11, 14, 130, 9, 14, 130, 11, 24, 152, 11, 24, 152, 16, 48, 
	14, 48, 12, 48, 11, 48
};
static prog_uint8_t g_track_data1 [] = {
	45, 48, 40, 48, 152, 45, 22, 130, 45, 12, 47, 12, 49, 12, 50, 12, 
	52, 96, 160, 52, 6, 130, 52, 6, 130, 52, 16, 53, 16, 55, 16, 57, 
	96, 57, 16, 57, 14, 130, 57, 14, 130, 57, 16, 55, 16, 53, 16, 55, 
	14, 130, 55, 16, 53, 16, 52, 94, 130, 52, 48, 50, 22, 130, 50, 12, 
	52, 12, 53, 94, 130, 52, 24, 50, 24, 48, 22, 130, 48, 12, 50, 12, 
	52, 96, 176, 47, 22, 130, 47, 12, 49, 12, 51, 96, 54, 48, 52, 24, 
	40, 10, 130, 40, 10, 130, 40, 10, 130, 140, 40, 10, 130, 40, 10, 130, 
	40, 10, 130, 140, 40, 10, 130, 40, 10, 130, 40, 10, 130, 140, 40, 24
};
static prog_uint8_t g_track_data2 [] = {
	37, 24, 152, 37, 14, 130, 37, 14, 130, 35, 14, 130, 37, 22, 130, 37, 
	22, 130, 37, 12, 38, 12, 40, 12, 42, 12, 43, 48, 45, 12, 47, 12, 
	49, 12, 50, 12, 52, 48, 43, 16, 45, 16, 47, 16, 48, 48, 41, 12, 
	43, 12, 45, 12, 47, 12, 48, 16, 144, 48, 14, 130, 48, 16, 47, 16, 
	45, 16, 48, 16, 48, 16, 43, 14, 130, 43, 14, 130, 43, 16, 41, 16, 
	43, 14, 130, 144, 43, 14, 130, 43, 16, 41, 16, 43, 16, 41, 22, 130, 
	41, 12, 40, 12, 41, 22, 130, 41, 12, 43, 12, 45, 48, 43, 24, 41, 
	24, 40, 22, 130, 40, 12, 38, 12, 40, 22, 130, 40, 12, 41, 12, 43, 
	48, 41, 24, 40, 24, 39, 46, 130, 39, 24, 40, 24, 42, 22, 130, 42, 
	12, 44, 12, 45, 12, 140, 47, 12, 140, 44, 24, 32, 10, 130, 32, 10, 
	130, 32, 10, 130, 140, 32, 10, 130, 32, 10, 130, 32, 10, 130, 140, 32, 
	10, 130, 32, 10, 130, 32, 10, 130, 140, 32, 24
};
static uint8_t* g_tracks_datas [] = {
	g_track_data0, g_track_data1, g_track_data2
};
static uint16_t g_tracks_sizes [] = {
	134, 128, 187
};
static uint8_t (*g_tracks_generators [])(struct track*) = {
	gen_saw, gen_sq, gen_sq
};
```