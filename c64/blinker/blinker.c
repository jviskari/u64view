#include <c64/memmap.h>
#include <c64/sid.h>
#include <c64/vic.h>
#include <c64/rasterirq.h>
#include <stdio.h>

#define frame_counter (*(byte *)0x0002)

__interrupt void raster_func(void)
{
	frame_counter--;
	if(frame_counter == 0)
	{
		sid.voices[0].ctrl = SID_CTRL_RECT | SID_CTRL_GATE;	
	    vic.color_border++;
		frame_counter = 50;			
    }
	else if (frame_counter == 40)
	{
		sid.voices[0].ctrl = 0;
	}
}

RIRQCode	raster_irq;

int main(void)
{
	// Map in the RAM underneath the BASIC rom
	mmap_set(MMAP_NO_BASIC);

	// Init the raster IRQ system to use the kernal iterrupt vector
	rirq_init_kernal();

	// Init the music interrupt on raster line 250
	rirq_build(&raster_irq, 1);
	rirq_call(&raster_irq, 0, raster_func);
	rirq_set(0, 250, &raster_irq);

	// Prepare the raster IRQ order
	rirq_sort();

	// start raster IRQ processing
	rirq_start();

	// Do something to show that the music plays without
	// main thread pumping
    frame_counter = 50;
	printf("AUDIO VIDEO SYNC TEST ");

	sid.voices[0].freq = NOTE_D(7);
	sid.voices[0].attdec = SID_ATK_2 | SID_DKY_6;
	sid.voices[0].susrel = SID_DKY_300 | 0xf0;
	sid.voices[0].pwm = 0x800;	
	sid.fmodevol = 15;

	for(;;)
	{
			
	}

	return 0;
}
