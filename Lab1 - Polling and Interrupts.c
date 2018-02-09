/*
 * "Hello World" example.
 *
 * ECE 224 - Lab 1
 * Diana Jokic (20606918)
 * Lisa Zhang  (20525310)
 *
 */

#include <stdio.h>
#include "sys/alt_irq.h"
#include "altera_avalon_pio_regs.h"

//Define mode
#define INTERRUPT 0
//#define POLLING 0

#define PERIOD_START 2
#define PERIOD_END 5000

alt_u16 missed_pulses;
alt_u16 latency;
alt_u16 bckgnd_ctr = 0;

int background()
{
	IOWR(LED_PIO_BASE, 0, 1);
	int j;
	int x = 0;
	int grainsize = 4;
	int g_taskProcessed = 0;

	for(j = 0; j < grainsize; j++)
	{
		g_taskProcessed++;
	}
	bckgnd_ctr++;
	IOWR(LED_PIO_BASE, 0, 0);
	return x;
}

static void response_ISR(void* context, alt_u32 id)
{
	IOWR(LED_PIO_BASE, 0, 2);
	//toggle response out instead of flag
	IOWR(RESPONSE_OUT_BASE, 0, 1);									//write high to response out
	IOWR(RESPONSE_OUT_BASE, 0, 0);									//write low to response out

	IOWR(STIMULUS_IN_BASE, 3, 0);	 								//clears the req line
	IOWR(LED_PIO_BASE, 0, 0);
}


int main()
{
	//print mode
	#ifdef INTERRUPT
		printf("Operation Mode: INTERRUPT \n");
	#endif
	#ifdef POLLING
		printf("Operation Mode: POLLING \n");
	#endif

	//print titles
	printf("Period, Pulse width, Background tasks run, Avg latency, Missed pulses \n");
	while(0x0001 & IORD(BUTTON_PIO_BASE,0));							//wait for button PB0 to be pushed

#ifdef INTERRUPT

	IOWR(STIMULUS_IN_BASE, 3, 0); 										//clears the req line
	alt_irq_register(STIMULUS_IN_IRQ, (void *)0, response_ISR);			//register the interrupt
	IOWR(STIMULUS_IN_BASE, 2, 0XFFFF);									//enables interrupt

	int period = PERIOD_START;
	while(period < PERIOD_END + 1){
		bckgnd_ctr = 0;
		IOWR(EGM_BASE, 0, 0);											//enable EGM
		IOWR(EGM_BASE, 2, period);										//set period of EGM
		IOWR(EGM_BASE, 3, period/2);									//set pulse width of EGM

		IOWR(LED_PIO_BASE, 0, 4);
		IOWR(LED_PIO_BASE, 0, 0);

		IOWR(EGM_BASE, 0, 1);											//enable EGM


		while((IORD(EGM_BASE, 1)& 0x0001)!= 0){							//while EGM is busy, run background code
			background();
		}

		missed_pulses = IORD(EGM_BASE, 5);								//read missed pulses from EGM
		latency = IORD(EGM_BASE, 4);									//read latency from EGM
		IOWR(EGM_BASE, 0, 0);											//disable EGM

		printf("%d, %d, %d, %d, %d \n", period, period/2, bckgnd_ctr, latency, missed_pulses);
		period = period + 2;
	}
#endif

#ifdef POLLING
	int period = PERIOD_START;
	alt_u8 curr;

	while(period < PERIOD_END + 1){
		bckgnd_ctr = 0;
		alt_u8 characterization = 0;
		alt_u16 bckgnd_ctr_polling = 0;

		IOWR(EGM_BASE, 0, 0);											//disable EGM
		IOWR(EGM_BASE, 2, period);										//set period of EGM
		IOWR(EGM_BASE, 3, period/2);									//set pulse width of EGM

		IOWR(LED_PIO_BASE, 0, 4);
		IOWR(LED_PIO_BASE, 0, 0);

		IOWR(EGM_BASE, 0, 1);											//enable EGM

		while(IORD(EGM_BASE, 1)){										//while EGM is busy, run background code and poll
			if (characterization == 0){									//first loop; determine how many background tasks to do
				//wait for first EGM stimulus
				while (!IORD(STIMULUS_IN_BASE,0)){}

				//send Response
				IOWR(RESPONSE_OUT_BASE, 0, 1);							//write high to response out
				IOWR(RESPONSE_OUT_BASE, 0, 0);							//write low to response out

				//read new stimulus value
				curr = IORD(STIMULUS_IN_BASE,0);						//read new stimulus value

				//run as long as stimulus is high
				while(curr && (IORD(EGM_BASE, 1))){
					background();										//run background
					bckgnd_ctr_polling++;								//increment counter
					curr = IORD(STIMULUS_IN_BASE,0);					//read new value
				}

				//run as long as stimulus is low
				while(!curr && (IORD(EGM_BASE, 1))){
					background();										//run background
					bckgnd_ctr_polling++;								//increment counter
					curr = IORD(STIMULUS_IN_BASE,0);					//read new value
				}

				//the while loop detected a NEW rising edge, send response
				IOWR(RESPONSE_OUT_BASE, 0, 1);							//write high to response out
				IOWR(RESPONSE_OUT_BASE, 0, 0);							//write low to response out

				//after response, perform characterized background task number
				int i;
				for (i = 1; i < bckgnd_ctr_polling; i++){
					if(IORD(EGM_BASE, 1)==1){
						background();
					}else{
						break;
					}
				}
				//wait in case stimulus is still high
				while (IORD(STIMULUS_IN_BASE,0) && (IORD(EGM_BASE, 1))){}

				//first loop done, characterized background tasks
				characterization = 1;
			}

			//Not first loop code
			else{
				while (!IORD(STIMULUS_IN_BASE,0) && (IORD(EGM_BASE, 1))){}

				//new rising edge, send out response
				IOWR(RESPONSE_OUT_BASE, 0, 1);							//write high to response out
				IOWR(RESPONSE_OUT_BASE, 0, 0);							//write low to response out

				//perform characterized number of background tasks
				int j;
				for (j = 1; j < bckgnd_ctr_polling; j++){
					if(IORD(EGM_BASE, 1)== 1){
						background();
					}else{
						break;
					}
				}
				//wait in case stimulus is still high
				while (IORD(STIMULUS_IN_BASE,0) && (IORD(EGM_BASE, 1))){}
			}
		}

		missed_pulses = IORD(EGM_BASE, 5);								//read missed pulses from EGM
		latency = IORD(EGM_BASE, 4);									//read latency from EGM
		IOWR(EGM_BASE, 0, 0);											//disable EGM

		printf("%d, %d, %d, %d, %d \n", period, period/2, bckgnd_ctr, latency, missed_pulses);
		period = period + 2;

	}
#endif
	while(1);
		return 0;
}

