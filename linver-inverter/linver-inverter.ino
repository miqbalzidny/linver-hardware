#include <avr/io.h>
#include <avr/interrupt.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 20, 4);  // set the LCD address to 0x27 for a 16 chars and 2 line display

const byte vfbPin = A0,
           tfbPin = A1,
           battPin = A2;
volatile double percentMod;
int phs;

//---------------------------------Look Up Table for 50 Hz---------------------------------------
int lookUp1[] = {
  0, 25, 50, 75, 100, 125, 150, 175, 199, 223, 247, 271, 294, 318, 341, 363, 385, 407, 429, 450,
  470, 490, 510, 529, 548, 566, 583, 600, 616, 632, 647, 662, 675, 689, 701, 713, 724, 734, 744, 753,
  761, 768, 775, 781, 786, 790, 794, 796, 798, 800, 800, 800, 798, 796, 794, 790, 786, 781, 775, 768,
  761, 753, 744, 734, 724, 713, 701, 689, 675, 662, 647, 632, 616, 600, 583, 566, 548, 529, 510, 490,
  470, 450, 429, 407, 385, 363, 341, 318, 294, 271, 247, 223, 199, 175, 150, 125, 100, 75, 50, 25, 0
};

void setup() {
  // Register initilisation, see datasheet for more detail.
  TCCR1A = 0b10110000;
  /*      10xxxxxx Clear OC1A/OC1B on compare match when up-counting. Set OC1A/OC1B on compare match when down counting
          xx11xxxx Set OC1A/OC1B on compare match when up-counting. Clear OC1A/OC1B on compare match when down counting.
          xxxxxx00 WGM1 1:0 for waveform 8 (phase freq. correct).
  */
  TCCR1B = 0b00010001;
  /*      000xxxxx
          xxx10xxx WGM1 3:2 for waveform mode 8.
          xxxxx001 no prescale on the counter.
  */
  TIMSK1 = 0b00000001;
  /*      xxxxxxx1 TOV1 Flag interrupt enable. */
  ICR1 = 800;        /* Counter TOP value (at 16MHz XTAL, SPWM carrier freq. 10kHz, 200 samples/cycle).*/
  sei();             /* Enable global interrupts.*/
  DDRB = 0b00011110; /* Pin 9, 10, 11, 12 as outputs.*/
  PORTB = 0;

  pinMode(13, OUTPUT);
  pinMode(8, OUTPUT);
//  pinMode(7, OUTPUT);
  pinMode(2, INPUT_PULLUP);

  lcd.init();  // initialize the lcd
  lcd.init();
  lcd.backlight();
  lcd.setCursor(3, 0);
  lcd.print("Yopie DIY");  // Welcome screen
  lcd.setCursor(0, 1);
  lcd.print("Arduino SPWM Gen");

  percentMod = 0.01;
  for (int i = 0; i < 75; i++) {  // Soft Start
    percentMod = percentMod + 0.01;
    delay(20);
  }
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Batt--Vout--Temp");
}


void alarmIndication(int alarm) {
  TCCR1A = 0;  // shutdown SPWM output
  TIMSK1 = 0;
  PORTB &= 0b11100001;

  lcd.clear();
  lcd.setCursor(4, 0);
  lcd.print("WARNING!");
  lcd.setCursor(0, 1);
  if (alarm == 2) lcd.print(" UNDER VOLTAGE");
  if (alarm == 3) lcd.print("  OVER VOLTAGE");
  if (alarm == 4) lcd.print("OVER TEMPERATURE");
  if (alarm == 5) lcd.print("  LOW BATTERY");
  if (alarm == 20) lcd.print(" SHORT CIRCUIT");

loopX:
  for (int i = 0; i < alarm; i++) {
    digitalWrite(7, HIGH);  // turn ON LED and Buzzer
    digitalWrite(13, HIGH);
    delay(200);
    digitalWrite(7, LOW);  // then turn OFF
    digitalWrite(13, LOW);
    delay(200);
  }
  delay(1000);
  goto loopX;  //never ending story... until reset
}

void feedBackTest(float vfbIn, int tfbIn, int battIn) {
  long alrm;
  static int alrmCnt;
  static int dispCnt;
  float dis1;
  float dis2;
  float dis3;

  if (digitalRead(2) == LOW) return;
  if (phs != 1) return;

  alrm = constrain(vfbIn, 462, 660);
  if (alrm != vfbIn) alrmCnt++;
  else
    alrmCnt = 0;
  if (alrm == 462 && alrmCnt >= 150) alarmIndication(2);             // underVoltage @ 2.5V --> 2.75 / 5 x 1023 = 562
  if (alrm == 660 && alrmCnt >= 15) alarmIndication(3);              // overVoltage @ 3.15V --> 3.15 / 5 x 1023 = 645
  if (tfbIn >= 924) alarmIndication(4);                              // over temp @ 80C --> 10k/(10k + 1.068k) x 1023 =924
  if (battIn <= 457) alarmIndication(5);                             // low batt @ 10.5V --> (2k7/12.7k x 10.5) / 5 x 1023 = 457
  if (tfbIn >= 725 && digitalRead(8) == LOW) digitalWrite(8, HIGH);  // fan ON @45C --> 725
  if (tfbIn <= 679 && digitalRead(8) == HIGH) digitalWrite(8, LOW);  // fan OFF @40C --> 679

  if (dispCnt >= 50) {        // display updated every 50 cycle to avoid flickering
    dis1 = battIn * 0.02299;  // constant is the result of reversing the above calculation
    dis2 = vfbIn * 0.3560;
    dis3 = ((tfbIn - 512) / 11.0) + 25.0;
    lcd.setCursor(0, 1);
    lcd.print(String(dis1, 1) + "  " + String(dis2, 0) + "   " + String(dis3, 1));
    dispCnt = 0;
  }
  dispCnt++;
}

void loop() {
  static int vfbValue;
  static int vfbValue1;
  static int vfbRise = 0;
  static int vMax;
  float alrm = 0;
  static int alrmCnt = 0;

  float ampDiff;

  if (phs == 1) {
    vfbValue1 = vfbValue;
    vMax=max(vMax,vfbValue);

    vfbValue = analogRead(vfbPin);  // vfbPin = 0 to 5V ---> vfbValue = 0 to 1023

    if (vfbValue > vfbValue1 && vfbValue > 200) vfbRise = 1;  //check for positif cycle, bigger than noise

    if (vfbValue < vfbValue1 && vfbRise == 1) {               // maximum vfb value reached
      vfbRise = 0;
      ampDiff = 614 - vfbValue1;
      if (ampDiff > 5 || ampDiff < -5) {
        percentMod = percentMod + (ampDiff / vfbValue1);  // voltage correction
        if (percentMod > 0.97) percentMod = 0.97;    // limit duty cycle to 97%
      }
      feedBackTest(vfbValue1, analogRead(tfbPin), analogRead(battPin));    
    }
    
    if (vMax<=50 && phs==0) alrmCnt++;     // Vout<=18V alarm (50/614x220V <= 18V)
    if (alrmCnt>=1) alarmIndication(20);   // when occur for 1 cycle or 20mS
  } else  vMax=0;
}

/*---------------------------------------------------------------------------------------------------------*/
ISR(TIMER1_OVF_vect) {
  static int num;
  static int ph;
  static int dtA = 0;
  static int dtB = 5;

  if (num >= 99) {          // <------------------ 50 Hz !!!
    if (ph == 0) {          // OC1A as SPWM out
      TCCR1A = 0b10110000;  // clear OC1A, set OC1B on compare match
      dtA = 0;              // no dead time
      dtB = 5;              // adding dead time to OC1B
    } else {
      TCCR1A = 0b11100000;  // OC1B as SPWM out
      dtA = 5;
      dtB = 0;
    }
    ph ^= 1;
  }
  OCR1A = int(lookUp1[num] * percentMod) + dtA;  // SPWM width update
  OCR1B = int(lookUp1[num] * percentMod) + dtB;  // note: 0.7 used to reduce inveter output voltage

  num++;
  if (num >= 100) {  // toggle left bridge (50Hz) !!!
    delayMicroseconds(60);
    if (ph == 1) {
      digitalWrite(12, LOW);
      digitalWrite(11, HIGH);
      phs = 1;
    } else {
      digitalWrite(11, LOW);
      digitalWrite(12, HIGH);
      phs = 0;
    }
    num = 0;
  }

}
