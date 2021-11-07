#include <exec/interrupts.h>
#include <hardware/intbits.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <clib/exec_protos.h>
#include <clib/input_protos.h>
#include <clib/dos_protos.h>
#include <clib/potgo_protos.h>
#include <dos/dos.h>
#include <stdio.h>

#include "newmouse.h"
//#define NM_WHEEL_UP 0x7A
//#define NM_WHEEL_DOWN 0x7B

#define VERSTAG "\0$VER: Blabber mouse driver 0.2.0 ("__DATE__")"

//#define DEBUG
#define MM_NOTHING 0
#define MM_WHEEL_DOWN 1
#define MM_WHEEL_UP 2
#define MM_WHEEL_LEFT 3
#define MM_WHEEL_RIGHT 4
#define MM_MIDDLEMOUSE_DOWN 5
#define MM_MIDDLEMOUSE_UP 6
#define MM_FOURTH_DOWN 7
#define MM_FOURTH_UP 8
#define MM_FIVETH_DOWN 9
#define MM_FIVETH_UP 10

#define OUTRY 1L<<15
#define DATRY 1L<<14
#define OUTRX 1L<<13
#define DATRX 1L<<12
#define OUTLY 1L<<11
#define DATLY 1L<<10
#define OUTLX 1L<<9
#define DATLX 1L<<8

extern void VertBServer();  /* proto for asm interrupt server */

struct {
    UBYTE head;		        // 0 message counter head
    UBYTE tail;		        // 1 message counter tail
    ULONG sigbit;         // 2
    struct Task *task;		// 6
    APTR potgoResource;		// 10
    UBYTE codes[256];     // 14
} mousedata;

struct Interrupt vertblankint = {
	//    struct  Node is_Node;
	{	0,			// struct  Node *ln_Succ;	/* Pointer to next (successor) */
		0,			// struct  Node *ln_Pred;	/* Pointer to previous (predecessor) */
		NT_INTERRUPT,		// UBYTE   ln_Type;
		10-10, //10-10,			// BYTE    ln_Pri;		/* Priority, for sorting */
		"Wheel Mouse Drver"	// char    *ln_Name;		/* ID string, null terminated */
	},
	&mousedata,			// APTR    is_Data;		    /* server data segment  */
	VertBServer			// VOID    (*is_Code)();	    /* server code entry    */
};

void CreateMouseEvents(int t);
int AllocResources();
void FreeResources();
extern struct CIA ciaa, ciab;

struct IOStdReq   *InputIO;
struct MsgPort    *InputMP;
struct InputEvent *MouseEvent;
struct InputBase  *InputBase;
struct PotgoBase  *PotgoBase;
BYTE intsignal;
int code,joydat, msgdata;
int temp, bang_cnt;
ULONG potbits;
UBYTE button_state;
UWORD start_line;

STRPTR ver = (STRPTR)VERSTAG;

/*  0x23 | 1011 |1  1  1  0 |1110|E|1101|D| 3 | | CODE_MMB_DOWN
    0x21 | 1001 |1  1  0  1 |1101|D|1110|E| 3 | | CODE_MMB_UP
    0x20 | 1000 |1  1  0  0 |1100|C|1100|C| 2 | | CODE_4TH_DOWN
    0x32 | 1110 |1  0  1  1 |1011|B|1011|B| 3 | | CODE_WHEEL_UP
    0x33 | 1111 |1  0  1  0 |1010|A|1001|9| 2 |*| CODE_4TH_UP
    0x31 | 1101 |1  0  0  1 |1001|9|1010|A| 2 |*| CODE_WHEEL_LEFT
    0x12 | 0110 |0  1  1  1 |0111|7|0111|7| 3 | | CODE_WHEEL_DOWN
    0x13 | 0111 |0  1  1  0 |0110|6|0101|5| 2 |*| CODE_WHEEL_RIGHT
    0x11 | 0101 |0  1  0  1 |0101|5|0110|6| 2 |*| CODE_5TH_UP
    0x02 | 0010 |0  0  1  1 |0011|3|0011|3| 2 | | CODE_5TH_DOWN */

unsigned int mouse_codes[] = {  MM_NOTHING, // 0
                                MM_NOTHING, // 1
                                MM_NOTHING, // 2
                                MM_FIVETH_DOWN, // 3
                                MM_NOTHING, // 4
                                MM_FIVETH_UP, // 5
                                MM_WHEEL_RIGHT, // 6
                                MM_WHEEL_DOWN, // 7
                                MM_NOTHING, // 8
                                MM_WHEEL_LEFT, // 9
                                MM_FOURTH_UP, // A
                                MM_WHEEL_UP, // B
                                MM_FOURTH_DOWN, // C
                                MM_MIDDLEMOUSE_UP, // D
                                MM_MIDDLEMOUSE_DOWN, // E
                                MM_NOTHING // F
};

int mouse_code(int joydat)
{
  int c = mouse_codes[joydat];
  if(c == MM_FOURTH_DOWN) {
    if(button_state & 0x01)
      c = MM_NOTHING;
    else
      button_state |= 0x01;
  }
  if(c == MM_FOURTH_UP) {
    if(button_state & 0x01)
      button_state &= ~0x01;
    else
      c = MM_NOTHING;
  }
  if(c == MM_FIVETH_DOWN) {
    if(button_state & 0x02)
      c = MM_NOTHING;
    else
      button_state |= 0x02;
  }
  if(c == MM_FIVETH_UP) {
    if(button_state & 0x02)
      button_state &= ~0x02;
    else
      c = MM_NOTHING;
  }
  return c;
}

int main(void)
{
  mousedata.head = 0;
  mousedata.tail = 0;
  button_state = 0;
	if (AllocResources())
	{
		mousedata.task = FindTask(0);
		mousedata.sigbit = 1 << intsignal;
		AddIntServer(INTB_VERTB, &vertblankint);
		SetTaskPri(mousedata.task, 20); /* same as input.device */

		printf("Blabber mouse wheel driver installed.\n");
		printf(__DATE__);
		printf("; ");
		printf(__TIME__);
		printf("\ngcc: ");
		printf(__VERSION__);
    printf("\nMake sure a suitable mouse is connected to mouse port,\notherwise expect unexpected.\n");
    printf("To stop press CTRL-C.\n");
//		printf("DEBUG: NM_WHEEL driver started\n");
		bang_cnt = 0;

		while (1)
		{
			ULONG signals = Wait (mousedata.sigbit | SIGBREAKF_CTRL_C);
			if (signals & mousedata.sigbit)
			{
        while(mousedata.head != mousedata.tail)
        {
          msgdata = mousedata.codes[mousedata.tail];
          //msgdata = msgdata ^ ((msgdata & 0xAA) >> 1);

          temp = msgdata >> 4; // ^ ((msgdata & 0x22) >> 1);
          msgdata &= 0x0F;

#ifdef DEBUG
          printf("%1d%1d%1d%1d -> %1d%1d%1d%1d\n",
          ((temp & 0x0008) >> 3), ((temp & 0x0004) >> 2), ((temp & 0x0002) >> 1), ((temp & 0x0001) >> 0),
          ((msgdata & 0x0008) >> 3), ((msgdata & 0x0004) >> 2), ((msgdata & 0x0002) >> 1), ((msgdata & 0x0001) >> 0));
#endif // DEBUG

          // detect accidental change on the data lines
          // only accept the expected reaction of MSP controller
					if(!temp && msgdata)
            CreateMouseEvents((msgdata));

          mousedata.codes[mousedata.tail] = 0;
          ++mousedata.tail;
        }
			}
			if (signals & SIGBREAKF_CTRL_C)
			{
#ifdef DEBUG
        printf("head: %d.\n", mousedata.head);
        printf("tail: %d.\n", mousedata.tail);
#endif // DEBUG
				PutStr("Exiting\n");
				break;
			}
		}
	} else {
		return -1;
	}
	FreeResources();

	return 0;
}

int AllocResources()
{
	if (intsignal = AllocSignal (-1))
	{
#ifdef DEBUG
		PutStr("Debug: Signal allocated.\n");
#endif // DEBUG
		if (InputMP = CreateMsgPort())
		{
#ifdef DEBUG
			PutStr("Debug: Message port created.\n");
#endif // DEBUG
			if (MouseEvent = AllocMem(sizeof(struct InputEvent),MEMF_PUBLIC))
			{
#ifdef DEBUG
				PutStr("Debug: Memory for MouseEvent allocated.\n");
#endif // DEBUG
				if (InputIO = CreateIORequest(InputMP,sizeof(struct IOStdReq)))
				{
#ifdef DEBUG
					PutStr("Debug: CIORequest created.\n");
#endif // DEBUG
					if (!OpenDevice("input.device",0UL,
								(struct IORequest *)InputIO,0UL))
					{
#ifdef DEBUG
						PutStr("Debug: input.device opened.\n");
#endif // DEBUG
						InputBase = (struct InputBase *)InputIO->io_Device;
						if (mousedata.potgoResource = OpenResource("potgo.resource"))
						{
#ifdef DEBUG
							PutStr("Debug: potgo.resource opened.\n");
#endif // DEBUG
							PotgoBase = (struct PotgoBase *)mousedata.potgoResource;
							potbits = AllocPotBits(OUTLX | DATLX);
							if(potbits == OUTLX | DATLX)
							{
#ifdef DEBUG
								PutStr("Debug: potgo output MMB allocated.");
#endif // DEBUG
								return 1;
							}
							else
								PutStr("Error: Could not allocate potgo output MMB");
						}
						else
							PutStr("Error: Could not open potgo.resource\n");
					}
					else
						PutStr("Error: Could not open input.device\n");
				}
				else
					PutStr("Error: Could not create IORequest\n");
			}
			else
				PutStr("Error: Could not allocate memory for MouseEvent\n");
		}
		else
			PutStr("Error: Could not create message port\n");
	}
	else
		PutStr("Error: Could not allocate signal\n");
	return 0;
}

void FreeResources()
{
	FreePotBits(OUTLX | DATLX);
	if (InputIO)
	{
		CloseDevice((struct IORequest *)InputIO);
		DeleteIORequest(InputIO);
	}
	if (MouseEvent) FreeMem(MouseEvent, sizeof(struct InputEvent));
	if (InputMP) DeleteMsgPort(InputMP);
	if (intsignal) FreeSignal(intsignal);
	RemIntServer(INTB_VERTB, &vertblankint);
}

void CreateMouseEvents(int t)
{
  if(t == MM_NOTHING)
    return;
	MouseEvent->ie_EventAddress = NULL;
	MouseEvent->ie_NextEvent = NULL;
	MouseEvent->ie_Class = IECLASS_RAWKEY;
	MouseEvent->ie_SubClass = 0;
	MouseEvent->ie_Qualifier = PeekQualifier();		//???
	switch (t)
	{
		case MM_WHEEL_DOWN:
			MouseEvent->ie_Code = NM_WHEEL_DOWN;
#ifdef DEBUG
		  printf("Debug: Wheel Down.\n");
#endif // DEBUG
		break;
		case MM_WHEEL_UP:
#ifdef DEBUG
		  printf("Debug: Wheel Up.\n");
#endif // DEBUG
			MouseEvent->ie_Code = NM_WHEEL_UP;
		break;
		case MM_WHEEL_LEFT:
#ifdef DEBUG
		  printf("Debug: Wheel Left.\n");
#endif // DEBUG
			MouseEvent->ie_Code = NM_WHEEL_LEFT;
		break;
		case MM_WHEEL_RIGHT:
#ifdef DEBUG
		  printf("Debug: Wheel Right.\n");
#endif // DEBUG
			MouseEvent->ie_Code = NM_WHEEL_RIGHT;
		break;
		case MM_MIDDLEMOUSE_DOWN:
#ifdef DEBUG
		  printf("Debug: MMB down.\n");
#endif // DEBUG
			MouseEvent->ie_Class = IECLASS_RAWMOUSE;
			MouseEvent->ie_Code = IECODE_MBUTTON;
			MouseEvent->ie_Qualifier = IEQUALIFIER_MIDBUTTON | IEQUALIFIER_RELATIVEMOUSE;
			MouseEvent->ie_X = 0;
			MouseEvent->ie_Y = 0;
		break;
		case MM_MIDDLEMOUSE_UP:
#ifdef DEBUG
		  printf("Debug: MMB up.\n");
#endif // DEBUG
			MouseEvent->ie_Class = IECLASS_RAWMOUSE;
			MouseEvent->ie_Code = IECODE_MBUTTON | IECODE_UP_PREFIX;
			MouseEvent->ie_Qualifier = IEQUALIFIER_RELATIVEMOUSE;
			MouseEvent->ie_X = 0;
			MouseEvent->ie_Y = 0;
			break;
		case MM_FOURTH_DOWN:
#ifdef DEBUG
		  printf("Debug: 4th down.\n");
#endif // DEBUG
			MouseEvent->ie_Code = NM_BUTTON_FOURTH;
			MouseEvent->ie_X = 0;
			MouseEvent->ie_Y = 0;
		break;
		case MM_FOURTH_UP:
#ifdef DEBUG
		  printf("Debug: 4th up.\n");
#endif // DEBUG
			MouseEvent->ie_Code = NM_BUTTON_FOURTH | IECODE_UP_PREFIX;
			MouseEvent->ie_X = 0;
			MouseEvent->ie_Y = 0;
			break;
		case MM_FIVETH_DOWN:
#ifdef DEBUG
		  printf("Debug: 5th down.\n");
#endif // DEBUG
			MouseEvent->ie_Code = NM_BUTTON_FIVETH;
			MouseEvent->ie_X = 0;
			MouseEvent->ie_Y = 0;
		break;
		case MM_FIVETH_UP:
#ifdef DEBUG
		  printf("Debug: 5th up.\n");
#endif // DEBUG
			MouseEvent->ie_Code = NM_BUTTON_FIVETH | IECODE_UP_PREFIX;
			MouseEvent->ie_X = 0;
			MouseEvent->ie_Y = 0;
			break;
	}

	InputIO->io_Data = (APTR)MouseEvent;
	InputIO->io_Length = sizeof(struct InputEvent);
	InputIO->io_Command = IND_WRITEEVENT;

	DoIO((struct IORequest *)InputIO);
}
