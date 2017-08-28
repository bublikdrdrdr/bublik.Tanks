#include <targets/AT91SAM7.h>
#include "usart.h"
#include "lcd.h"
#define LCD_BACKLIGHT PIOB_SODR_P20
#define AUDIO_OUT PIOB_SODR_P19
#define SW_1 PIOB_SODR_P24
#define SW_2 PIOB_SODR_P25
//TODO: joystick SW
#define JL PIOA_SODR_P7//wyjœcia przycisków joystick'a
#define JD PIOA_SODR_P8
#define JU PIOA_SODR_P9
#define JR PIOA_SODR_P14
#define JC PIOA_SODR_P15

__attribute__((section(".fast"))); // konfiguracja delay
void delay(int n) { //funkcja zatrzymania, opisana w poprzednich sprawozdaniach
  volatile int i;
  for (i = 400 * n; i > 0; i--) {
    __asm__("nop");
  }
}

int currentRound;
int firstWons;
int secondWons;
int quitFromTank1 = 6;
int quitFromTank2 = 6;
int roundEnd = 0;

//5-LBshoot, 1-right, 2-down, 3-left, 4-up, 0-nothing
int myButton;
int enemyButton;

int deltaMove = 2;//pixels per one step
int deltaMoveBullet = 3;//pixels per one bullet step
int screenX=0, screenY=0, screenEndX=131, screenEndY=131;
int wallsDrawed = 0;
//can't be bigger than tank size cause of this: 
/*iterations:
		  1		   2		  3			  4
####				 ####
#####     -        -          -  #####    -
####				 ####
bullet (-) flies through enemy tank and doesn't hit it
*/
int wallCount;
//int walls [] = {50,40,60,90, 0,0,132,4,0,4,4,132,4,128,132,132,128,4,132,128};//wall start XY, end XY, next wall
int walls [] = {-1,-1,132,0,   -3,-1,-1,132  ,0,132,132,133,  130,0,132,133,
 20,30,30,40,
 30,20,40,40,
  
  90,0,100,40,
  
  20,60,50,70,
  
  80,60,110,70,
  
  90,90,110,100,
  90,100,100,110
  
  ,30,90,40,132};//wall start XY, end XY, next wall
//players XY, bullets XY, players and bullets directions (2,4,6,8 (like keypad))
//bullets null state = position<0
int p1x, p1y, p2x, p2y, b1x, b1y, b2x, b2y, p1D, p2D, b1D, b2D;
int Pp1x, Pp1y, Pp2x, Pp2y, Pb1x, Pb1y, Pb2x, Pb2y;//previous

int tankSize = 10;//TODO: change
int bulletSize = 2;

int globalScoreY = 0;//you
int globalScoreE = 0;//enemy

int shootSoundDelay = 2;
void shootSound(){
for (int i = 0; i < 20; i++)
{
  PIOB_SODR|=AUDIO_OUT;
  delay(shootSoundDelay);
  PIOB_CODR|=AUDIO_OUT;
  delay(shootSoundDelay);
  }
}

void boomSound(){
int f = 25;
  for (int i = 0; i < 30; i++)
  {
  f+=5;
  PIOB_SODR|=AUDIO_OUT;
  delay(f);
  PIOB_CODR|=AUDIO_OUT;
  delay(f);
  }
}

void copyPrevious(){
  Pp1x = p1x;
  Pp1y = p1y;
  Pp2x = p2x;
  Pp2y = p2y;
  Pb1x = b1x;
  Pb1y = b1y;
  Pb2x = b2x;
  Pb2y = b2y;
}

void readButtons(){
  //my
  if ((PIOB_PDSR & SW_1)==0){
    myButton = 5;
  } else if ((PIOA_PDSR & JR)==0){
    myButton = 1;
    } else if ((PIOA_PDSR & JD)==0){
    myButton = 2;
    } else if ((PIOA_PDSR & JL)==0){
    myButton = 3;
    } else if ((PIOA_PDSR & JU)==0){
    myButton = 4;
  } else{
    myButton=0;
  }

delay(100);
  //send
  write_char_USART0(myButton);

  //enemy
  enemyButton = (int)read_char_USART0();
}

int checkWallsCollision(int x, int y)
{
    int i;
	for (i = 0; i < wallCount; i++){
		int xx1 = walls[i * 4 + 0];
		int xx2 = walls[i * 4 + 2];
		int yy1 = walls[i * 4 + 1];
		int yy2 = walls[i * 4 + 3];
		//check tank&wall collision
		/*if (((((xx1 >= x) && (xx1 <= (x + tankSize))) 
                || ((xx2 >= x) && (xx2 <= (x + tankSize)))) 
                && (((yy1 >= y) && (yy1 <= (y + tankSize))) 
                || ((yy2 >= y) && (yy2 <= (y + tankSize))))))*/
                if (
                  ((x<=xx2)&&(x+tankSize>=xx1))
                  &&
                  ((y<=yy2)&&(y+tankSize>=yy1))
                )
		{ //tank is inside of wall
			return 0;
		}
		//check tank&screen border collision (optional) //use walls as a world border
	}
	return 1;
}
int checkBulletWallsCollision(int x, int y)
{
    int i;
	for (i = 0; i < wallCount; i++){
		int xx1 = walls[i * 4 + 0];
		int xx2 = walls[i * 4 + 2];
		int yy1 = walls[i * 4 + 1];
		int yy2 = walls[i * 4 + 3];
                if (
                  ((x<=xx2)&&(x+2>=xx1))
                  &&
                  ((y<=yy2)&&(y+2>=yy1))
                )
		{ //tank is inside of wall
			return 0;
		}
		//check tank&screen border collision (optional) //use walls as a world border
	}
	return 1;
}

int checkTanksCollision(int x, int y, int ex, int ey){
  if (((x<=ex+tankSize)&&(x+tankSize>=ex))&&((y<=ey+tankSize)&&(y+tankSize>=ey)))
  return 0; else return 1;
                
}


int checkTankHit(int bX, int bY, int tX, int tY){
	if ((bX+2>=tX) && (bX<=tX+tankSize) && (bY+2>=tY) && (bY<=tY+tankSize))
        return 1; else return 0;
}

int checkBulletOutOfScreen(int bX, int bY){
	if ( !((bX>=screenX)&&(bX<=screenEndX)&&(bY>=screenY)&&(bY<=screenEndY)))
         return 1; else return 0;
}

void setLastTankDirection(int button, int my){
	if (my==1){
		switch(button){
			case 1: p1D = 6; break;
			case 2: p1D = 2; break;
			case 3: p1D = 4; break;
			case 4: p1D = 8; break;
		}
	} else{
		switch(button){
			case 1: p2D = 6; break;
			case 2: p2D = 2; break;
			case 3: p2D = 4; break;
			case 4: p2D = 8; break;
		}
	}
}


void logic(){
  copyPrevious();
  setLastTankDirection(myButton, 1);
  setLastTankDirection(enemyButton, 0);
  int i;
  //check my tank
  if (!((myButton==0)||(myButton==5)))
  { 
    //calculate new temp xy
    int nx = p1x;
    int ny = p1y;
	switch(p1D){
		case 2: ny+=deltaMove; break;
		case 4: nx-=deltaMove; break;
		case 6: nx+=deltaMove; break;
		case 8: ny-=deltaMove; break;
	}
    
	int ok = checkWallsCollision(nx, ny);
        int ok2 = checkTanksCollision(nx,ny, p2x, p2y);
	if ((ok==1) && (ok2==1))
	{
		p1x = nx;
		p1y = ny;
	}
  } else if (myButton==5){
    if ((b1x<0) || (b1y<0))
    {//we can shoot
    shootSound();
    quitFromTank1 = 0;
      b1D = p1D;
      b1x = p1x+(tankSize/2)-1;
      b1y = p1y+(tankSize/2)-1;
    }
  }
  
  //check enemy tank
  if (!((enemyButton == 0) || (enemyButton == 5)))
  {
	  //calculate new temp xy
		int nx = p2x;
		int ny = p2y;
		switch(p2D){
		case 2: ny+=deltaMove; break;
		case 4: nx-=deltaMove; break;
		case 6: nx+=deltaMove; break;
		case 8: ny-=deltaMove; break;
	}

	  int ok = checkWallsCollision(nx, ny);
	  int ok2 = checkTanksCollision(nx,ny, p1x, p1y);
          if ((ok==1) && (ok2==1))
          {
		p2x = nx;
		p2y = ny;
          }
  }  else if (enemyButton==5){
    if ((b2x<0) || (b2y<0))
    {//we can shoot
    shootSound();
    quitFromTank2 = 0;
      b2D = p2D;
      b2x = p2x+(tankSize/2)-1;
      b2y = p2y+(tankSize/2)-1;
    }
  }

  
	int hitsFirst = 0, hitsSecond = 0;
  //check my bullet
  if ((b1x>=0) && (b1y>=0))
  {//if bullet are on the screen
  //increment bullet position
            int nx = b1x;
          int ny = b1y;
	switch(b1D){
		  case 2: ny+=deltaMoveBullet; break;
		case 4: nx-=deltaMoveBullet; break;
		case 6: nx+=deltaMoveBullet; break;
		case 8: ny-=deltaMoveBullet; break;
	  }
    
	int ok = checkBulletWallsCollision(nx, ny);
	if (ok==1)
	{
		b1x = nx;
		b1y = ny;
	}else{
          b1x = -1;
	  b1y = -1;
        }  
  }
  
  if (checkBulletOutOfScreen(b1x, b1y)==1){
	  b1x = -1;
	  b1y = -1;
  } else{
	  if (checkTankHit(b1x, b1y, p2x,p2y)==1)
	  {//first's tank bullet hits second tank
		  hitsSecond = 1;
		  b1x = -1;
		  b1y = -1;
                  boomSound();
	  }
  }

  //check enemy bullet
  if ((b2x>=0) && (b2y>=0))
  {//if bullet are on the screen
	  
	  //increment bullet position
            int nx = b2x;
          int ny = b2y;
	switch(b2D){
		 case 2: ny+=deltaMoveBullet; break;
		case 4: nx-=deltaMoveBullet; break;
		case 6: nx+=deltaMoveBullet; break;
		case 8: ny-=deltaMoveBullet; break;
	  }
    
	int ok = checkBulletWallsCollision(nx, ny);
	if (ok==1)
	{
		b2x = nx;
		b2y = ny;
	} else{
          b2x = -1;
	  b2y = -1;
        }
  }
  
  if (checkBulletOutOfScreen(b2x, b2y)==1){
	  b2x = -1;
	  b2y = -1;
  } else{
	  if (checkTankHit(b2x, b2y, p1x,p1y)==1)
	  {//first's tank bullet hits second tank
		  hitsFirst = 1;
		  b2x = -1;
		  b2y = -1;
                  boomSound();
	  }
  }
  
  if ((hitsFirst==1) || (hitsSecond==1)){
	  roundEnd = 1;
	  if (hitsFirst==1) secondWons++;
	  if (hitsSecond==1) firstWons++;
  }
}


int bulletColor = WHITE;
int tankColor = WHITE;
int gunSize = 2;
int firstTimeTanksDrawed = 0;
void draw(){
//draw walls
  if (wallsDrawed == 0){
    wallsDrawed = 1;
    for (int i = 0; i < wallCount; i++){
		int xx1 = walls[i * 4 + 0];
		int xx2 = walls[i * 4 + 2];
		int yy1 = walls[i * 4 + 1];
		int yy2 = walls[i * 4 + 3];
                LCDSetRect(yy1,xx1, yy2, xx2,  1, GREEN);    
	}
  }
  //clear old positions 
  if ((!((myButton==0) || (myButton==5))) || (firstTimeTanksDrawed<2) || (quitFromTank1 < 4)){
   if(quitFromTank1<5)
    quitFromTank1++;
    if(firstTimeTanksDrawed<3)
  firstTimeTanksDrawed++;
  LCDSetRect(Pp1y,Pp1x, Pp1y+tankSize, Pp1x+tankSize,  1, BLACK);  
 
  //draw new positions
  switch(p1D){
    case 2: 
       LCDSetRect(p1y,p1x, p1y+tankSize-gunSize, p1x+tankSize,  1, tankColor); //TODO:red/blue tanks
       LCDSetRect(p1y+tankSize-gunSize,p1x+(tankSize/2)-gunSize/2, p1y+tankSize,p1x+(tankSize/2)+gunSize/2,  1, tankColor);
      // LCDSetPixel(p1y+tankSize, p1x+(tankSize/2), tankColor);
    break;
    case 4: 
       LCDSetRect(p1y,p1x+gunSize, p1y+tankSize, p1x+tankSize,  1, tankColor); //TODO:red/blue tanks
       LCDSetRect(p1y+tankSize/2-gunSize/2,p1x, p1y+tankSize/2+gunSize/2,p1x+gunSize,  1, tankColor);
    break;
    case 6: 
       LCDSetRect(p1y,p1x, p1y+tankSize, p1x+tankSize-gunSize,  1, tankColor); //TODO:red/blue tanks
       LCDSetRect(p1y+tankSize/2-gunSize/2,p1x+tankSize-gunSize, p1y+tankSize/2+gunSize/2,p1x+tankSize,  1, tankColor);
    break;
    case 8: 
       LCDSetRect(p1y+gunSize,p1x, p1y+tankSize, p1x+tankSize,  1, tankColor); //TODO:red/blue tanks
       LCDSetRect(p1y,p1x+(tankSize/2)-gunSize/2, p1y+gunSize,p1x+(tankSize/2)+gunSize/2,  1, tankColor);
    break;
  }}


  
    if ((!((enemyButton==0) || (enemyButton==5))) || (firstTimeTanksDrawed<2) || (quitFromTank2 < 4)){
    if(quitFromTank2<5)
    quitFromTank2++;
    if(firstTimeTanksDrawed<3)
  firstTimeTanksDrawed++;
  LCDSetRect(Pp2y,Pp2x, Pp2y+tankSize, Pp2x+tankSize,  1, BLACK); 
 switch(p2D){
    case 2: 
       LCDSetRect(p2y,p2x, p2y+tankSize-gunSize, p2x+tankSize,  1, tankColor); //TODO:red/blue tanks
       LCDSetRect(p2y+tankSize-gunSize,p2x+(tankSize/2)-gunSize/2, p2y+tankSize,p2x+(tankSize/2)+gunSize/2,  1, tankColor);
      // LCDSetPixel(p2y+tankSize, p2x+(tankSize/2), tankColor);
    break;
    case 4: 
       LCDSetRect(p2y,p2x+gunSize, p2y+tankSize, p2x+tankSize,  1, tankColor); //TODO:red/blue tanks
       LCDSetRect(p2y+tankSize/2-gunSize/2,p2x, p2y+tankSize/2+gunSize/2,p2x+gunSize,  1, tankColor);
    break;
    case 6: 
       LCDSetRect(p2y,p2x, p2y+tankSize, p2x+tankSize-gunSize,  1, tankColor); //TODO:red/blue tanks
       LCDSetRect(p2y+tankSize/2-gunSize/2,p2x+tankSize-gunSize, p2y+tankSize/2+gunSize/2,p2x+tankSize,  1, tankColor);
    break;
    case 8: 
       LCDSetRect(p2y+gunSize,p2x, p2y+tankSize, p2x+tankSize,  1, tankColor); //TODO:red/blue tanks
       LCDSetRect(p2y,p2x+(tankSize/2)-gunSize/2, p2y+gunSize,p2x+(tankSize/2)+gunSize/2,  1, tankColor);
    break;
  }}

  //clear old bullet
  if(Pb1x!=-1){
  LCDSetRect(Pb1y,Pb1x, Pb1y+2, Pb1x+2,  1, BLACK);
  LCDSetRect(b1y,b1x, b1y+2, b1x+2,  1, bulletColor);
  }
  if(Pb2x!=-1){
  LCDSetRect(Pb2y,Pb2x, Pb2y+2, Pb2x+2,  1, BLACK);
  LCDSetRect(b2y,b2x, b2y+2, b2x+2,  1, bulletColor);
  }
}

void setPositions(int n){
  if (n==1){
    p1x = 10;
    p1y = 10;
    p2x = 115;
    p2y = 115;
    p1D = 2;
    p2D = 8;
  } else{
    p1x = 115;
    p1y = 115;
    p2x = 10;
    p2y = 10;
    p1D = 8;
    p2D = 2;
  }
  b1x = -1;
  b2x = -1;
  b1y = -1;
  b2y = -1;

}


void round(){
firstTimeTanksDrawed = 0;
draw();
  while(roundEnd==0){
    readButtons();
    logic();
    draw();
  }
  roundEnd = 0;
}

void afterRoundClear(){
  LCDSetRect(p1y,Pp1x, p1y+tankSize, p1x+tankSize,  1, BLACK);  
  LCDSetRect(p2y,Pp2x, p2y+tankSize, p2x+tankSize,  1, BLACK);
  LCDSetRect(b1y,b1x, b1y+2, b1x+2,  1, BLACK);
  LCDSetRect(b2y,b2x, b2y+2, b2x+2,  1, BLACK);
  firstTimeTanksDrawed = 0;
  LCDPutStr("      ROUND     ", 60, 0, LARGE, WHITE, BLACK);
  delay(8000);
  LCDPutStr("      ROUND     ", 60, 0, LARGE, BLACK, BLACK);
  wallsDrawed = 0;
}

void game(int rounds, int n){
  currentRound = 0;
  firstWons = 0;
  secondWons = 0;
  while(currentRound<rounds){
  //one round 
    setPositions(n);
    round();
    afterRoundClear();
  ///one round
    currentRound++;
  }
  globalScoreY+=firstWons;
  globalScoreE+=secondWons;
}

int connect(){
char ch;
while(1){
  ch = read_char_USART0_nonstop();
  if(ch!=0){
    return 2;
  }

  if ((PIOA_PDSR & JC)==0){
  write_char_USART0('s');
  return 1;
  }
  
  //if right button - cancel
  if ((PIOB_PDSR & SW_2)==0){
  return 0;
  }
}
}

void initAll(){
  PMC_PCER=PMC_PCER_PIOB |PMC_PCER_PIOA;
  PIOB_OER=LCD_BACKLIGHT | AUDIO_OUT;
  PIOB_PER=LCD_BACKLIGHT | AUDIO_OUT;
  InitLCD();
  LCDSettings();
  LCDClearScreen();
  InitUSART0();
  InitUSART1();

  wallCount = sizeof(walls)/(sizeof(walls[0])*4);
}

void fastClear(){
	LCDPutStr("                 ", 62, 0, LARGE, BLACK, BLACK);
	LCDPutStr("                 ", 82, 0, LARGE, BLACK, BLACK);
	LCDPutStr("                 ", 102, 0, LARGE, BLACK, BLACK);
}

void showScore(){
fastClear();
char* outstr;
outstr = (char*)malloc(20); 
LCDPutStr("GAME OVER", 30, 2, SMALL, WHITE, BLACK);
sprintf(outstr,"your score: %d\0", firstWons);
LCDPutStr(outstr, 60, 2, SMALL, WHITE, BLACK);
sprintf(outstr,"enemy's score: %d\0", secondWons);
LCDPutStr(outstr, 90, 2, SMALL, WHITE, BLACK);
delay(2000);
wallsDrawed = 0;
}

void showStartLabels(int color){
  LCDPutStr("World of Tanks", 10, 8, LARGE, color, BLACK);
  LCDPutStr("(beta)", 26, 85, SMALL, color, BLACK);
  LCDPutStr(" by Vadym Borys", 45, 2, MEDIUM, color, BLACK);
  LCDPutStr("and Roman Baida", 55, 2, MEDIUM, color, BLACK);
  LCDPutStr("press center to start", 80, 2, SMALL, color, BLACK);

  LCDPutStr("joystick - move", 100, 20, SMALL, color, BLACK);
  LCDPutStr("left button - shoot", 109, 7, SMALL, color, BLACK);
}

void showStart(){
showStartLabels(WHITE);
}

void hideStart(){
 showStartLabels(BLACK);
}

//8-up, 2-down, 6-right, 4-left, 5 - center, SW1, SW2
int oldButton = 0;
int getButton(){
        while(1)
        {
	if (((PIOA_PDSR & JC)==0)||((PIOB_PDSR & SW_1)==0)||((PIOB_PDSR & SW_2)==0)){
		if (oldButton==0){
			oldButton = 5;
			return 5;
		}
	}
	if ((PIOA_PDSR & JR)==0){
		if (oldButton==0){
			oldButton = 6;
			return 6;
		}
	}
	if ((PIOA_PDSR & JL)==0){
		if (oldButton==0){
			oldButton = 4;
			return 4;
		}
	}
	if ((PIOA_PDSR & JU)==0){
		if (oldButton==0){
			oldButton = 8;
			return 8;
		}
	}
	if ((PIOA_PDSR & JD)==0){
		if (oldButton==0){
			oldButton = 2;
			return 2;
		}
	}
        oldButton=0;
	}
}

int menuPictureOK = 0;
//0-start game, 1 - show score, 2 - choose rounds , 3 - about, 4 - exit
int showMenu(){
	char *p[]={"START","SHOW SCORE", "CHOOSE ROUNDS", "ABOUT", "EXIT"};
        LCDClearScreen();
		LCDWrite130x130bmp();
	int menuPos = 0;
	//LCDPutStr("START", 109, 7, LARGE, WHITE, BLACK);
	while(1){
		fastClear();
		if (menuPos>0){
			//draw -1
			LCDPutStr(p[menuPos-1], 62,(50-(sizeof(p[menuPos-1])*8)), LARGE, BROWN, BLACK);
		}
		LCDPutStr(p[menuPos], 82, (50-(sizeof(p[menuPos])*8)), LARGE, WHITE, BLACK);
		if (menuPos<4){
			//draw +1
			LCDPutStr(p[menuPos+1], 102,(50-(sizeof(p[menuPos+1])*8)), LARGE, BROWN, BLACK);
		}
		
		
		int btn = getButton();
		if (btn==5)
		{
			return menuPos;
		} else if (btn==8){ 
			if (menuPos>0) menuPos--;
		} else if (btn==2){ 
			if (menuPos<4) menuPos++;
		}
	}
}

void showAbout(){
	menuPictureOK=0;
        LCDClearScreen();
	LCDWrite130x130bmp2();
        getButton();//wait for button press
}

int minR = 1;
int maxR = 10;
int chooseRoundsCount(int old){
	char* outstr;
        fastClear();
	outstr = (char*)malloc(3); 
	LCDPutStr("-", 80, 38, LARGE, WHITE, BLACK);
	LCDPutStr("+", 80, 83, LARGE, WHITE, BLACK);
	while(1){

		sprintf(outstr,"%d \0", old);
		LCDPutStr(outstr, 80, 58, LARGE, WHITE, BLACK);
		
		int btn = getButton();
                delay(900);
		if (btn==5){
			return old;
		} else if (btn==6){
			if (old<maxR) old++;
		} else if (btn==4){
			if (old>minR) old--;
		}
	}
}

void showConnectLabel(){
	fastClear();
	  LCDPutStr("press center to start", 80, 2, SMALL, WHITE, BLACK);
}

void hideConnectionLabel(){
	LCDPutStr("press center to start", 80, 2, SMALL, BLACK, BLACK);
}

void showGlobalScore(){
	fastClear();
 
	char* outstr;
	outstr = (char*)malloc(4); 
	LCDPutStr("SCORE:", 80, 42, LARGE, WHITE, BLACK);
	
	sprintf(outstr,"%d\0", globalScoreY);
	LCDPutStr(outstr, 100, 30, LARGE, WHITE, BLACK);
	
	sprintf(outstr,"%d\0", globalScoreE);
	LCDPutStr(outstr, 100, 74, LARGE, WHITE, BLACK);
	
	if (globalScoreY>globalScoreE){
		LCDPutStr(">", 100, 60, LARGE, GREEN, BLACK);
	} else{
		LCDPutStr("<", 100, 60, LARGE, RED, BLACK);
	}
        delay(10000);
}

int main()
{
  int n;//my number
  int roundCount = 5;
   initAll();
   while(1)
   {
	   int res = showMenu();
	   switch(res){
			case 4:
                PIOB_CODR=LCD_BACKLIGHT;
                return;
                            
			case 3: 
				showAbout();
				break;
			case 0:
				showConnectLabel();
				n = connect();
				hideConnectionLabel();
				if (n!=0){
					menuPictureOK=0;
					LCDClearScreen();
					game(roundCount, n);
					showScore();
				}
				break;
			case 2:
				roundCount = chooseRoundsCount(roundCount);
				break;
			case 1:
				showGlobalScore();
				break;
	   }		
   }
}
