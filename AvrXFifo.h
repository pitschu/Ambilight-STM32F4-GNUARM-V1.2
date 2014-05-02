#ifndef _AvrXFifo_h_
#define _AvrXFifo_h_
/*
    Buffered BYTE oriented FIFO

	Uses providers/consumer model.  When an interrupt
	drives the consumer/provider side use the interrupt calls

	The fifo needs to be both declared and initialized.
*/

typedef struct AvrXFifo
{
	volatile short in;
	volatile short out;
	volatile short size;
	uint8_t buf[1];
}
AvrXFifo, *pAvrXFifo;

enum
{
	FIFO_OK,
	FIFO_ERR = -1	// Same as stdio EOF
};

#define AVRX_DECL_FIFO(Name, Size)					\
uint8_t Name##Fifo[Size + sizeof(AvrXFifo) - 1];	\
const pAvrXFifo Name = (pAvrXFifo)Name##Fifo;		\
static const short Name##FifoSz = Size;

#define AVRX_INIT_FIFO(Name)		\
	AvrXFlushFifo(Name);			\
	Name->size = Name##FifoSz

#define AVRX_EXT_FIFO(Name)			\
	extern uint8_t Name##Fifo[];	\
	static const pAvrXFifo Name = (pAvrXFifo)Name##Fifo

int16_t AvrXPutFifo(pAvrXFifo, uint8_t);
int16_t AvrXPutStringFifo(pAvrXFifo p, char *s);
int16_t AvrXPullFifo(pAvrXFifo);
void AvrXWaitPutFifo(pAvrXFifo, uint8_t);
int16_t AvrXWaitPullFifo(pAvrXFifo);
int16_t AvrXPeekFifo(pAvrXFifo);
int16_t AvrXStatFifo(pAvrXFifo);
void AvrXFlushFifo(pAvrXFifo);

#endif	// _AvrXFifo_h_
