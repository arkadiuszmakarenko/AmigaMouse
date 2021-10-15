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

extern void VertBServer();  /* proto for asm interrupt server */

struct {
    UWORD potgo;			// 0 0xdff016
    UWORD joy0dat;			// 2
    UBYTE pra;				// 4 0xbfe001
    UBYTE pad;				// 5
    ULONG sigbit;			// 6
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
int code, button_state, prev_joy0dat, temp;

int main(void)
{
	if (AllocResources())
	{
		mousedata.task = FindTask(0);
		mousedata.sigbit = 1 << intsignal;

		AddIntServer(INTB_VERTB, &vertblankint);

		SetTaskPri(mousedata.task, 20); /* same as input.device */
		printf("Blabber mouse wheel driver installed.\nMake sure a suitable mouse is connected to mouse port, otherwise expect unexpected.\n");
//		printf("DEBUG: NM_WHEEL driver started\n");
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

				temp = mousedata.joy0dat ^ ((mousedata.joy0dat & 0x0202) >> 1);
				temp &= 0x0303;
				temp |= (temp & 0x0300) >> 6;
				temp &= 0x000F;
				temp ^= 0x000F;
				code = MM_NOTHING;
//				if(prev_joy0dat != (mousedata.joy0dat & 0x0303))
//				{
//					printf("joy: %04x->%04x -> ", prev_joy0dat & 0x0303, mousedata.joy0dat & 0x0303);
//					prev_joy0dat = mousedata.joy0dat & 0x0303;
//				}
//				if( (mousedata.joy0dat & 0x0303) != 0x0202)
//				{
//					printf("%1X -> ", temp);
//				}
				switch(mousedata.joy0dat & 0x0303)
				{                    // YQXQ
					case 0x0000: // 1111 MMB pressed
						if(!(button_state & 0x01))
						{
//							printf("%1X 1111 MMB down\n", temp);
							code |= MM_MIDDLEMOUSE_DOWN;
							button_state |= 0x01;
						}
						break;
					case 0x0001: // 1110 middle button up
						if(button_state & 0x01)
						{
//							printf("%1X 1110 MMB up\n", temp);
							code |= MM_MIDDLEMOUSE_UP;
							button_state &= ~0x01;
						}
						break;

					case 0x0002: // 1100 4th down
						if(!(button_state & 0x02))
						{
//							printf("%1X 1100 4th down\n", temp);
							code |= MM_FOURTH_DOWN;
							button_state |= 0x02;
						}
						break;
					case 0x003: // 1101 4th up
						if(button_state & 0x02)
						{
//							printf("%1X 1101 4th up\n", temp);
							code |= MM_FOURTH_UP;
							button_state &= ~0x02;
						}
						break;

					case 0x0100: // 1011 5th down
						if(!(button_state & 0x04))
						{
//							printf("%1X 1011 5th down\n", temp);
							code |= MM_FIVETH_DOWN;
							button_state |= 0x04;
						}
						break;
					case 0x0101: // 1010 5th up
						if(button_state & 0x04)
						{
//							printf("%1X 1010 5th up\n", temp);
							code |= MM_FIVETH_UP;
							button_state &= ~0x04;
						}
						break;
					case 0x0102: // 1000 wheel right
						printf("1000 wheel right\n");
						code |= MM_WHEEL_RIGHT;
						break;
					case 0x0103: // 1001 wheel left
						code |= MM_WHEEL_LEFT;
						printf("1001 wheel left\n");
						break;

					case 0x0200: // 0011 wheel up
						code |= MM_WHEEL_UP;
//						printf("0011 wheel up\n");
						break;
					case 0x0201: // 0010 wheel down
						code |= MM_WHEEL_DOWN;
//						printf("0010 wheel down\n");
						break;

					case 0x0202: // 1111 -> nothing
//						printf("\n");
//					printf("0x%02x\n", prev_joy0dat & 0x0303);
						break;
					default:
						printf("unsupported code 0x%02x -> 0x%02X", mousedata.joy0dat & 0x0303, temp);
//						printf("1/Y %1d; ", (temp & 0x0008) >> 3);
//						printf("2/X %1d; ", (temp & 0x0004) >> 2);
//						printf("3YQ %1d; ", (temp & 0x0002) >> 1);
//						printf("4XQ %1d\n", (temp & 0x0001) >> 0);
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
							return 1;
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
