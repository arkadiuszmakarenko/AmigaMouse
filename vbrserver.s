	INCLUDE "hardware/custom.i"
	INCLUDE "hardware/cia.i"
	INCLUDE "devices/input.i"
	INCLUDE "devices/inputevent.i"
	INCLUDE "resources/potgo.i"
	INCLUDE "lvo/exec_lib.i"
	INCLUDE "lvo/potgo_lib.i"


* Entered with:       A0 == scratch (execpt for highest pri vertb server)
*  D0 == scratch      A1 == is_Data
*  D1 == scratch      A5 == vector to interrupt code (scratch)
*                     A6 == scratch
*
    SECTION CODE

	; Pin 5 (mouse button 3) is connected to an interrupt enabled pin on the Arduino
	; Toggling this generates an interrupt and the Arduino writes the scroll wheel
	; values to right/middle mopuse buttons
	XDEF    _VertBServer
_VertBServer:

	; If the middle mouse button being held down do nothing!
	; Just in case we have plain mouse plugged in and MMB is down (line short)
	LEA	_custom,A0
	MOVE.W	potinp(A0), D0				; read POTGOR
	AND.W	#$100, D0				; mask with BIT8
	BEQ	exit

	; Output enable right & middle mouse.  Write 0 to middle
	; 09    OUTLX   Output enable for Paula (pin 32 for DIL and pin 35 for PLCC) -> enable
	; 08    DATLX   I/O data Paula (pin 32 for DIL and pin 35 for PLCC)          -> set low
	MOVE.L	14(A1),A6				; is_Data
	MOVE.L	#$00000200,D0				; word
	MOVE.L	#$00000300,D1				; mask
	JSR	_LVOWritePotgo(A6)		; WritePotgo(word,mask)(d0/d1)

	; Wait a bit.
	; The Arduino needs to catch the interrupt and reply on the right/middle mouse buttons
	LEA		_custom,A0

	; Save regs for C code before
	MOVE.W	joy0dat(A0),2(A1)			; Mouse Counters (used now)

Delay:
	; cocolino 36,
	; ez-mouse 25
	MOVEQ	#17,D1					; Needs testing on a slower Amiga!
.wait1
	MOVE.B	vhposr+1(A0),D0				; Bits 7-0     H8-H1 (horizontal position)
.wait2
	CMP.B	vhposr+1(A0),D0
	BEQ.B	.wait2
	DBF	D1,	.wait1

	; Save regs for C code after MMB pulse
	MOVE.W	potinp(A0),(A1)				; Middle/Right Mouse
	MOVE.W	joy0dat(A0),D0				; Mouse Counters (used now)
	EOR.W	D0,2(A1)				; EXOR joy0dat before and after pulse

	; cmp.b #0, D5
	; BNE skiplmouse
	MOVE.B 	$BFE001,4(A1)				; Left Mouse ODD CIA (CIA-A)
; skiplmouse:

	; If there was no change on data joy0dat values, no need to signal
	ANDI.W	#$0303,2(A1)				; mask out everything else, but the X0,X1, Y0 and Y1
	TST.W	2(A1)
	BEQ	exit

	;
	; Signal the main task
	; delay introduced in code below is enough to confirm reception to MSP
	;
	MOVE.L	A1,-(SP)				; preserve A1 on stack
	MOVE.L 4.W,A6
	MOVE.L 6(A1),D0					; Signals
	MOVE.L 10(A1),A1				; Task
	JSR _LVOSignal(A6)
	MOVE.L	(SP)+,A1				; restore A1 from stack

exit:
	; Output enable right & middle mouse.  Write 1 to middle
	; 09    OUTLX   Output enable for Paula (pin 32 for DIL and pin 35 for PLCC) -> enable
	; 08    DATLX   I/O data Paula (pin 32 for DIL and pin 35 for PLCC)          -> set high
	MOVE.L	14(A1),A6				; is_Data
	MOVE.L	#$00000300,D0				; word
	MOVE.L	#$00000300,D1				; mask
	JSR	_LVOWritePotgo(A6)			; WritePotgo(word,mask)(d0/d1)

	MOVE.L	$DFF000, A0				; if you install a vertical blank server at priority 10 or greater, you must place custom ($DFF000) in A0 before exiting
	MOVEQ.L #0,D0					; set Z flag to continue to process other vb-servers
	RTS
