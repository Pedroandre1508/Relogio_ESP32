// M1 ESP32+LCD16x2: RS=21 E=22 D4-D7=19,18,5,17  B1=27 B2=26 B3=25 LED=2
// sem delay() no loop; debounce; millis. Wokwi: "Build busy" = fila na nuvem;
// build local (extensao VS Code) costuma ser mais estavel. Sketch pequeno ok.
#include <LiquidCrystal.h>

const int P_RS=21, P_E=22, P_D4=19, P_D5=18, P_D6=5, P_D7=17;
const int P_B1=27, P_B2=26, P_B3=25, P_LED=2;
const int BTN[3] = {P_B1, P_B2, P_B3};
const uint32_t T_DB=50, T_LONG=1200, T_FBK=800, T_PISC=500, T_PULH=200, T_PCRD=100;

enum S { S_BOOT, S_RUN, S_SH, S_SM, S_CRO };
static S st = S_BOOT;
static uint32_t tBoot0, tS, t1m, tLed, ftil, tPc, tB, swA, sw0; static int eH, eM;
static bool pa=1, swG, fB=1;
static char fbuf[17];
struct { int p, s; uint32_t tms, tdn; bool lg; } b[3];
LiquidCrystal Lcd(P_RS, P_E, P_D4, P_D5, P_D6, P_D7);

static void p2(int v) { v %= 100; Lcd.print((char)('0'+v/10)); Lcd.print((char)('0'+v%10)); }
static void d2(int v) { v %= 10; Lcd.print((char)('0'+v)); }
static uint32_t swM() { return swG ? swA + (millis()-sw0) : swA; }
static void hBip() { int m=(tS/60)%60, s=(int)(tS%60); if (m==0 && s==0) tLed=millis()+T_PULH; }
void hRel(int k, uint32_t d);

static void fbk(const char* t) {
  uint8_t i; for (i=0; i<16 && t[i]; i++) fbuf[i]=(char)t[i];
  for (; i<16; i++) fbuf[i]=' ';
  fbuf[16]=0; ftil=millis()+T_FBK; }
static void p16() { for (int i=0; i<16; i++) Lcd.print(fbuf[i]); }
static void bI(){ for (int k=0;k<3;k++){ b[k].p=1; b[k].s=1; b[k].tms=b[k].tdn=0; b[k].lg=0; } }
static void blp(){ if (millis()-tB>T_PISC) tB=millis(), fB=!fB; }

void bC() {
  uint32_t t=millis();
  for (int n=0;n<3;n++) { int w=digitalRead(BTN[n])==LOW?0:1;
    if (w!=b[n].p){ b[n].p=w; b[n].tms=t; }
    if (t - b[n].tms < T_DB) continue;
    if (b[n].p == b[n].s) continue;
    { int a=b[n].s; b[n].s=b[n].p;
      if (a==1 && b[n].s==0) { b[n].tdn=t; b[n].lg=0; }
      else if (a==0 && b[n].s==1) { hRel(n,t - b[n].tdn); b[n].tdn=0; } } }
  if (st==S_RUN && b[1].s==0 && b[1].tdn) {
    if (!b[1].lg && t - b[1].tdn >= T_LONG) {
      b[1].lg=1; st=S_CRO; pa=1; swA=sw0=0; swG=0; fbk("> Cron.     "); tPc=0; } } }
void tck() { if (pa || st!=S_RUN) return; uint32_t w=millis();
  while (w - t1m >= 1000) { t1m+=1000; tS++; if (tS>=86400) tS=0; hBip(); } }
void lEd() { uint32_t t=millis();
  if (t < tLed) { digitalWrite(P_LED,HIGH); return; }
  if (st==S_SH || st==S_SM) { digitalWrite(P_LED,HIGH); return; }
  if (st==S_CRO) { digitalWrite(P_LED,swG?HIGH:LOW); return; } digitalWrite(P_LED,LOW); }

void hRel(int k, uint32_t d) {
  if (st==S_BOOT) return; if (k==1 && d >= T_LONG && st==S_CRO) return;
  if (k==0) {
    if (st==S_RUN)  { eH=(tS/3600)%24; eM=(tS/60)%60; st=S_SH; pa=1; tB=millis(); fB=1; fbk("Config. H  "); }
    else if (st==S_SH)  { st=S_SM;  fbk("Config. M  ");  tB=millis(); fB=1; }
    else if (st==S_SM)  { tS=(uint32_t)(eH%24)*3600u + (uint32_t)(eM%60)*60u; st=S_RUN; pa=0; t1m=millis(); fbk("Salvou! 0s"); }
    else if (st==S_CRO) { st=S_RUN; swG=0; swA=0; pa=0; t1m=millis(); fbk("Fim  cron. "); } }
  else if (k==1) {
    if (st==S_SH)  { eH=(eH+1)%24;  fbk("+1 hora   ");  tB=millis(); fB=1; }
    else if (st==S_SM)  { eM=(eM+1)%60;  fbk("+1 min.  ");  tB=millis(); fB=1; }
    else if (st==S_CRO)  { if (!swG) { sw0=millis(); swG=1; fbk("> Corre   "); } else { swA+=(millis()-sw0); swG=0; fbk(">Pausa   "); } } }
  else {
    if (st==S_SH)  { eH=(eH+23)%24;  fbk("-1 hora   ");  tB=millis(); fB=1; }
    else if (st==S_SM)  { eM=(eM+59)%60;  fbk("-1 min.  ");  tB=millis(); fB=1; }
    else if (st==S_CRO)  { if (swG) swA+=(millis()-sw0); swA=0; swG=0; sw0=millis(); fbk("> Zero!   "); } } }
void des() {
  if (st==S_CRO) { if (millis()-tPc < T_PCRD) return; tPc=millis(); } else tPc=millis();
  if (st==S_SH || st==S_SM) blp();
  if (st==S_BOOT) { if (millis()<tBoot0) { Lcd.setCursor(0,0); Lcd.print("  ESP32  Rel.  ");
      Lcd.setCursor(0,1); Lcd.print("  Inicializ... "); return; } st=S_RUN; pa=0; t1m=millis(); }
  if (st==S_RUN) {
    int h=(tS/3600)%24, m=(tS/60)%60, s=(int)(tS%60);
    Lcd.setCursor(0,0); Lcd.print("  "); p2(h); Lcd.print(':'); p2(m); Lcd.print(':'); p2(s); Lcd.print("  ");
    Lcd.setCursor(0,1);
    if (millis()<ftil) p16();
    else Lcd.print("EXEC B1+ B2^1.2s");
  } else if (st==S_SH) {
    Lcd.setCursor(0,0); Lcd.print("MODO: CONFIG. ");
    Lcd.setCursor(0,1);
    if (millis()<ftil) p16();
    else if (fB) { Lcd.print("Hora >>"); p2(eH); Lcd.print("<<  "); } else Lcd.print("H. (pisca)  ");
  } else if (st==S_SM) {
    Lcd.setCursor(0,0); Lcd.print("MODO: CONFIG. ");
    Lcd.setCursor(0,1);
    if (millis()<ftil) p16();
    else if (fB) { Lcd.print("Min. >>"); p2(eM); Lcd.print("<<  "); } else Lcd.print("M. (pisca)  ");
  } else {
    uint32_t mss=swM(), ds=mss/100, s0=ds/10, d0=ds%10, mm=(uint32_t)((s0/60)%100), ss=(int)(s0%60);
    Lcd.setCursor(0,0); Lcd.print("CRO ");
    p2((int)mm); Lcd.print(':'); p2(ss); Lcd.print('.'); d2((int)(d0%10)); Lcd.print("   ");
    Lcd.setCursor(0,1);
    if (millis()<ftil) p16(); else Lcd.print("1=fim2=I/P3=0");
  } }
void setup() { bI();
  for (int n=0;n<3;n++) pinMode(BTN[n],INPUT_PULLUP);
  pinMode(P_LED,OUTPUT); digitalWrite(P_LED,LOW);
  Lcd.begin(16,2);
  st=S_BOOT; tS=tLed=tPc=0; tBoot0=millis()+850; t1m=millis();
  Lcd.setCursor(0,0); Lcd.print("  ESP32  Rel.  ");  Lcd.setCursor(0,1);  Lcd.print("  Inicializ... ");
}
void loop() { bC(); tck(); lEd(); des(); }
