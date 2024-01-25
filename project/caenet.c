#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/bit_ops.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "hardware/structs/bus_ctrl.h"
#include "send.pio.h"

const uint CAPTURE_PIN_BASE = 16;
const uint CAPTURE_PIN_COUNT = 1;
const uint CAPTURE_N_SAMPLES = 4000;

void logic_analyser_init(PIO pio, uint sm, uint pin_base, uint pin_count, float div) {
    // Load a program to capture n pins. This is just a single `in pins, n`
    // instruction with a wrap.
    uint16_t capture_prog_instr = pio_encode_in(pio_pins, pin_count);
    struct pio_program capture_prog = {
            .instructions = &capture_prog_instr,
            .length = 1,
            .origin = -1
    };
    uint offset = pio_add_program(pio, &capture_prog);
    // Configure state machine to loop over this `in` instruction forever,
    // with autopush enabled.
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_in_pins(&c, pin_base);
    sm_config_set_wrap(&c, offset, offset);
    sm_config_set_clkdiv(&c, div);
    sm_config_set_in_shift(&c, true, true, 32);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);
    pio_sm_init(pio, sm, offset, &c);
}

void logic_analyser_arm(PIO pio, uint sm, uint dma_chan, uint32_t *capture_buf, size_t capture_size_words,
                        uint trigger_pin, bool trigger_level)
{
    pio_sm_set_enabled(pio, sm, false);
    pio_sm_clear_fifos(pio, sm);
    pio_sm_restart(pio, sm);

    dma_channel_config c = dma_channel_get_default_config(dma_chan);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);
    channel_config_set_dreq(&c, pio_get_dreq(pio, sm, false));

    dma_channel_configure(dma_chan, &c,
        capture_buf,        // Destination pointer
        &pio->rxf[sm],      // Source pointer
        capture_size_words, // Number of transfers
        true                // Start immediately
    );
    pio_sm_exec(pio, sm, pio_encode_wait_gpio(trigger_level, trigger_pin));
    pio_sm_set_enabled(pio, sm, true);
}

//funkcja do debugowania
void print_binary(uint32_t data) {
    for (int i = 0; i < 32; ++i) {
        putchar((data & (1u << i)) ? '1' : '0');
    }
}
//funkcja do debugowania
void print_binary8(uint8_t data) {
    for (int i = 7; i >= 0; --i) {
        putchar((data & (1u << i)) ? '1' : '0');
    }
}

// Funkcja do obliczania bitu parzystości dla danego bajtu
int calculateParityBit(uint8_t inputData) {
    int parityBit = 0;

    // Oblicz bit parzystości
    for (int j = 0; j < 8; j++) {
        parityBit ^= (inputData >> j) & 1;
    }

    return parityBit;
}

// Funkcja
size_t protocol_get(uint8_t *inputArray, size_t inputSize, uint32_t *bufor) 
{
	size_t outputSize=0;	//size of output
	int bit_count=0; //do przemieszczania się po bitach w inputarray
	int o_count=0;	//ile mamy zer po kolei
	int bufor_count=0;	//do przemieszczania sie po bajtach w outputarray
	int byte_count=0;	//do przemieszczania się po bajtach w inputarray
	int bit;			//znak
	
	for(int i=bufor_count;i<bufor_count+4;i++)
	{
		bufor = realloc(bufor, 4*(bufor_count+4)+1);
		bufor[i]=0xFFFFFFFF;
	}
	bufor_count+=4;

	while(byte_count<inputSize)
	{
		if(o_count==4)
		{
			bit=1;
			o_count=0;
		}
		else if(bit_count==8)
		{
			bit=calculateParityBit(inputArray[byte_count]);
			byte_count++;
			bit_count=0;
		}
		else
		{
			bit=(inputArray[byte_count] >> (7-bit_count)) & 1;
			bit_count++;
		}
		
		if(bit==0)
		{
			for(int i=bufor_count;i<bufor_count+4;i++)
			{
				bufor = realloc(bufor, 4*(bufor_count+4)+1);
				bufor[i]=0x00000000;
			}
			bufor_count+=4;
			o_count++;
		}
		else if(bit==1)
		{
			for(int i=bufor_count;i<bufor_count+4;i++)
			{
				bufor = realloc(bufor, 4*(bufor_count+4)+1);
				bufor[i]=0xFFFFFFFF;
			}
			bufor_count+=4;
			o_count=0;
		}
	}
	outputSize=bufor_count*sizeof(bufor[0]);
	return outputSize;
}

size_t get_tables( uint32_t *array,size_t size,uint8_t *bufor)
{
	bufor[0]=0; 
	int ones=0;		// liczba ciągów jedynek 
	int zeros=0;	// liczba ciągów zer
	bool currentChar = ((array[0] >> 0) & 1); //flaga na znak
    int currentCount = 1; //licznik 
	
    int bit=0;	//licznik bitóœ
	int bit1=0; //tylko pierwszy bit
    int bitpar=0; //licznik dla bitu parzystości
	
	size_t sent_lenght=0; //długość buforu 
	
    for (int k = 0; k < size/sizeof(array[0]); k++) 
    {
        for (int j = 0; j < 32; j++) 
        {
			if (((array[k] >> j) & 1) == currentChar)
			{
            currentCount++;
            } 
        else 
        {
            if (currentChar == 0) {
				ones=round((double)currentCount/(double)32);
				for(int i=1; i<=ones;i++)
				{
					if(zeros==4) 
					{
						zeros=0;
					}
					else if(bitpar==8)
					{
						bitpar=0;
						bufor = realloc(bufor, bit/8+1); // Powiększ bufor o jeden bajt
						if (bufor == NULL) {
}					
						bufor[(int)floor(bit/8)]=0;
					}
					else if(bit1==0)
					{
						bit1++;
					}
					else 
					{
						bufor[(int)(floor(bit/8))]=bufor[(int)floor(bit/8)] | 1 << (7-(bit%8));
						bit++;
						bitpar++;	
					}
				}
			} 
			else if (currentChar == 1) 
			{
				zeros=round((double)currentCount/(double)32);
				for(int i=1; i<=zeros;i++)
				{
					if (bitpar==8)
					{
						bitpar=0;
						bufor = realloc(bufor, bit/8+1	); // Powiększ bufor o jeden bajt
						bufor[(int)floor(bit/8)]=0;
					}
					else if(i<6) 
					{
						bit++;
						bitpar++;
					}
				}
				
            }
            
            currentChar = ((array[k] >> j) & 1);
            currentCount = 1;
        }
    }
}
		zeros=round((double)currentCount/(double)32);
		for(int i=1; i<=zeros;i++)
		{
			if (bitpar==8)
			{
				bitpar=0;
				bufor = realloc(bufor, bit/8+1	); // Powiększ bufor o jeden bajt
				bufor[(int)floor(bit/8)]=0;
			}
			else if(i<6) 
			{
				bit++;
				bitpar++;
				sent_lenght=(int)(floor(bit/8));
			}
		}
	return sent_lenght;
}

int main() 
{
	char user;
	gpio_init(16);
    gpio_set_dir(16, GPIO_IN);
    gpio_pull_up(16);

    gpio_init(18);
    gpio_set_dir(18, GPIO_OUT);
    
    gpio_init(19);
    gpio_set_dir(19, GPIO_OUT);
    
    gpio_init(20);
    gpio_set_dir(20, GPIO_OUT);
    gpio_put(20,1);
    
    stdio_init_all();
    uint buf_size_words =  32768; //2048 słów po 16 bitów każdy 2048*16=32768
    uint32_t *capture_buf = malloc(buf_size_words*sizeof(uint32_t));
    hard_assert(capture_buf);
    bus_ctrl_hw->priority = BUSCTRL_BUS_PRIORITY_DMA_W_BITS | BUSCTRL_BUS_PRIORITY_DMA_R_BITS;
    
    getchar();
    uint16_t memory=0; //ilość danych do wysłania (2 pierwsze bajty)
    
    uint8_t *input = malloc (memory*sizeof(uint8_t));
    hard_assert(input);
	uint8_t *outcome = malloc (sizeof(uint8_t));
	hard_assert(outcome);

    PIO pio = pio0;
    uint sm = pio_claim_unused_sm(pio, true);
    uint dma_chan = 0;
	
	PIO pio_send = pio1;
	uint sm1=pio_claim_unused_sm(pio_send, true);
	uint dma_chan1=1;
	uint offset = pio_add_program(pio_send, &send_program);
	
    logic_analyser_init(pio, sm, CAPTURE_PIN_BASE, CAPTURE_PIN_COUNT, 4.f);
    sleep_ms(100);
    send_program_init(pio_send, sm1, offset, 15,15.5);
    // The state machine is now running. Any value we push to its TX FIFO will
    bool error=1;	//flaga error
    uint32_t *buffer = malloc(sizeof(uint32_t));
    hard_assert(buffer);
    while(1)
	{
		memory=0;
		pio_sm_put_blocking(pio_send, sm1, 0);
		gpio_put(18,0);
		gpio_put(20,1);
		sleep_ms(20);
		memory |= (getchar() << 8); //kolejność starszy młodszy od lewej do prawej
		memory |= getchar();
		printf("Ilość pamięci przydzielonej do wysłanych danych: %X \n",memory);
		printf("Dane do wysłania: ");
		for(int i=0; i<memory; i++)
		{
			input[i]=getchar();
			printf("%X ",input[i]);
		} 
		printf("\n");
		size_t buffer_size=protocol_get(input, memory,buffer);
		size_t buffer_words= buffer_size/sizeof(buffer[0]);
		sleep_ms(20);
		logic_analyser_arm(pio, sm, dma_chan, capture_buf, buf_size_words, 16, true);
		sleep_ms(20);
		gpio_put(20,0);
		gpio_put(19,1);
		for(int i=0; i<buffer_words; i++)
		{
			pio_sm_put_blocking(pio_send, sm1, buffer[i]);
		}
		pio_sm_put_blocking(pio_send, sm1, 0);
		sleep_ms(100);
		//po nadawaniu, odbieramy dane od zasilacza
		//dla urządzenia odbiorczego (do testowania) musi być jeden jeśli podłączamy jedno urządzenie do drugiego
		pio_sm_put_blocking(pio_send, sm1, 1); 
		//dma_channel_wait_for_finish_blocking(dma_chan); //czekamy na dane
		//użycie funkcji dma_channel_is_busy(dma_chan) pozwala nam łatwo wprowadzić timeout error
		for(int i=0; i<5 ; i++)
		{
			sleep_ms(500);
			error=dma_channel_is_busy(dma_chan);
		}
		if(error==1)
		{
			printf("BRAK ODPOWIEDZI\n");
			gpio_put(18,1);
			gpio_put(19,0);
			sleep_ms(1000);
			continue;
		}
		gpio_put(19,0);
		error=1;
		for (size_t i = 0; i < buf_size_words; ++i) 
		{
			capture_buf[i]=~capture_buf[i];
		}
		for (size_t i = 0; i < buf_size_words; ++i) //sprawdzenie czy dane zwrotne to nie same zera
		{
			if(capture_buf[i]!=0x0000) error=0;
		}
		if(error==1)
		{
			printf("BRAK ODPOWIEDZI\n");
			gpio_put(19,0);
			gpio_put(18,1);
			sleep_ms(1000);
			continue;
		}
		
		size_t outcome_size=get_tables(capture_buf,buf_size_words,outcome);
		size_t outcome_words=outcome_size/sizeof(outcome[0]);
		//for (size_t i = 0; i < outcome_words*10; ++i) 
		//{
		//	print_binary(capture_buf[i]);
			//printf("\n");
		//}
		printf("Tablica danych odebranych w formacie hexagonalnym: \n");
		for (size_t i = 0; i < outcome_words; ++i) 
		{
			//print_binary8(outcome[i]);
			//printf("\t");
			printf("%X ",outcome[i]);
			//printf("\n");
		}
	}
}
