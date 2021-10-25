#include <exec/interrupts.h>
#include <hardware/intbits.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <clib/exec_protos.h>
#include <clib/input_protos.h>
#include <clib/dos_protos.h>
#include <dos/dos.h>
#include <stdio.h>

#include "newmouse.h"
//#define NM_WHEEL_UP 0x7A
//#define NM_WHEEL_DOWN 0x7B

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
    UWORD potgo;		// 0 0xdff016
    UWORD joy0dat;		// 2
    UBYTE pra;			// 4 0xbfe001
    UBYTE pad;			// 5
    ULONG sigbit;		// 6
    struct Task *task;		// 10
    APTR potgoResource;		// 14
} mousedata;

struct Interrupt vertblankint = {
	{ 0, 0, NT_INTERRUPT, 10-10, "MSP430 laser mouse drver" },
	&mousedata,
	VertBServer
};

void CreateMouseEvents(int t);
int AllocResources();
void FreeResources();
extern struct CIA ciaa, ciab;

struct IOStdReq   *InputIO;
struct MsgPort    *InputMP;
struct InputEvent *MouseEvent;
struct InputBase  *InputBase;
BYTE intsignal;
int code;
#ifdef DEBUG
int button_state, prev_joy0dat;
#endif // DEBUG
int temp, bang_cnt;
ULONG potbits;

int main(void)
{
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
//		printf("DEBUG: NM_WHEEL driver started\n");
		bang_cnt = 0;
		while (1)
		{
			ULONG signals = Wait (mousedata.sigbit | SIGBREAKF_CTRL_C);
			if (signals & SIGBREAKF_CTRL_C)
			{
				PutStr("Exiting\n");
				break;
			}
			if (signals & mousedata.sigbit)
			{
				code = MM_NOTHING;
#ifdef DEBUG
				if(prev_joy0dat != (mousedata.joy0dat & 0x0303))
				{
					printf("joy: %04x->%04x -> %1X\n", mousedata.potgo & 0x0303, mousedata.joy0dat & 0x0303, temp);
					printf("joy: %04x->%04x -> %1X\n", prev_joy0dat & 0x0303, mousedata.joy0dat & 0x0303, temp);
					prev_joy0dat = mousedata.joy0dat & 0x0303;
				}
				if( (mousedata.joy0dat & 0x0303) != 0x0000)
				{
					printf("%1X -> ", temp);
				}
#endif // DEBUG
				switch(mousedata.joy0dat)// & 0x0303)
				{                    // YQXQ
					case 0x0202: // 0x0F MMB pressed
#ifdef DEBUG
						if(!(button_state & 0x01))
						{
							printf("%1X 1111 MMB down\n", temp);
#endif // DEBUG
							code |= MM_MIDDLEMOUSE_DOWN;
#ifdef DEBUG
							button_state |= 0x01;
						}
#endif // DEBUG
						break;
					case 0x0203: // 0x0E middle button up
#ifdef DEBUG
						if(button_state & 0x01)
						{
							printf("%1X 1110 MMB up\n", temp);
#endif // DEBUG
							code |= MM_MIDDLEMOUSE_UP;
#ifdef DEBUG
							button_state &= ~0x01;
						}
#endif // DEBUG
						break;

					case 0x0103: // 0x0C 4th down
#ifdef DEBUG
						if(!(button_state & 0x02))
						{
							printf("%1X 1100 4th down\n", temp);
#endif // DEBUG
							code |= MM_FOURTH_DOWN;
#ifdef DEBUG
							button_state |= 0x02;
						}
#endif // DEBUG
						break;
					case 0x0102: // 0x0D 4th up
#ifdef DEBUG
						if(button_state & 0x02)
						{
							printf("%1X 1101 4th up\n", temp);
#endif // DEBUG
							code |= MM_FOURTH_UP;
#ifdef DEBUG
							button_state &= ~0x02;
						}
#endif // DEBUG
						break;

					case 0x0301: // 0x0B 5th down
#ifdef DEBUG
						if(!(button_state & 0x04))
						{
							printf("%1X 5th down\n", temp);
#endif // DEBUG
							code |= MM_FIVETH_DOWN;
#ifdef DEBUG
							button_state |= 0x04;
						}
#endif // DEBUG
						break;
					case 0x0101: // 0x0A 5th up
#ifdef DEBUG
						if(button_state & 0x04)
						{
							printf("%1X 5th up\n", temp);
#endif // DEBUG
							code |= MM_FIVETH_UP;
#ifdef DEBUG
							button_state &= ~0x04;
						}
#endif // DEBUG
						break;

					case 0x0303: // 0x07 wheel right
#ifdef DEBUG
						printf("%1X wheel right\n", temp);
#endif // DEBUG
						code |= MM_WHEEL_RIGHT;
						break;
					case 0x0302: // 0x06 wheel left
#ifdef DEBUG
						printf("%1X wheel left\n", temp);
#endif // DEBUG
						code |= MM_WHEEL_LEFT;
						break;

					case 0x0200: // 0x03 wheel up
#ifdef DEBUG
						printf("%1X wheel up\n", temp);
#endif // DEBUG
						code |= MM_WHEEL_UP;
						break;
					case 0x0201: // 0x02 wheel down
#ifdef DEBUG
						printf("%1X wheel down\n", temp);
#endif // DEBUG
						code |= MM_WHEEL_DOWN;
						break;

					case 0x0000: // 1111 -> nothing
						printf("bang! (%d)\n", bang_cnt++);
						break;

					case 0x0001: // 0001
#ifdef DEBUG
						printf("0001\n");
#endif // DEBUG
						break;
					case 0x0003: // 0010
#ifdef DEBUG
						printf("0010\n");
#endif // DEBUG
						break;
					case 0x0100: // 0100
#ifdef DEBUG
						printf("0100\n");
#endif // DEBUG
						break;
					case 0x0300: // 1000
#ifdef DEBUG
						printf("1000\n");
#endif // DEBUG
						break;

					default:
						temp = mousedata.joy0dat ^ ((mousedata.joy0dat & 0x0202) >> 1);
						temp &= 0x0303;
						temp |= (temp & 0x0300) >> 6;
						temp &= 0x000F;
						printf("unsupported code 0x%04x -> 0x%02X -> %1d%1d%1d%1d", mousedata.joy0dat & 0x0303, temp,
							((temp & 0x0008) >> 3), ((temp & 0x0004) >> 2), ((temp & 0x0002) >> 1), ((temp & 0x0001) >> 0));
						break;
				}

				CreateMouseEvents(code);

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
		if (InputMP = CreateMsgPort())
		{
			if (MouseEvent = AllocMem(sizeof(struct InputEvent),MEMF_PUBLIC))
			{
				if (InputIO = CreateIORequest(InputMP,sizeof(struct IOStdReq)))
				{
					if (!OpenDevice("input.device",NULL,
								(struct IORequest *)InputIO,NULL))
					{
						InputBase = (struct InputBase *)InputIO->io_Device;
						if (mousedata.potgoResource = OpenResource("potgo.resource"))
						{
							potbits = AllocPotBits(OUTLX | DATLX);
							if(potbits == OUTLX | DATLX)
							{
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
	if (t == 0)
	{
		return;
	}
	MouseEvent->ie_EventAddress = NULL;
	MouseEvent->ie_NextEvent = NULL;
	MouseEvent->ie_Class = IECLASS_RAWKEY;
	MouseEvent->ie_SubClass = 0;
	MouseEvent->ie_Qualifier = PeekQualifier();		//???
	switch (t)
	{
		case MM_WHEEL_DOWN:
			MouseEvent->ie_Code = NM_WHEEL_DOWN;
		break;
		case MM_WHEEL_UP:
			MouseEvent->ie_Code = NM_WHEEL_UP;
		break;
		case MM_WHEEL_LEFT:
			MouseEvent->ie_Code = NM_WHEEL_LEFT;
		break;
		case MM_WHEEL_RIGHT:
			MouseEvent->ie_Code = NM_WHEEL_RIGHT;
		break;
		case MM_MIDDLEMOUSE_DOWN:
			MouseEvent->ie_Class = IECLASS_RAWMOUSE;
			MouseEvent->ie_Code = IECODE_MBUTTON;
			MouseEvent->ie_Qualifier = IEQUALIFIER_MIDBUTTON | IEQUALIFIER_RELATIVEMOUSE;
			MouseEvent->ie_X = 0;
			MouseEvent->ie_Y = 0;
		break;
		case MM_MIDDLEMOUSE_UP:
			MouseEvent->ie_Class = IECLASS_RAWMOUSE;
			MouseEvent->ie_Code = IECODE_MBUTTON | IECODE_UP_PREFIX;
			MouseEvent->ie_Qualifier = IEQUALIFIER_RELATIVEMOUSE;
			MouseEvent->ie_X = 0;
			MouseEvent->ie_Y = 0;
			break;
		case MM_FOURTH_DOWN:
			MouseEvent->ie_Code = NM_BUTTON_FOURTH;
			MouseEvent->ie_X = 0;
			MouseEvent->ie_Y = 0;
		break;
		case MM_FOURTH_UP:
			MouseEvent->ie_Code = NM_BUTTON_FOURTH | IECODE_UP_PREFIX;
			MouseEvent->ie_X = 0;
			MouseEvent->ie_Y = 0;
			break;
		case MM_FIVETH_DOWN:
			MouseEvent->ie_Code = NM_BUTTON_FIVETH;
			MouseEvent->ie_X = 0;
			MouseEvent->ie_Y = 0;
		break;
		case MM_FIVETH_UP:
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
