
// verzie
//
// 2022-07-18 nova objektova verzia


#define VERSION 20220718

#include <TimeLib.h>
#include <LiquidCrystal.h>

#define ECHOPIN 7 // Echo Pin
#define TRIGPIN 6 // Trigger Pin
#define PLNA 4 // vzdialenost v cm od senzora ked je nadrz plna
#define PRAZDNA 89 // vzdialenost v cm od senzora ked je nadrz prazdna
#define HLADINA_AKTIVACIE 95 // percento naplnenia nadrze pri ktorom zase zacne spinat cerpadlo a doplnat nadrz
#define VODA_PRAH 900 // hodnota hladinovaho senzora, prah ked detekuje vodu
#define STUDNA_DOBA_BEHU 120 // kolko sekund maximalne moze bezat cerpadlo v studni
#define STUDNA_DOBA_ODDYCHU 600 // kolko sekund ma cerpadlo studne oddychovat 

// technicke definicie
#define TIME_MSG_LEN  11   // time sync to PC is HEADER and unix time_t as ten ascii digits
#define TIME_HEADER  255   // Header tag for serial time sync message

// chyby
#define ERROR_MAX_MIN 1 // chyba: voda je detekovana na maxime ale neni detekovana na minime

// warningy
#define WRN_VECER_NEDOST_NADOBA -1 // varovanie: mala sa spustit vecer zavlaha ale nadoba nemala ani 50%
#define WRN_RANO_NEDOST_NADOBA -2  // varovanie: mala sa spustit rano zavlaha ale nadoba nemala ani 50%
#define WRN_ZAVL_PRAZDNA_NADOBA -3  // varovanie: pri zavlazovani sa vyprazdnila nadoba 

#define AKTUALNY_CAS_V_SEKUNDACH second() + minute() * 60 + hour() * 3600

// ***** globals *****

boolean fHladinaBolaMax;
boolean fStudnaNaplna;

int Status; // kod chyby alebo varovania

int gZavlaha = 0;  // aktualna sekcia zavlahy ktora polieva, ak 0 - nepolieva ziadna
int gZavlahaMod = 2;  // 1 polievaj urceny cas v stanovenu dobu aj rano aj vecer,
// 2 vypolievat celu nadrz aj rano aj vecer,
// 3 ak je malo vody v nadrzi zozpolievaj obsah nadoby na 3 sekcie rano a 3 sekcie vecer
int zavlahaVecer[2] = {22, 00}; //cas vecernej zavlahy
int zavlahaRano[2] = {06, 00}; // cas rannej zavlahy
int gCasZavlahy[6]; // casy pri ktorych sa prepne dalsia sekcia pri mode 1
int gValueMin, gValueMax;
int gMode = 1;  // mod zavlahy 0 - nepolievaj, 1 - iba vecer, 2 - vecer aj rano
int gZostavajuciCasNaplnania = 30;   // cas v minutach kolko sa bude naplnat nadoba, zvoli sa pri spustani
int lZostavajuciCasNaplnania;   // aktualny celkovy cas v sekundach kolko sa bude naplnat nadoba

// ************** classes definition CerpadloStudna **************

class CerpadloStudna {
  private:
    boolean zapnute;
    boolean mamVodu;
    boolean senzory;
    long posledneZapnutie;
    long posledneVypnutie;

  private:
    // vrati true ak je voda na kontakte Max
    boolean jeVodaNaMax() {
      int sensorValueMax = gValueMax = analogRead(A1);
      if (sensorValueMax < VODA_PRAH)
        return true;
      else
        return false;
    }

    // vrati true ak je voda na kontakte Min
    boolean jeVodaNaMin() {
      int sensorValueMin = gValueMin = analogRead(A0);
      if (sensorValueMin < VODA_PRAH)
        return true;
      else
        return false;
    }

  public:
    void Init();
    void Update(int zavlaha);
    boolean Zapni();
    boolean Vypni();
    boolean Stav() { return zapnute; };
};

void CerpadloStudna::Init(){
  pinMode(11, OUTPUT); // rele 7 cerpadlo studna
  pinMode(12, OUTPUT); // rele 8 pripajanie GND vodica senzorov vlhkosti v studni

  posledneZapnutie = AKTUALNY_CAS_V_SEKUNDACH;
  posledneVypnutie = AKTUALNY_CAS_V_SEKUNDACH;
  zapnute = false;
  digitalWrite(11, HIGH); // vypni cerpadlo studne

  digitalWrite(12, LOW); // zapni kontakt senzorov vlhkosti v studni
  senzory = true;
  delay(1000);
  mamVodu = jeVodaNaMax() && jeVodaNaMin();
  delay(1000);
  digitalWrite(12, HIGH); // vypni kontakt senzorov vlhkosti v studni
  senzory = false;
}

void CerpadloStudna::Update(int zavlaha){
  long aktualny_cas_v_sekundach = AKTUALNY_CAS_V_SEKUNDACH;
  long studna_doba_behu = STUDNA_DOBA_BEHU;

  // ak sa uz zohrieva tepelna prudova poistka treba vypnut
  if (zapnute && (zavlaha > 0 || !mamVodu || (aktualny_cas_v_sekundach - posledneZapnutie >= studna_doba_behu))){
    this->Vypni();
    return;
  }

  // kontakty senzorov zapnut
  if (zapnute && (aktualny_cas_v_sekundach % 10 == 0)){
    digitalWrite(12, LOW); // zapni kontakt senzorov vlhkosti v studni
    senzory = true;
  }
  if (!zapnute && (aktualny_cas_v_sekundach % 900 == 0)){
    digitalWrite(12, LOW); // zapni kontakt senzorov vlhkosti v studni
    senzory = true;
  }

  // merat hladiny
  if (zapnute && senzory && (aktualny_cas_v_sekundach % 10 == 1)){
    mamVodu = jeVodaNaMax() && jeVodaNaMin();
  }
  if (!zapnute && senzory && (aktualny_cas_v_sekundach % 900 == 1)){
    mamVodu = jeVodaNaMax() && jeVodaNaMin();
  }
  
  // kontakty senzorov vypnut
  if (zapnute && (aktualny_cas_v_sekundach % 10 >= 2)){
    digitalWrite(12, HIGH); // vypni kontakt senzorov vlhkosti v studni
    senzory = false;
  }
  if (!zapnute && (aktualny_cas_v_sekundach % 900 >= 2)){
    digitalWrite(12, HIGH); // vypni kontakt senzorov vlhkosti v studni
    senzory = false;
  }

}

boolean CerpadloStudna::Zapni(){
  long aktualny_cas_v_sekundach = AKTUALNY_CAS_V_SEKUNDACH;
  long studna_doba_oddychu = STUDNA_DOBA_ODDYCHU;
  if (!zapnute && mamVodu && (aktualny_cas_v_sekundach - posledneVypnutie >= studna_doba_oddychu)){
     posledneZapnutie = aktualny_cas_v_sekundach;
     zapnute = true;
     digitalWrite(11, LOW); // zapni cerpadlo studne
  }
}

boolean CerpadloStudna::Vypni(){
  posledneVypnutie = AKTUALNY_CAS_V_SEKUNDACH;
  zapnute = false;
  digitalWrite(11, HIGH); // vypni cerpadlo studne
}

// ************** classes definition Nadoba **************



LiquidCrystal lcd(5, 4, 3, 2, 1, 0);
CerpadloStudna gCStudna;


// ***** globalne funkcie ******
void zapniSekciu(int i);
int distanceCm();
int percentoNaplnenia();
void naplnajNadobu();
void prepisDisplay(void);

// ***** Setup ******
void setup() {
  // put your setup code here, to run once:
  //Serial.begin(9600);

  lcd.begin(16, 2);
  lcd.print("Initializing...");
  lcd.setCursor(0, 1);
  lcd.print("Ver.");
  lcd.print(VERSION);

  setTime(00, 00, 00, 1, 1, 2016);
  delay(1000);
  pinMode(TRIGPIN, OUTPUT);
  pinMode(ECHOPIN, INPUT);
  pinMode(8, OUTPUT); // rele 1-6 BCD A
  pinMode(9, OUTPUT); // rele 1-6 BCD B
  pinMode(10, OUTPUT); // rele 1-6 BCD C
  pinMode(13, INPUT); // nastavovacie tlacitko

  gCStudna.Init();

  for (int i = 8; i <= 10; i++)
    digitalWrite(i, LOW); // vypni vsetky sekcie

  nastavCasPriSpustani();  // nastavenie casu pri spustani

  lZostavajuciCasNaplnania = gZostavajuciCasNaplnania * 60; // cas v sekundach kolko sa bude naplnat nadoba

  Status = 0;
}

// ***** Main loop ******
void loop() {
  // put your main code here, to run repeatedly:
  int oldSecond = 0;
  int i, percSekcia;


  while (1) {

    // management nadoby a cerpadla v studni
    //gPercentoNaplnenia = percentoNaplnenia();
    
    gCStudna.Update(gZavlaha);
    if (gZavlaha > 0)
        gCStudna.Vypni();
    else {
        if (lZostavajuciCasNaplnania > 0)
            gCStudna.Zapni();
        else
            gCStudna.Vypni();
    }
    

    if (digitalRead(13) == 0) { // ak sa stlaci tlacitko zapne alebo vypne to zavlahu v tom momente
      if (gZavlaha > 0) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Stop");
        lcd.setCursor(0, 1);
        lcd.print("watering...");
        delay(1000);
        gZavlaha = 0;
        zapniSekciu(0);

        // iniciuj cerpadlo v studni na docerpanie
        lZostavajuciCasNaplnania = gZostavajuciCasNaplnania * 60;
      }
      else if (gZavlahaMod == 2) { // Mod 2 vypolievat celu nadrz, kriterium je bud percento alebo cas
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Starting");
        lcd.setCursor(0, 1);
        lcd.print("watering...");
        gCStudna.Vypni();
        delay(10000);
        gZavlaha = 1;

        // sekcia 1 vzadu pri domceku
        gCasZavlahy[0] = 14* 60;

        //sekcia 2 pri terase
        gCasZavlahy[1] = 12 * 60;

        //sekcia 3 bok a skalka
        gCasZavlahy[2] = 4 * 60;

        //sekcia 4 vpredu rotatory
        gCasZavlahy[3] = 12 * 60;

        // sekcia 5 vzadu pri komposte
        gCasZavlahy[4] = 12 * 60;

        // sekcia 6 kvapkova
        gCasZavlahy[5] = 1 * 5;

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(gCasZavlahy[0]/60);
        lcd.print(" ");
        lcd.print(gCasZavlahy[1]/60);
        lcd.print(" ");
        lcd.print(gCasZavlahy[2]/60);
        lcd.setCursor(0, 1);
        lcd.print(gCasZavlahy[3]/60);
        lcd.print(" ");
        lcd.print(gCasZavlahy[4]/60);
        lcd.print(" ");
        lcd.print(gCasZavlahy[5]/60);

        delay(3000);

        zapniSekciu(gZavlaha);
      }
    }

    // spustanie zavlahy v stanoveny cas
    if ((hour() == zavlahaVecer[0] && minute() == zavlahaVecer[1] && second() == 0 && gMode > 0) || (hour() == zavlahaRano[0] && minute() == zavlahaRano[1] && second() == 0 && gMode > 1)) {
      if (gZavlahaMod == 2) { // Mod 2 vypolievat celu nadrz aj rano aj vecer, kriterium je bud percento alebo cas
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Starting");
        lcd.setCursor(0, 1);
        lcd.print("watering...");
        gCStudna.Vypni();
        delay(1000);
        gZavlaha = 1;

        // sekcia 1 vzadu pri domceku
        gCasZavlahy[0] = 14* 60;

        //sekcia 2 pri terase
        gCasZavlahy[1] = 12 * 60;

        //sekcia 3 bok a skalka
        gCasZavlahy[2] = 4 * 60;

        //sekcia 4 vpredu rotatory
        gCasZavlahy[3] = 12 * 60;

        // sekcia 5 vzadu pri komposte
        gCasZavlahy[4] = 12 * 60;

        // sekcia 6 kvapkova
        gCasZavlahy[5] = 1 * 5;

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(gCasZavlahy[0]/60);
        lcd.print(" ");
        lcd.print(gCasZavlahy[1]/60);
        lcd.print(" ");
        lcd.print(gCasZavlahy[2]/60);
        lcd.setCursor(0, 1);
        lcd.print(gCasZavlahy[3]/60);
        lcd.print(" ");
        lcd.print(gCasZavlahy[4]/60);
        lcd.print(" ");
        lcd.print(gCasZavlahy[5]/60);

        delay(3000);

        zapniSekciu(gZavlaha);
      }
    }


    if (gZavlaha && gZavlahaMod == 2) { // mod rozpolievaj obsah nadoby rovnomerne na vsetky sekcie
      if ((gCasZavlahy[gZavlaha - 1] == 0)  && gZavlaha < 6) {
        gZavlaha++;
        zapniSekciu(gZavlaha);
      }
      if ((gCasZavlahy[gZavlaha - 1] == 0) && gZavlaha == 6) {
        gZavlaha = 0;
        zapniSekciu(0);

        // iniciuj cerpadlo v studni na docerpanie
        lZostavajuciCasNaplnania = gZostavajuciCasNaplnania * 60;
      }
    }

    if (oldSecond != second()) { // raz za sekundu prepis display
      oldSecond = second();
      if (gZavlaha && gCasZavlahy[gZavlaha - 1] > 0) {
        gCasZavlahy[gZavlaha - 1]--;
      }
      if (gCStudna.Stav() && lZostavajuciCasNaplnania > 0)
        lZostavajuciCasNaplnania--;   
      prepisDisplay();  // obsluha displeja
    }
    delay(120);

    if (Status > 0) { // ak nastala chyba nekonecna slucka
      prepisDisplay();
      while (1);
    }
  }

}
// ****************************** pomocne funkcie **************************

void nastavCasPriSpustani() {
  int h = 0, m = 0, s = 0;
  int timer = 0;
  lcd.clear();
  lcd.begin(16, 2);
  lcd.print("Set hour");

  while (timer < 15) {
    // cas
    lcd.setCursor(0, 1);
    if (h < 10)
      lcd.print("0");
    lcd.print(h);
    lcd.print(":");
    if (m < 10)
      lcd.print("0");
    lcd.print(m);
    lcd.print(":");
    if (s < 10)
      lcd.print("0");
    lcd.print(s);

    if (digitalRead(13) == 0) {
      h++;
      if (h == 24)
        h = 0;
      timer = 0;
    }
    else
      timer++;
    delay(200);
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Set minute");
  timer = 0;
  while (timer < 15) {
    // cas
    lcd.setCursor(0, 1);
    if (h < 10)
      lcd.print("0");
    lcd.print(h);
    lcd.print(":");
    if (m < 10)
      lcd.print("0");
    lcd.print(m);
    lcd.print(":");
    if (s < 10)
      lcd.print("0");
    lcd.print(s);

    if (digitalRead(13) == 0) {
      m++;
      if (m == 60)
        m = 0;
      timer = 0;
    }
    else
      timer++;
    delay(200);
  }

  setTime(h, m, 00, 1, 1, 2016);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Set mode");
  timer = 0;
  while (timer < 15){
    // cas
    lcd.setCursor(0, 1);
    if (gMode == 0)
      lcd.print("No watering");
    if (gMode == 1)
      lcd.print("At the evening");
    if (gMode == 2)
      lcd.print("Both evening and morning");

    if (digitalRead(13) == 0) {
      gMode++;
      if (gMode > 2)
        gMode = 0;
      timer = 0;
    }
    else
      timer++;
    delay(200);
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Set time to fill");
  timer = 0;
  while (timer < 15){
    // cas
    lcd.setCursor(0, 1);
    lcd.print(gZostavajuciCasNaplnania);
    lcd.print(" ");

    if (digitalRead(13) == 0) {
      gZostavajuciCasNaplnania++;
      if (gZostavajuciCasNaplnania > 50)
        gZostavajuciCasNaplnania = 0;
      timer = 0;
    }
    else
      timer++;
    delay(200);
  }
}

// prevedie cislo do binarneho kodu BCD pre digitalne vystupy 8-10, 0 nic, 1 prve rele,... atd
// zapne rele odpovedajuce cislu
// 0 vsetky vypnute
void zapniSekciu(int k) {


  if (k >= 4) {
    digitalWrite(10, HIGH);
    k = k - 4;
  }
  else digitalWrite(10, LOW);

  if (k >= 2) {
    digitalWrite(9, HIGH);
    k = k - 2;
  }
  else digitalWrite(9, LOW);

  if (k >= 1) {
    digitalWrite(8, HIGH);
  }
  else digitalWrite(8, LOW);
}

void prepisDisplay(void) {
  int h, m, s;
  // status
  lcd.clear();
  lcd.setCursor(0, 0);
  if (Status == 0) {
    lcd.print("OK MOD");
    lcd.print(gZavlahaMod);
  }
  else if (Status < 0) {
    lcd.print("WRN ");
    lcd.print(-1 * Status);
  }
  else if (Status > 0)
  {
    lcd.print("ERR ");
    lcd.print(Status);
  }

  // cas
  lcd.setCursor(8, 0);
  h = hour();
  if (h < 10)
    lcd.print("0");
  lcd.print(h);
  lcd.print(":");
  m = minute();
  if (m < 10)
    lcd.print("0");
  lcd.print(m);
  lcd.print(":");
  s = second();
  if (s < 10)
    lcd.print("0");
  lcd.print(s);

  // detail
  lcd.setCursor(0, 1);
  if (gCStudna.Stav())
    lcd.print("CP ");

  if (gZavlaha > 0 && gZavlahaMod == 1) {
    lcd.print("Z");
    lcd.print(gZavlaha);
    lcd.print(" ");
    m = gCasZavlahy[gZavlaha - 1] / 60;
    s = gCasZavlahy[gZavlaha - 1] - m * 60;
    if (m < 10)
      lcd.print("0");
    lcd.print(m);
    lcd.print(":");
    if (s < 10)
      lcd.print("0");
    lcd.print(s);
  }
  if (gZavlaha > 0 && gZavlahaMod == 2) {
    lcd.print("Z");
    lcd.print(gZavlaha);
    lcd.print(" ");
    m = gCasZavlahy[gZavlaha - 1] / 60;
    s = gCasZavlahy[gZavlaha - 1] - m * 60;
    if (m < 10)
      lcd.print("0");
    lcd.print(m);
    lcd.print(":");
    if (s < 10)
      lcd.print("0");
    lcd.print(s);
  }
  else {
      lcd.print(gValueMin);
      lcd.print("/");
      lcd.print(gValueMax);
      

      // ZostavajuciCasNaplnania
      lcd.setCursor(11, 1);
      m = lZostavajuciCasNaplnania / 60;
      s = lZostavajuciCasNaplnania - m * 60;
      if (m < 10)
        lcd.print("0");
      lcd.print(m);
      lcd.print(":");
      if (s < 10)
        lcd.print("0");
      lcd.print(s);
    }
}
