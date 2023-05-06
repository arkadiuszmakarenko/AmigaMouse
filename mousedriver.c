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

//#define DEBUG

// avoid definition of any of the following pairs to disable reporting them to OS
#define MM_WHEEL_UP 0x0A
#define MM_WHEEL_DOWN 0x09

// do not define the following constants to disable reporting MMB
//#define MM_MIDDLEMOUSE_UP 0x0D
//#define MM_MIDDLEMOUSE_DOWN 0x0E

//#define MM_WHEEL_RIGHT 0x06
//#define MM_WHEEL_LEFT 0x05

//#define MM_FOURTH_UP 0x80
//#define MM_FOURTH_DOWN 0x40

//#define MM_FIVETH_UP 0x08
//#define MM_FIVETH_DOWN 0x04

#if defined MM_MIDDLEMOUSE_UP && defined MM_MIDDLEMOUSE_DOWN
#if defined MM_WHEEL_LEFT && defined MM_WHEEL_RIGHT
#define TEMPLATE "M=MMB/S,V=VREV/S,H=HREV/S"
LONG myargs[3];
BOOL vreverse, hreverse, mmb_use;
#else
#define TEMPLATE "M=MMB/S,V=VREV/S"
LONG myargs[2];
BOOL vreverse, mmb_use;
#endif // defined(MM_WHEEL_LEFT) && defined(MM_WHEEL_RIGHT)
#else
#if defined MM_WHEEL_LEFT && defined MM_WHEEL_RIGHT
#define TEMPLATE "V=VREV/S,H=HREV/S"
LONG myargs[2];
BOOL vreverse, hreverse;
#else
#define TEMPLATE "V=VREV/S"
LONG myargs[1];
BOOL vreverse;
#endif // defined(MM_WHEEL_LEFT) && defined(MM_WHEEL_RIGHT)
#endif // defined MM_MIDDLEMOUSE_UP && defined MM_MIDDLEMOUSE_DOWN

struct RDArgs *myrda;


#define MM_NOTHING 0

#define OUTRY 1L<<15
#define DATRY 1L<<14
#define OUTRX 1L<<13
#define DATRX 1L<<12
#define OUTLY 1L<<11
#define DATLY 1L<<10
#define OUTLX 1L<<9
#define DATLX 1L<<8

#define LMB_MASK 1L<<6
#define RMB_MASK 1L<<7

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
		"Wheel Mouse Driver"	// char    *ln_Name;		/* ID string, null terminated */
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
int code, joydat, msgdata, prevmsgdata;
int bang_cnt;
ULONG potbits;
UBYTE button_state_mmb, button_state_4th, button_state_5th;
UWORD start_line;
int max_length;
int edge_count;

const char version[] = "$VER: TankMouse.driver 0.3 (" __DATE__ ") (c) 2023 Szymon Bieganski";

int mouse_code(unsigned int c)
{
  switch(c & 0x0F)
  {
#ifdef MM_FOURTH_DOWN
    case MM_FOURTH_DOWN:
      if(button_state_4th)
        { button_state_4th = 0; return MM_FOURTH_DOWN; }
      else
        return MM_NOTHING;
    break;
#endif // MM_FOURTH_DOWN
#ifdef MM_FOURTH_UP
    case MM_FOURTH_UP:
      if(button_state_4th)
        return MM_NOTHING;
      else
        { button_state_4th = 1; return MM_FOURTH_UP; }
    break;
#endif // MM_FOURTH_UP
#ifdef MM_FIVETH_DOWN
    case MM_FIVETH_DOWN:
      if(button_state_5th)
        { button_state_5th = 0; return MM_FIVETH_DOWN; }
      else
        return MM_NOTHING;
    break;
#endif // MM_FIVETH_DOWN
#ifdef MM_FIVETH_UP
    case MM_FIVETH_UP:
      if(button_state_5th)
        return MM_NOTHING;
      else
        { button_state_5th = 1; return MM_FIVETH_UP; }
    break;
#endif // MM_FIVETH_UP
  }
  return 1;
}

int main(void)
{
  mousedata.head = 0;
  mousedata.tail = 0;
  max_length = 0;
  edge_count = 0;
  button_state_mmb = button_state_4th = button_state_5th = 1;
	if (AllocResources())
	{
		mousedata.task = FindTask(0);
		mousedata.sigbit = 1 << intsignal;
		AddIntServer(INTB_VERTB, &vertblankint);
		SetTaskPri(mousedata.task, 20); /* same as input.device */

		printf("TankMouse wheel driver installed.\n");
		printf(__DATE__ "; " __TIME__ "\ngcc: " __VERSION__);
    printf("\nMake sure a suitable mouse is connected to mouse port,\notherwise expect unexpected.\n");

#if defined MM_MIDDLEMOUSE_UP && defined MM_MIDDLEMOUSE_DOWN
    mmb_use = FALSE;
#endif // defined MM_MIDDLEMOUSE_UP && defined MM_MIDDLEMOUSE_DOWN
    vreverse = FALSE;
#if defined(MM_WHEEL_LEFT) && defined(MM_WHEEL_RIGHT)
    hreverse = FALSE;
#endif // defined(MM_WHEEL_LEFT) && defined(MM_WHEEL_RIGHT)

    if(myrda = (struct RDArgs *)AllocDosObject(DOS_RDARGS, NULL)) {	/* parse my command line */
  		ReadArgs(TEMPLATE, myargs, myrda);
#if defined MM_MIDDLEMOUSE_UP && defined MM_MIDDLEMOUSE_DOWN
      if(myargs[0])
        mmb_use = TRUE;
      if(myargs[1])
        vreverse = TRUE;
#if defined(MM_WHEEL_LEFT) && defined(MM_WHEEL_RIGHT)
      if(myargs[2])
        hreverse = TRUE;
#endif // defined(MM_WHEEL_LEFT) && defined(MM_WHEEL_RIGHT)
#else
      if(myargs[0])
        vreverse = TRUE;
#if defined(MM_WHEEL_LEFT) && defined(MM_WHEEL_RIGHT)
      if(myargs[1])
        hreverse = TRUE;
#endif // defined(MM_WHEEL_LEFT) && defined(MM_WHEEL_RIGHT)
#endif // defined MM_MIDDLEMOUSE_UP && defined MM_MIDDLEMOUSE_DOWN
    }
#if defined MM_MIDDLEMOUSE_UP && defined MM_MIDDLEMOUSE_DOWN
    printf("use middle mouse button  : %s\n", mmb_use?"TRUE":"FALSE");
#endif // defined MM_MIDDLEMOUSE_UP && defined MM_MIDDLEMOUSE_DOWN
    printf("reverse vertical axis  : %s\n", vreverse?"TRUE":"FALSE");
#if defined(MM_WHEEL_LEFT) && defined(MM_WHEEL_RIGHT)
    printf("reverse horizontal axis: %s\n", hreverse?"TRUE":"FALSE");
#endif // defined(MM_WHEEL_LEFT) && defined(MM_WHEEL_RIGHT)
    printf("To stop press CTRL-C.\n");
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

          msgdata &= 0xCF;

          if((mousedata.head - mousedata.tail) > max_length)
            max_length = mousedata.head - mousedata.tail;

          //temp = ((msgdata & 0x0008) >> 3) | ((msgdata & 0x0004) >> 2) | ((msgdata & 0x0002) >> 1) | ((msgdata & 0x0001) >> 0));

#ifdef DEBUG
          if(prevmsgdata != msgdata) {
            printf("0x%02X [%1d%1d%1d%1d] -> LMB: %1d; RMB: %1d; Q: 0x%01X\n",
            msgdata,
            ((msgdata & 0x0008) >> 3), ((msgdata & 0x0004) >> 2), ((msgdata & 0x0002) >> 1), ((msgdata & 0x0001) >> 0),
            (msgdata & LMB_MASK) >> 6,
            (msgdata & RMB_MASK) >> 7,
            (msgdata & 0x000F) >> 0
            );
          }
#endif // DEBUG

          if(msgdata & LMB_MASK)
          {
            if(!button_state_mmb)
            {
              button_state_mmb = 1;
#ifdef MM_MIDDLEMOUSE_UP
              if(mmb_use)
                CreateMouseEvents(MM_MIDDLEMOUSE_UP);
              else
              {
                ++edge_count;
#ifdef DEBUG
                PutStr("Debug: MMB event detected.\n");
#endif // DEBUG
              }
#endif // MM_MIDDLEMOUSE_UP
            }
            else
            {
              if(!(msgdata & RMB_MASK))
                CreateMouseEvents(msgdata & 0x0F);
            }
          }
          else
          {
            if(button_state_mmb)
            {
              button_state_mmb = 0;
#ifdef MM_MIDDLEMOUSE_DOWN
              if(mmb_use)
                CreateMouseEvents(MM_MIDDLEMOUSE_DOWN);
                else
                {
                  ++edge_count;
#ifdef DEBUG
                  PutStr("Debug: MMB event detected.\n");
#endif // DEBUG
                }
#endif // MM_MIDDLEMOUSE_DOWN
            }
            else
            {
              if(!(msgdata & RMB_MASK))
                CreateMouseEvents(msgdata & 0x0F);
            }
          }

          prevmsgdata = msgdata;

          // accept wheel mouse movement IFF RMB low
          //if(!(msgdata & 0x0020)) // && mouse_code((msgdata & 0x000F)))
          //  CreateMouseEvents(mouse_code(msgdata));

          //if(temp)
          //  ++edge_count;

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
        printf("Maximum FIFO used depth was: %d\n", max_length);
        printf("Spurious edges encountered: %d\n", edge_count);
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
							potbits = AllocPotBits(OUTLX | DATLX); // | OUTLY | DATLY);
							if(potbits == OUTLX | DATLX) // | OUTLY | DATLY)
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
  if(vreverse && ((t == MM_WHEEL_DOWN) || (t == MM_WHEEL_UP)))
    if(t == MM_WHEEL_DOWN)
      t = MM_WHEEL_UP;
    else
      t = MM_WHEEL_DOWN;
#if defined MM_WHEEL_LEFT && defined MM_WHEEL_RIGHT
  if(hreverse && ((t == MM_WHEEL_LEFT) || (t == MM_WHEEL_RIGHT)))
    if(t == MM_WHEEL_RIGHT)
      t = MM_WHEEL_LEFT;
    else
      t = MM_WHEEL_RIGHT;
#endif // defined(MM_WHEEL_LEFT) && defined(MM_WHEEL_RIGHT)

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
#ifdef MM_WHEEL_LEFT
		case MM_WHEEL_LEFT:
#ifdef DEBUG
		  printf("Debug: Wheel Left.\n");
#endif // DEBUG
			MouseEvent->ie_Code = NM_WHEEL_LEFT;
		break;
#endif // MM_WHEEL_LEFT
#ifdef MM_WHEEL_RIGHT
		case MM_WHEEL_RIGHT:
#ifdef DEBUG
		  printf("Debug: Wheel Right.\n");
#endif // DEBUG
			MouseEvent->ie_Code = NM_WHEEL_RIGHT;
		break;
#endif // MM_WHEEL_RIGHT
#ifdef MM_MIDDLEMOUSE_DOWN
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
#endif // MM_MIDDLEMOUSE_DOWN
#ifdef MM_MIDDLEMOUSE_UP
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
#endif // MM_MIDDLEMOUSE_UP
#ifdef MM_FOURTH_DOWN
		case MM_FOURTH_DOWN:
#ifdef DEBUG
		  printf("Debug: 4th down.\n");
#endif // DEBUG
			MouseEvent->ie_Code = NM_BUTTON_FOURTH;
			MouseEvent->ie_X = 0;
			MouseEvent->ie_Y = 0;
		break;
#endif // MM_FOURTH_DOWN
#ifdef MM_FOURTH_UP
		case MM_FOURTH_UP:
#ifdef DEBUG
		  printf("Debug: 4th up.\n");
#endif // DEBUG
			MouseEvent->ie_Code = NM_BUTTON_FOURTH | IECODE_UP_PREFIX;
			MouseEvent->ie_X = 0;
			MouseEvent->ie_Y = 0;
			break;
#endif // MM_FOURTH_UP
#ifdef MM_FIVETH_DOWN
		case MM_FIVETH_DOWN:
#ifdef DEBUG
		  printf("Debug: 5th down.\n");
#endif // DEBUG
			MouseEvent->ie_Code = NM_BUTTON_FIVETH;
			MouseEvent->ie_X = 0;
			MouseEvent->ie_Y = 0;
		break;
#endif // MM_FIVETH_DOWN
#ifdef MM_FIVETH_UP
		case MM_FIVETH_UP:
#ifdef DEBUG
		  printf("Debug: 5th up.\n");
#endif // DEBUG
			MouseEvent->ie_Code = NM_BUTTON_FIVETH | IECODE_UP_PREFIX;
			MouseEvent->ie_X = 0;
			MouseEvent->ie_Y = 0;
			break;
#endif // MM_FIVETH_UP
    default:
    #ifdef DEBUG
		  printf("Debug: unrecognized code 0x%02X.\n", t);
    #endif // DEBUG
      ++edge_count;
      return;
    break;
	}

	InputIO->io_Data = (APTR)MouseEvent;
	InputIO->io_Length = sizeof(struct InputEvent);
	InputIO->io_Command = IND_WRITEEVENT;

	DoIO((struct IORequest *)InputIO);
}
