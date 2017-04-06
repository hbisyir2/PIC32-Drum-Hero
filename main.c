////////////////////////////////////////////////////////////////////////////////////
// ECE 2534:        Lab 04, Hakeem Bisyir
//
// File name:       main.c
//
// Description:     This programs implements a variation of the game Guitar Hero.
//                  Upon starting, it will display two welcome messages that last
//                  for about 5 seconds each.  The main menu has two options, play
//                  game and difficulty.  The difficulty menu allows you to change
//                  between easy and hard mode.  The play game option takes
//                  you to the game.  The game entails following the action
//                  of the note on the screen to increment your score.  Following
//                  the game, there is a post-game message that displays for about
//                  5 seconds.  The game then takes you back to the main menu.
//
// Efficiency:		The program works as described.
//
// My Experience:	Designing the interface of the game was easy.  The challenging
//                  part of the project was being able to detect when a note
//                  was hit at the right interval.  Detecting the left twist 
//                  and right twist was fairly simple as it followed the model
//                  of the buttons and joystick however the double tap took
//                  a fair amount of reading and experimenting.
//                  
//
// Additional
// Implementations: The difficulty modifier changes both the speed that the
//                  notes move and the space between the notes.
//
//                  The songs are all randomly generated.
//
// Date:            12/02/2016


#include <stdio.h>                      // for sprintf()
#include <plib.h>                       // Peripheral Library
#include <stdbool.h>                    // for data type bool
#include "PmodOLED.h"
#include "OledChar.h"
#include "OledGrph.h"
#include "delay.h"

// Diligent board configuration
#pragma config ICESEL       = ICS_PGx1  // ICE/ICD Comm Channel Select
#pragma config DEBUG        = OFF       // Debugger Disabled for Starter Kit
#pragma config FNOSC        = PRIPLL	// Oscillator selection
#pragma config POSCMOD      = XT	    // Primary oscillator mode
#pragma config FPLLIDIV     = DIV_2	    // PLL input divider
#pragma config FPLLMUL      = MUL_20	// PLL multiplier
#pragma config FPLLODIV     = DIV_1	    // PLL output divider
#pragma config FPBDIV       = DIV_8	    // Peripheral bus clock divider
#pragma config FSOSCEN      = OFF	    // Secondary oscillator enable

// struct for each note that is displayed
typedef struct __note {
    int noteRow;        // Row the note will be drawn on
    int noteColumn;     // Column the note will be drawn on
    int LRD;            // Detemines if the note is an L, R, or D
    bool draw;          // will draw the note if this parameter is true
} note;

// function declarations
void initialize(SpiChannel chn, unsigned int ClkDiv);                           // initializes Oled, SPI, and Accelerometer
void initSPI(SpiChannel chn, unsigned int stcClkDiv);                           // initialize SPI
void setAccelReg(SpiChannel chn, unsigned int address, unsigned int data);      // writes to an accelerometer register
int getAccelReg(SpiChannel chn, unsigned int address);                          // reads from an accelerometer register
void initAccelerometer(SpiChannel chn);                                         // initialize accelerometer
void getAccelData(SpiChannel chn, int accelData[]);                             // reads XYZ values from accel
void Timer1InitNotesMoveEasy();                                                 // timer that determines rate that the notes move
void Timer1InitNotesMoveHard();                                                 // timer that determines rate that the notes move
void Timer2InitReadSPI();                                                       // timer that controls samples from the accel
void Timer3InitUpdateCrowd();                                                   // timer that controls when the crowd bar updates
void Timer4InitMenuDebounce();                                                  // Timer that controls debouncing the main menu
void Timer5InitWelcomeMessage();                                                // timer that controls welcome messages
bool detectRT();                                                                // returns true if a right twist is detected
bool detectLT();                                                                // returns true if a left twist is detected
void drawGame(int noteCenter, int crowdCenter);                                 // Draws the guitar hero board
void clearBoard();                                                              // Clears the notes and check mark on the board
void drawMid(int center);                                                       // Draws the vertical note bar in the middle of the scree
void drawCrowdBar(int center);                                                  // Draws the horizontal crowd fill bar on the board
void fillCrowdBar(int center, int applause);                                    // fills the crowd bar to the correct level
void drawNote(note toDraw);                                                     // Draws the note from the struct defined above
void drawL(int x, int y);                                                       // Draws an L on a pixel basis
void drawR(int x, int y);                                                       // Draws an R on a pixel basis
void drawD(int x, int y);                                                       // Draws a D on a pixel basis
void drawCheck();                                                               // Draws a check mark on a pixel basis
void createSong(note songToStore[], int size, bool hard);                       // Stores the notes into an array of notes
bool getInput1();                                                               // Detects button 1 press
bool getInput2();                                                               // Detects button 2 press
void initTapDetect(SpiChannel chn);                                             // Initialize accel to detect a double tap
void clear2BRegister(SpiChannel chn);                                           // Initialize accel to clear the z axis bit
bool checkDoubleTap(SpiChannel chn);                                            // returns true if a double tap is detected

// Constant global variables
#define JE_SPI SPI_CHANNEL3         // SPI channel for pin JE
const int noteWidth = 12;           // Width of the note bar
const int noteLength = 3;           // Length of the note bar
const int crowdLength = 60;         // Length of the crowd bar
const int crowdHeight = 4;          // Height of the crowd bar
const int crowdStepSize = 10;       // Step size to increment and decrement crowd bar
const int songLength = 20;          // Amount of notes in the song

// Global variables to read from SPI
int oldData[3];             // Old data is stored here
int newData[3];             // New data is read from accel and stored here

// Interrupt handler for timer 2
void __ISR(_TIMER_2_VECTOR, IPL4AUTO) _Timer2Handler(void) {
    int k = 0;
    for (k = 0; k < 3; k++) {
        oldData[k] = newData[k];
    }
    getAccelData(JE_SPI, newData);
    INTClearFlag(INT_T2);
}


void Timer1InitNotesMoveEasy() {
    OpenTimer1(T1_ON | T1_IDLE_CON | T1_SOURCE_INT | T1_PS_1_256 | T1_GATE_OFF, 9061);
    return;
}


void Timer1InitNotesMoveHard() {
    OpenTimer1(T1_ON | T1_IDLE_CON | T1_SOURCE_INT | T1_PS_1_256 | T1_GATE_OFF, 5061);
    return;
}


void Timer2InitReadSPI()
{
    OpenTimer2(T2_ON | T2_IDLE_CON | T2_SOURCE_INT | T2_PS_1_256 | T2_GATE_OFF, 29061);
    INTSetVectorPriority(INT_TIMER_2_VECTOR, INT_PRIORITY_LEVEL_4);
    INTClearFlag(INT_T2);
    INTEnable(INT_T2, INT_ENABLED);
    return;
}


void Timer3InitUpdateCrowd()
{
    OpenTimer3(T3_ON | T3_IDLE_CON | T3_SOURCE_INT | T3_PS_1_256 | T3_GATE_OFF, 49061);
    return;
}


void Timer4InitMenuDebounce()
{
    OpenTimer4(T4_ON | T4_IDLE_CON | T4_SOURCE_INT | T4_PS_1_256 | T4_GATE_OFF, 8624);
    return;
}


void Timer5InitWelcomeMessage()
{
    OpenTimer5(T5_ON | T5_IDLE_CON | T5_SOURCE_INT | T5_PS_1_256 | T5_GATE_OFF, 39061);
    return;
}


int main() {
    
    int i = 0;
    int applause = 0;
    char score[16];
    bool oldDraw = false;
    bool inputWait = true;
    bool playingGame = true;
    note notes[20];
    bool hardMode = false;
    int decrementApplause;
    int notesHit;
    int notesMissed;
    bool DinBar;
    
    // initialize system
    initialize(JE_SPI, 8);
    
    //Configure system for interrupts
    INTConfigureSystem(INT_SYSTEM_CONFIG_MULT_VECTOR);
    INTEnableInterrupts();
    
    OledClearBuffer();
    OledSetCursor(0, 0);
    OledPutString("ECE 2534, Lab 4");
    OledSetCursor(0, 1);
    OledPutString("by Hakeem Bisyir");
    OledSetCursor(0, 2);
    OledPutString("Drum Hero!");
    OledUpdate();
    i = 0;
    TMR5 = 0x0;
    Timer5InitWelcomeMessage();
    while (1) {
        if (INTGetFlag(INT_T5)) {
            INTClearFlag(INT_T5);
            i++;
        };
        if (i >= 6) {
            break;
        };
    };
    INTClearFlag(INT_T5);
    
    OledClearBuffer();
    OledSetCursor(0, 0);
    OledPutString("L - left twist");
    OledSetCursor(0, 1);
    OledPutString("R - right twist");
    OledSetCursor(0, 2);
    OledPutString("D - double tap");
    OledUpdate();
    
    i = 0;
    TMR5 = 0x0;
    Timer5InitWelcomeMessage();
    
    while (1) {
        if (INTGetFlag(INT_T5)) {
            INTClearFlag(INT_T5);
            i++;
        };
        if (i >= 6) {
            break;
        };
    };
    INTClearFlag(INT_T5);
    
    OledClearBuffer();
    
    // initialize timer with correct timing
    Timer2InitReadSPI();
    
    enum States {mainPlay, playGame, endGame, mainDifficulty, difficultyEasy, difficultyHard, test};
    // Possible states of state machine
    enum States systemState = mainPlay; 	 // Initialize system state
    
    Timer4InitMenuDebounce();
    while (1) {
        switch(systemState)
        {
            
        case mainPlay:
            inputWait = true;
            OledClearBuffer();
            OledSetCursor(0, 0);
            OledPutString("Main Menu");
            OledSetCursor(0, 1);
            OledPutString("-> Play Game");
            OledSetCursor(0, 2);
            OledPutString("   Difficulty");
            OledSetCursor(0, 3);
            if (hardMode) {
                OledPutString("Current: Hard");
            }
            else {
                OledPutString("Current: Easy");
            }
            OledUpdate();
            while (inputWait) {
                if (INTGetFlag(INT_T4)) {
                    INTClearFlag(INT_T4);
                    if (getInput1())
                    {
                        systemState = mainDifficulty;
                        inputWait = false;
                    }
                    else if (getInput2())
                    {
                        systemState = playGame;
                        inputWait = false;
                    }
                } // end debounce if
            } // end while
            break;
            
        case playGame:
            
            // Stores the song in array notes
            createSong(notes, 20, hardMode);
            
            // Initialize timer to move notes
            if (hardMode) {
                Timer1InitNotesMoveHard();
            }
            else {
                Timer1InitNotesMoveEasy();
            }
            
            OledClearBuffer();
            playingGame = true;
            applause = 0;
            decrementApplause = 0;
            notesHit = 0;
            notesMissed = 0;
            drawGame(20, 90);
            
            while(playingGame) {

                if (INTGetFlag(INT_T1)) { // doubouncing statement to move notes
                    INTClearFlag(INT_T1);

                    for (i = 0; i < songLength; i++) {
                        notes[i].noteColumn++;
                        if (notes[i].noteColumn > 0 && notes[i].noteColumn < 24) drawNote(notes[i]);
                        if (notes[i].noteColumn == 24 && notes[i].draw == true) {
                            decrementApplause++;
                            notesMissed++;
                        }
                    }

                    if (decrementApplause == 4) { // decrements applause every 4 missed notes
                        applause = applause ? applause - 1 : 0;
                        decrementApplause = 0;
                        fillCrowdBar(90, applause);
                    }
                    OledUpdate();
                    clearBoard();
                } // end if
                
                DinBar = false; // Checks if a D is in the strumming bar
                
                for (i = 0; i < songLength; i++) {
                    if (notes[i].noteColumn >= 19 && notes[i].noteColumn <= 24) { // if a right twist is detected on R note
                        if (notes[i].LRD == 1 && detectRT()) {
                            oldDraw = notes[i].draw;
                            notes[i].draw = false;
                            if (notes[i].draw == false && oldDraw == true) {
                                applause++;
                                fillCrowdBar(90, applause);
                                drawCheck();
                                notesHit++;
                            }
                        }
                        else if (notes[i].LRD == 0 && detectLT()) { // if a left twist is detected on L note
                            oldDraw = notes[i].draw;
                            notes[i].draw = false;
                            if (notes[i].draw == false && oldDraw == true) {
                                applause++;
                                fillCrowdBar(90, applause);
                                drawCheck();
                                notesHit++;
                            }
                        }
                        else if (notes[i].LRD == 2) { // if a D is in the strum bar
                            DinBar = true;
                            if (checkDoubleTap(JE_SPI)) {
                                oldDraw = notes[i].draw;
                                notes[i].draw = false;
                                if (notes[i].draw == false && oldDraw == true) {
                                    applause++;
                                    fillCrowdBar(90, applause);
                                    drawCheck();
                                    notesHit++;
                                }
                            }
                        }
                    }
                }
                if (notes[songLength-1].noteColumn >= 26) playingGame = false; // when last note reaches end of game
                if (DinBar && (getAccelReg(JE_SPI, 0x2A) == 6)) { // if D is in strum bar, start looking for a double tap
                    initTapDetect(JE_SPI);
                }
                else if (!DinBar && (getAccelReg(JE_SPI, 0x2A) == 1)) { // otherwise, reset z axis bit
                    clear2BRegister(JE_SPI);
                }
                getAccelReg(JE_SPI, 0x2A);
                getAccelReg(JE_SPI, 0x2B);
                getAccelReg(JE_SPI, 0x30);
            }
            systemState = endGame;
            break;
        case endGame:
            OledClearBuffer();
            OledSetCursor(0, 0);
            OledPutString("Congratulations");
            sprintf(score, "Applause: %1d", applause);
            OledSetCursor(0, 1);
            OledPutString(score);
            sprintf(score, "Notes Hit: %2d", notesHit);
            OledSetCursor(0, 2);
            OledPutString(score);
            sprintf(score, "Notes Missed: %2d", notesMissed);
            OledSetCursor(0, 3);
            OledPutString(score);
            OledUpdate();
            i = 0;
            TMR5 = 0x0;
            Timer5InitWelcomeMessage();
            while (1) {
                if (INTGetFlag(INT_T5)) {
                    INTClearFlag(INT_T5);
                    i++;
                };
                if (i >= 6) {
                    break;
                };
            };
            OledClearBuffer();
            INTClearFlag(INT_T5);
            systemState = mainPlay;
            break;

        case mainDifficulty:
            inputWait = true;
            OledClearBuffer();
            OledSetCursor(0, 0);
            OledPutString("Main Menu");
            OledSetCursor(0, 1);
            OledPutString("   Play Game");
            OledSetCursor(0, 2);
            OledPutString("-> Difficulty");
            OledSetCursor(0, 3);
            if (hardMode) {
                OledPutString("Current: Hard");
            }
            else {
                OledPutString("Current: Easy");
            }
            OledUpdate();
            while (inputWait) {
                if (INTGetFlag(INT_T4)) {
                    INTClearFlag(INT_T4);
                    if (getInput1())
                    {
                        systemState = mainPlay;
                        inputWait = false;
                    }
                    else if (getInput2())
                    {
                        systemState = difficultyEasy;
                        inputWait = false;
                    }
                } // end debounce if
            } // end while
            break;
            
        case difficultyEasy:
            inputWait = true;
            OledClearBuffer();
            OledSetCursor(0, 0);
            OledPutString("Difficulty");
            OledSetCursor(0, 1);
            OledPutString("-> Easy");
            OledSetCursor(0, 2);
            OledPutString("   Hard");
            OledSetCursor(0, 3);
            if (hardMode) {
                OledPutString("Current: Hard");
            }
            else {
                OledPutString("Current: Easy");
            }
            OledUpdate();
            while (inputWait) {
                if (INTGetFlag(INT_T4)) {
                    INTClearFlag(INT_T4);
                    if (getInput1())
                    {
                        systemState = difficultyHard;
                        inputWait = false;
                    }
                    else if (getInput2())
                    {
                        systemState = mainDifficulty;
                        inputWait = false;
                        hardMode = false;
                    }
                } // end debounce if
            } // end while
            break;
        case difficultyHard:
            inputWait = true;
            OledClearBuffer();
            OledSetCursor(0, 0);
            OledPutString("Difficulty");
            OledSetCursor(0, 1);
            OledPutString("   Easy");
            OledSetCursor(0, 2);
            OledPutString("-> Hard");
            OledSetCursor(0, 3);
            if (hardMode) {
                OledPutString("Current: Hard");
            }
            else {
                OledPutString("Current: Easy");
            }
            OledUpdate();
            while (inputWait) {
                if (INTGetFlag(INT_T4)) {
                    INTClearFlag(INT_T4);
                    if (getInput1())
                    {
                        systemState = difficultyEasy;
                        inputWait = false;
                    }
                    else if (getInput2())
                    {
                        systemState = mainDifficulty;
                        inputWait = false;
                        hardMode = true;
                    }
                } // end debounce if
            } // end while
            break;
        } // end switch
    } // end while
} // end main


/////////////////////////////////////////////////////////////////
// Function:     initialize
//
// Description:  Initializes the Oled, SPI, and accel
//
// Inputs:       chn - SPI channel to initialize
//               ClkDiv - clock divisor to use
//
// Return value: None

void initialize(SpiChannel chn, unsigned int ClkDiv) {
    TRISGSET = 0xc0;     // For BTN1 & BTN2: configure PortG bit for input
    DelayInit();
    OledInit();
    initSPI(chn, ClkDiv);
    initAccelerometer(chn);
    OledClearBuffer();
}

/////////////////////////////////////////////////////////////////
// Function:     initSPI
//
// Description:  Initializes the SPI with correct configuration bits
//
// Inputs:       chn - SPI channel to initialize
//               stcClkDiv - clock divisor to use
//
// Return value: None

void initSPI (SpiChannel chn, unsigned int stcClkDiv) {
    SpiChnOpen(chn,
        SPI_OPEN_MSTEN
        | SPI_OPEN_SMP_END
        | SPI_OPEN_MSSEN
        | SPI_OPEN_CKP_HIGH
        | SPI_OPEN_MODE8
        | SPI_OPEN_ENHBUF,
        stcClkDiv);
}

/////////////////////////////////////////////////////////////////
// Function:     setAccelReg
//
// Description:  Writes a value to a specified accel register
//
// Inputs:       chn - SPI channel to use
//               address - the register address to write to
//               data - the data to write to the register
//
// Return value: None

void setAccelReg(SpiChannel chn, unsigned int address, unsigned int data) {
    SpiChnPutC(chn, address);
    SpiChnPutC(chn, data);
    SpiChnGetC(chn);
    SpiChnGetC(chn);
}

/////////////////////////////////////////////////////////////////
// Function:     getAccelReg
//
// Description:  reads from a specified accelerometer register
//
// Inputs:       chn - SPI channel to use
//               address - accel register to read from
//
// Return value: int that is read from the register

int getAccelReg(SpiChannel chn, unsigned int address) {
    int data;
    unsigned int sendAddress = (1 << 7) | address;
    SpiChnPutC(chn, sendAddress);
    SpiChnPutC(chn, 0x00);
    SpiChnGetC(chn);
    data = SpiChnGetC(chn);
    return data;
}

/////////////////////////////////////////////////////////////////
// Function:     initAccelerometer
//
// Description:  Initializes the accelerometer by writing to certain registers
//
// Inputs:       chn - SPI channel to use
//
// Return value: None

void initAccelerometer(SpiChannel chn) {
    setAccelReg(chn, 0x31, 0x01); // set DATA_FORMAT
    setAccelReg(chn, 0x2D, 0x08); // sets accel for read POWER_CTL
    initTapDetect(chn);
}

/////////////////////////////////////////////////////////////////
// Function:     getAccelData
//
// Description:  Reads from acceleromter registers to gather
//               X, Y, and Z values
//
// Inputs:       chn - SPI channel to use
//               accelData[] - Array to store values
//
// Return value: None

void getAccelData(SpiChannel chn, int accelData[]) {
    unsigned int i, LSB, MSB;
    unsigned int address = 0x32;

    for (i = 0; i < 3; i++) {
        LSB = getAccelReg(chn, address);
        address++;
        MSB = getAccelReg(chn, address);
        address++;
        accelData[i] = (MSB << 8) | LSB;
        // adjusts for negative numbers
        if (accelData[i] & 0x8000) {
            accelData[i] = accelData[i] | 0xFFFF0000;
        }
    }
}

/////////////////////////////////////////////////////////////////
// Function:     detectRT
//
// Description:  returns true if a right twist is detected,
//               otherwise returns false
//
// Inputs:       none
//
// Return value: true if right twist is detected, otherwise false

bool detectRT() {
    bool firstCondition = false; 
    bool secondCondition = false;
    if ((oldData[1] < 40 && oldData[1] > -40) && (oldData[2] < 150 && oldData[2] > 80)) {
        firstCondition = true;
    }
    if ((newData[1] < 150 && newData[1] > 80) && (newData[2] < 40 && newData[2] > -40)) {
        secondCondition = true;
    }
    if (firstCondition && secondCondition) {
        return true;
    }
    return false;
}

/////////////////////////////////////////////////////////////////
// Function:     detectLT
//
// Description:  returns true if a left twist is detected,
//               otherwise returns false
//
// Inputs:       none
//
// Return value: true if left twist is detected, otherwise false

bool detectLT() {
    bool firstCondition = false; 
    bool secondCondition = false;
    if ((oldData[1] < 40 && oldData[1] > -40) && (oldData[2] < 150 && oldData[2] > 80)) {
        firstCondition = true;
    }
    if ((newData[1] > -150 && newData[1] < -80) && (newData[2] < 40 && newData[2] > -40)) {
        secondCondition = true;
    }
    if (firstCondition && secondCondition) {
        return true;
    }
    return false;
}

/////////////////////////////////////////////////////////////////
// Function:     drawGame
//
// Description:  draws the note bar and crowd fill bar
//
// Inputs:       noteCenter - center X coordinate of the note bar
//               crowdCenter - center X coordinate of the crowd bar
//
// Return value: None

void drawGame(int noteCenter, int crowdCenter) {
    drawMid(noteCenter);
    drawCrowdBar(crowdCenter);
    OledUpdate();
}

/////////////////////////////////////////////////////////////////
// Function:     clearBoard
//
// Description:  Clears the notes and check mark off the board
//
// Inputs:       none
//
// Return value: None

void clearBoard() {
    int clearX = 0, clearY = 0;
    OledSetDrawColor(0);
    for (clearY = crowdHeight + 1; clearY < 31 - noteLength; clearY++)  {
        for (clearX = 0; clearX < 128; clearX++) {
            OledMoveTo(clearX, clearY);
            OledDrawPixel();
        }
    }
    OledMoveTo(120, 29);
    OledDrawPixel();
    OledMoveTo(121, 30);
    OledDrawPixel();
    OledMoveTo(122, 31);
    OledDrawPixel();
    OledMoveTo(123, 30);
    OledDrawPixel();
    OledMoveTo(124, 29);
    OledDrawPixel();
    OledMoveTo(125, 28);
    OledDrawPixel();
    OledMoveTo(126, 27);
    OledDrawPixel();
    OledMoveTo(127, 26);
    OledDrawPixel();
    OledSetDrawColor(1);
}

/////////////////////////////////////////////////////////////////
// Function:     drawMid
//
// Description:  draws vertical note bar
//
// Inputs:       center - center X coordinate of note bar
//
// Return value: None

void drawMid(int center) {
    int j=0;
    int leftEnd = center - noteWidth/2;
    int rightEnd = center + noteWidth/2;
    OledSetDrawColor(1);
    for (j = 0; leftEnd + j <= rightEnd; j++) {
        OledMoveTo(leftEnd + j, 0);
        OledDrawPixel();
        OledMoveTo(leftEnd + j, 31);
        OledDrawPixel();
    }
    for (j = 0; j <= noteLength; j++) {
        OledMoveTo(leftEnd, j);
        OledDrawPixel();
        OledMoveTo(rightEnd, j);
        OledDrawPixel();
        OledMoveTo(rightEnd, 31-j);
        OledDrawPixel();
        OledMoveTo(leftEnd, 31-j);
        OledDrawPixel();
    }
}

/////////////////////////////////////////////////////////////////
// Function:     drawCrowdBar
//
// Description:  draws the crowd fill bar on the board
//
// Inputs:       center - center X coordinate of the crowd fill bar
//
// Return value: None

void drawCrowdBar(int center) {
    int leftEnd = center - crowdLength/2;
    int rightEnd = center + crowdLength/2;
    int j = 0;
    OledSetDrawColor(1);
    for (j = 0; leftEnd + j <= rightEnd; j++) {
        OledMoveTo(leftEnd + j, 0);
        OledDrawPixel();
        OledMoveTo(leftEnd + j, crowdHeight);
        OledDrawPixel();
    }
    for (j = 0; j <= crowdHeight; j++) {
        OledMoveTo(leftEnd, j);
        OledDrawPixel();
        OledMoveTo(rightEnd, j);
        OledDrawPixel();
    }
}

/////////////////////////////////////////////////////////////////
// Function:     fillCrowdBar
//
// Description:  fills the crowd bar to the appropriate level
//
// Inputs:       center - center X coordinate of crowd fill bar
//               applause - number between 1 and crowdStepSize that
//               specifies how far to fill the meter
//
// Return value: None

void fillCrowdBar(int center, int applause) {
    int leftEnd = center - crowdLength/2;
    int rightEnd = center + crowdLength/2;
    int drawPos;
    if (applause >= 9) applause = 8;
    else if (applause < 0) applause = 0;
    OledSetDrawColor(0);
    for (drawPos = leftEnd + 1; drawPos < rightEnd; drawPos++) {
        OledMoveTo(drawPos, 1);
        OledDrawPixel();
        OledMoveTo(drawPos, 2);
        OledDrawPixel();
        OledMoveTo(drawPos, 3);
        OledDrawPixel();
    }
    OledSetDrawColor(1);
    for (drawPos = leftEnd + 1; drawPos < (applause * crowdStepSize) + leftEnd; drawPos++) {
        OledMoveTo(drawPos, 1);
        OledDrawPixel();
        OledMoveTo(drawPos, 2);
        OledDrawPixel();
        OledMoveTo(drawPos, 3);
        OledDrawPixel();
    }
}

/////////////////////////////////////////////////////////////////
// Function:     drawNote
//
// Description:  Draws the struct type note on the board
//
// Inputs:       toDraw - the note to draw
//
// Return value: None

void drawNote(note toDraw) {
    if (toDraw.noteColumn >= 0 && toDraw.LRD == 0 && toDraw.draw == true) {
        drawL(toDraw.noteColumn, toDraw.noteRow);
    }
    else if (toDraw.noteColumn >= 0 && toDraw.LRD == 1 && toDraw.draw == true) {
        drawR(toDraw.noteColumn, toDraw.noteRow);
    }
    else if (toDraw.noteColumn >=0 && toDraw.LRD == 2 && toDraw.draw == true) {
        drawD(toDraw.noteColumn, toDraw.noteRow);
    }
    OledUpdate();
}

/////////////////////////////////////////////////////////////////
// Function:     drawL
//
// Description:  draws an L on a pixel basis
//
// Inputs:       x - x coordinate to draw L
//               y - y coordinate to draw L
//
// Return value: None

void drawL(int x, int y) {
    int toDrawX = 127 - (5 * (x + 1));
    int toDrawY = (y + 1) * 6 + 3;
    int j = 0;
    OledSetDrawColor(1);
    for (j = 0; j < 5; j++) {
        OledMoveTo(toDrawX, toDrawY);
        OledDrawPixel();
        toDrawY++;
    }
    for (j = 0; j < 3; j++) {
        OledMoveTo(toDrawX, toDrawY);
        OledDrawPixel();
        toDrawX++;
    }
}

/////////////////////////////////////////////////////////////////
// Function:     drawR
//
// Description:  draws an R on a pixel basis
//
// Inputs:       x - x coordinate to draw R
//               y - y coordinate to draw R
//
// Return value: None

void drawR(int x, int y) {
    int toDrawX = 127 - (5 * (x + 1));
    int toDrawY = (y + 1) * 6 + 3;
    int j = 0;
    OledSetDrawColor(1);
    for (j = 0; j < 5; j++) {
        OledMoveTo(toDrawX, toDrawY);
        OledDrawPixel();
        toDrawY++;
    }
    toDrawX = 127 - (5 * (x + 1));
    toDrawY = (y + 1) * 6 + 3;
    OledMoveTo(toDrawX + 1, toDrawY);
    OledDrawPixel();
    OledMoveTo(toDrawX + 2, toDrawY + 1);
    OledDrawPixel();
    OledMoveTo(toDrawX + 1, toDrawY + 2);
    OledDrawPixel();
    OledMoveTo(toDrawX + 1, toDrawY + 3);
    OledDrawPixel();
    OledMoveTo(toDrawX + 2, toDrawY + 4);
    OledDrawPixel();
}

/////////////////////////////////////////////////////////////////
// Function:     drawD
//
// Description:  draws a D on a pixel basis
//
// Inputs:       x - x coordinate to draw D
//               y - y coordinate to draw D
//
// Return value: None

void drawD(int x, int y) {
    int toDrawX = 127 - (5 * (x + 1));
    int toDrawY = (y + 1) * 6 + 3;
    int j = 0;
    OledSetDrawColor(1);
    for (j = 0; j < 5; j++) {
        OledMoveTo(toDrawX, toDrawY);
        OledDrawPixel();
        toDrawY++;
    }
    toDrawX = 127 - (5 * (x + 1));
    toDrawY = (y + 1) * 6 + 3;
    OledMoveTo(toDrawX + 1, toDrawY);
    OledDrawPixel();
    OledMoveTo(toDrawX + 2, toDrawY + 1);
    OledDrawPixel();
    OledMoveTo(toDrawX + 3, toDrawY + 2);
    OledDrawPixel();
    OledMoveTo(toDrawX + 2, toDrawY + 3);
    OledDrawPixel();
    OledMoveTo(toDrawX + 1, toDrawY + 4);
    OledDrawPixel();
}

/////////////////////////////////////////////////////////////////
// Function:     drawCheck
//
// Description:  draws a check on a pixel basis in the bottom right corner
//
// Inputs:       none
//
// Return value: None

void drawCheck() {
    OledSetDrawColor(1);
    OledMoveTo(120, 29);
    OledDrawPixel();
    OledMoveTo(121, 30);
    OledDrawPixel();
    OledMoveTo(122, 31);
    OledDrawPixel();
    OledMoveTo(123, 30);
    OledDrawPixel();
    OledMoveTo(124, 29);
    OledDrawPixel();
    OledMoveTo(125, 28);
    OledDrawPixel();
    OledMoveTo(126, 27);
    OledDrawPixel();
    OledMoveTo(127, 26);
    OledDrawPixel();
}

/////////////////////////////////////////////////////////////////
// Function:     createSong
//
// Description:  stores the song into a given array
//
// Inputs:       songToStore - the array to store the notes in
//               size - size of array of notes
//               hard - true if hard mode is enabled, otherwise false
//
// Return value: None

void createSong(note songToStore[], int size, bool hard) {
    int j = 0, r = 0;
    int row, column, type;
    for (j = 0; j < size; j++) {
        r = ReadTimer2() + ReadTimer5() + newData[2];
        row = r % 3;
        r = ReadTimer2() + ReadTimer5() + newData[0];
        if (hard) {
            column = j ? column - (r % 6) - 3 : 0;
        }
        else {
            column = j ? column - (r % 6) - 6 : 0;
        }
        r = ReadTimer2() + ReadTimer5() + newData[1];
        type = r % 3; 
        
        songToStore[j].noteRow = row;
        songToStore[j].noteColumn = column;
        songToStore[j].LRD = type;
        songToStore[j].draw = true;
    }
}


/////////////////////////////////////////////////////////////////
// Function:    getInput1
//
// Description: Perform a nonblocking check to see if BTN1 has been pressed
//
// Inputs:      None
//
// Returns:     TRUE if 0-to-1 transition of BTN1 is detected;
//                otherwise return FALSE

bool getInput1()
{
    enum Button1Position {UP, DOWN}; // Possible states of BTN1
    
    static enum Button1Position button1CurrentPosition = UP;  // BTN1 current state
    static enum Button1Position button1PreviousPosition = UP; // BTN1 previous state
    // Reminder - "static" variables retain their values from one call to the next.
    
    button1PreviousPosition = button1CurrentPosition;

    // Read BTN1
    if(PORTG & 0x40)                                
    {
        button1CurrentPosition = DOWN;
    } 
	else
    {
        button1CurrentPosition = UP;
    } 
    
    if((button1CurrentPosition == DOWN) && (button1PreviousPosition == UP))
    {
        return TRUE; // 0-to-1 transition has been detected
    }
    return FALSE;    // 0-to-1 transition not detected
}

/////////////////////////////////////////////////////////////////
// Function:    getInput2
//
// Description: Perform a nonblocking check to see if BTN2 has been pressed
//
// Inputs:      None
//
// Returns:     TRUE if 0-to-1 transition of BTN1 is detected;
//                otherwise return FALSE

bool getInput2()
{
    enum Button2Position {UP, DOWN}; // Possible states of BTN2
    
    static enum Button2Position button2CurrentPosition = UP;  // BTN2 current state
    static enum Button2Position button2PreviousPosition = UP; // BTN2 previous state
    // Reminder - "static" variables retain their values from one call to the next.
    
    button2PreviousPosition = button2CurrentPosition;

    // Read BTN2
    if(PORTG & 0x80)                                
    {
        button2CurrentPosition = DOWN;
    } 
	else
    {
        button2CurrentPosition = UP;
    } 
    
    if((button2CurrentPosition == DOWN) && (button2PreviousPosition == UP))
    {
        return TRUE; // 0-to-1 transition has been detected
    }
    return FALSE;    // 0-to-1 transition not detected
}

/////////////////////////////////////////////////////////////////
// Function:     initTapDetect
//
// Description:  initializes accel to look for double tap along z axis
//
// Inputs:       chn - SPI channel being used
//
// Return value: None

void initTapDetect(SpiChannel chn) 
{
    int doubleTapDetect = (1 << 5);
    int threshold = (1 << 5);
    int tapDuration = 0x40;
    int latency = 0x40;
    int window = 0x80;
    int zAxis = (1 << 0);
    setAccelReg(chn, 0x2E, 0x00);
    setAccelReg(chn, 0x1D, threshold);
    setAccelReg(chn, 0x21, tapDuration);
    setAccelReg(chn, 0x22, latency);
    setAccelReg(chn, 0x23, window);
    setAccelReg(chn, 0x2A, zAxis);
    setAccelReg(chn, 0x2E, doubleTapDetect);
}

/////////////////////////////////////////////////////////////////
// Function:     clear2BRegister
//
// Description:  initializes accel to look for movement across x and y
//               axes to clear z axis bit
//
// Inputs:       chn - SPI channel being used
//
// Return value: None

void clear2BRegister(SpiChannel chn)
{
    int singleTapDetect = (1 << 6);
    int doubleTapDetect = (1 << 5);
    int threshold = (1 << 3);
    int tapDuration = 0x40;
    int latency = 0x40;
    int window = 0x80;
    int zAxis = (1 << 0);
    int yAxis = (1 << 1);
    int xAxis = (1 << 2);
    setAccelReg(chn, 0x2E, 0x00);
    setAccelReg(chn, 0x1D, threshold);
    setAccelReg(chn, 0x21, tapDuration);
    setAccelReg(chn, 0x22, latency);
    setAccelReg(chn, 0x23, window);
    setAccelReg(chn, 0x2A, (yAxis | xAxis));
    setAccelReg(chn, 0x2E, singleTapDetect);
}

/////////////////////////////////////////////////////////////////
// Function:     checkDoubleTap
//
// Description:  checks for the z axis bit in address 0x2B
//
// Inputs:       chn - SPI channel being used
//
// Return value: true if z axis bit is set to 1, otherwise false

bool checkDoubleTap(SpiChannel chn) 
{
    int address0x2B;
    address0x2B = getAccelReg(chn, 0x2B);
    if (address0x2B & (1 << 0)) {
        return true;
    }
    return false;
}