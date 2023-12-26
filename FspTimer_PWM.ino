const int USE_LED_BLINKER = 1;
void LED_BLINKER() { if(USE_LED_BLINKER) digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN)); }


#include <FspTimer.h>
#include <AGTimerR4.h>
#define INTERVAL_MICRO_SEC 32  // micro sec

static FspTimer fsp_timer;



// 音痴でないノート番号 36～103
// xxx.h のフィルタ作ること @@@
// 1. 40～103をのみ発音する(64音階、6ビット)  0..39,104..127の ノートオン/ノートオフを無視する
// 2. 12音全て発音中のノートオンを無視する  それを覚えておいて、ノートオフが出てきたら無視する

#define N_NOTE  16  // 最大同時発音数 21

typedef struct {
  int8_t   vel;     // ベロシティ（音の強さ）
  uint16_t x;       // カウント
  uint16_t dx;      // dx を x に足していき、オーバーフロー(65535超えたら)で符号反転
  uint8_t  n;       // 音程
  uint8_t  ch;      // 音符番号
} TUnit;

// データ形式 （ビットフィールド使用。処理系によってはLSB→MSBの順ではないことがあるので注意）
typedef union {
  uint16_t word;
  struct {
    uint8_t n:7;    // note
    uint8_t vel:4;  // velocity
    uint8_t ch:4;   // ch
    uint8_t type:1; // 1:wait, 0:note
  };
} TNote;

const uint8_t Score[] = {
  #include "m0-lupin.h"
};

volatile uint8_t DeltaTime;

TUnit Unit[N_NOTE];           // 7バイト x 同時発音数
const uint16_t dx_array[] = { // note: 0--127
     34,    36,    38,    41,    43,    46,    48,    51,    54,    58,    61,    65,
     69,    73,    77,    82,    86,    92,    97,   103,   109,   115,   122,   129,
    137,   145,   154,   163,   173,   183,   194,   206,   218,   231,   244,   259,
    274,   291,   308,   326,   346,   366,   388,   411,   435,   461,   489,   518,
    549,   581,   616,   652,   691,   732,   776,   822,   871,   923,   978,  1036,
   1097,  1163,  1232,  1305,  1383,  1465,  1552,  1644,  1742,  1845,  1955,  2071,
   2195,  2325,  2463,  2610,  2765,  2930,  3104,  3288,  3484,  3691,  3910,  4143,
   4389,  4650,  4927,  5220,  5530,  5859,  6207,  6577,  6968,  7382,  7821,  8286,
   8779,  9301,  9854, 10440, 11060, 11718, 12415, 13153, 13935, 14764, 15642, 16572,
  17557, 18601, 19707, 20879, 22121, 23436, 24830, 26306, 27871, 29528, 31284, 33144,
};

void timerCallback() {    // タイマ割り込み  32us(=31.25KHz)
  static uint8_t tick32us;
  static uint16_t Envelope;

  if((++tick32us == 0) && (DeltaTime != 0)) DeltaTime--;  // 32us*256=8.192[ms]
  int8_t env_dec = (Envelope++ == 0xC00);                 // 65.536msごとにTRUEになる
  if(env_dec) Envelope = 0;

  uint8_t  out = 0x80;
  for(int8_t i=0; i<N_NOTE; i++) {
    int8_t v = Unit[i].vel;
  //if(v == 0) continue;
    out += v;
    uint16_t dx = Unit[i].dx;
    if((Unit[i].x += dx) < dx) Unit[i].vel = -v;
    if(env_dec && (0 < Unit[i].vel)) Unit[i].vel--;
  }

  //fsp_timer.set_duty_cycle(out, CHANNEL_B);   // D1==CHANNEL_A(==0), D0==CHANNEL_B(==1)
  R_GPT4->GTCCR[3] = out;
}

//----------------------------------------------------------------------
// 発音ユニットに値をセット(vel=0でノートオフ)
const uint8_t vel_conv_array[16] = { 0,1,1,2,3,3,4,5,5,6,7,7,8,9,9,10 };
void set_note(uint8_t ch, uint8_t notenum, uint8_t vel) {
  uint8_t   i;
  uint16_t  dx;

  if(vel != 0) {  // ノートオン
    // 空き(vel=0)見つける
    for(i=0; i<N_NOTE; i++)
      if(Unit[i].vel == 0) break;
    if(i == N_NOTE) return;       // 念のため

    //dx = dx_array[notenum - 24];  // note 0--23 は使わない
    dx = dx_array[notenum];  // note 0--23 は使わない
  //vel = vel_conv_array[vel];
    vel = vel >> 1;
  } else {        // ノートオフ
    // 同じもの探す
    for(i=0; i<N_NOTE; i++)
      if(Unit[i].n == notenum && Unit[i].ch == ch) break;
    if(i == N_NOTE) return;       // 念のため

    // それを消音
    notenum = 0;
    vel = 0;
    dx = 0;
  }

  Unit[i].dx = dx;
  Unit[i].vel = vel;
  Unit[i].n = notenum;
  Unit[i].ch = ch;
}

void play_score(void) {
  const uint8_t *p = Score;
  const uint8_t *pe = Score + sizeof(Score);

  while (p < pe) {
    LED_BLINKER();

    while(0 < DeltaTime);           // DeltaTime待ち

    int8_t c = *p++;                // 取り出す

    if (c & 0x80) DeltaTime = -c;   // wait
    else {                          // note
      //TNote data = (TNote)((uint16_t)c << 8 | *p++);
      TNote data;
      data.word = (uint16_t)c << 8 | *p++;
      if((24 <= data.n) && (data.n <= 119)) set_note(data.ch, data.n, data.vel);
    }
  }
}

#define PRINT_REG(BUCKET, REGISTER) \
  do { \
    uint32_t t = BUCKET->REGISTER; \
    char sbuf[128]; \
    sprintf(sbuf, "%04X %04X ", unsigned(t>>16), unsigned(t&0xFFFF)); \
    Serial.print(sbuf); \
    Serial.println(#REGISTER); \
  } while (false)

void printRegisters(const char x[]) {
  Serial.println(x);
  PRINT_REG(R_GPT4, GTPR);
  PRINT_REG(R_GPT4, GTPBR);
  PRINT_REG(R_GPT4, GTCR);
  PRINT_REG(R_GPT4, GTUDDTYC);
  PRINT_REG(R_GPT4, GTIOR);
  PRINT_REG(R_GPT4, GTCNT);
  PRINT_REG(R_GPT4, GTCCR[3]);
  PRINT_REG(R_GPT4, GTBER);
}

void init_hard(void) {
//   R_MSTP->MSTPCRD_b.MSTPD6 = 0;  // モジュールストップコントロール, ストップ状態を解除
// //  R_GPT4->GTPR = 255;
// R_GPT4->GTPBR = 255;
//   R_GPT4->GTIOR = 0x01090000;   // 周期の終わりでHigh, コンペアマッチでLow, GTIOCB端子出力許可
//   R_GPT4->GTCCR[3] = 128;       // 中央値
// R_GPT4->GTCCR[6] = 128;       // 中央値
//   R_PFS->PORT[3].PIN[1].PmnPFS_b.PMR = 0;         // D0を汎用入出力に設定
//   R_PFS->PORT[3].PIN[1].PmnPFS_b.PSEL = 0b00011;  // D0をGPT端子として使用
//   R_PFS->PORT[3].PIN[1].PmnPFS_b.PMR = 1;         // D0を周辺機能用の端子に設定
// R_GPT4->GTBER = 0x00150000;
//   R_GPT4->GTCR = 1;             // カウント動作を実行, のこぎり波PWMモード, PCLKD/1


printRegisters("initial");
  fsp_timer.begin(TIMER_MODE_PWM, GPT_TIMER, 4, 256, 128, TIMER_SOURCE_DIV_1); // GTIOC4A,4B  48Mz/256=187.5kHz
printRegisters("after begin");
  fsp_timer.open();
printRegisters("after open");

  R_PFS->PORT[3].PIN[1].PmnPFS_b.PMR = 0;         // D0を汎用入出力に設定
  R_PFS->PORT[3].PIN[1].PmnPFS_b.PSEL = 0b00011;  // D0をGPT端子として使用
  R_PFS->PORT[3].PIN[1].PmnPFS_b.PMR = 1;         // D0を周辺機能用の端子に設定
  R_GPT4->GTIOR_b.GTIOB = 0b01001;  // 周期の終わりでHigh, コンペアマッチでLow
  R_GPT4->GTIOR_b.OBE = 1;          // GTIOCB端子出力許可
R_GPT4->GTBER = 0x00150000;
  R_GPT4->GTCR_b.CST = 1;  //fsp_timer.start();
printRegisters("after start");
// for(;;);

  AGTimer.init(INTERVAL_MICRO_SEC, timerCallback);
}

void notes_off(void) {
  for (int i = 0; i < N_NOTE; i++) {
    Unit[i].vel = 0;
    Unit[i].dx  = 0;
    Unit[i].n   = 0;
    Unit[i].ch  = 0;
  }
}

void setup() {
  Serial.begin(2000000); while(!Serial); Serial.println();  // for debug
  pinMode(LED_BUILTIN, OUTPUT);  // for LED

  init_hard();
  for(;;) {
    play_score();
    notes_off();
    DeltaTime = 122; while(DeltaTime);  // 1 sec
  }
}

void loop() { }
// EOF

/*
     34,    36,    38,    41,    43,    46,    48,    51,    54,    58,    61,    65,
     69,    73,    77,    82,    86,    92,    97,   103,   109,   115,   122,   129,
    137,   145,   154,   163,   173,   183,   194,   206,   218,   231,   244,   259,
    274,   291,   308,   326,   346,   366,   388,   411,   435,   461,   489,   518,
    549,   581,   616,   652,   691,   732,   776,   822,   871,   923,   978,  1036,
   1097,  1163,  1232,  1305,  1383,  1465,  1552,  1644,  1742,  1845,  1955,  2071,
   2195,  2325,  2463,  2610,  2765,  2930,  3104,  3288,  3484,  3691,  3910,  4143,
   4389,  4650,  4927,  5220,  5530,  5859,  6207,  6577,  6968,  7382,  7821,  8286,
   8779,  9301,  9854, 10440, 11060, 11718, 12415, 13153, 13935, 14764, 15642, 16572,
  17557, 18601, 19707, 20879, 22121, 23436, 24830, 26306, 27871, 29528, 31284, 33144,


## 1オクターブ分の音程データ(MIDIノート番号の108～119に対応, oct=9のド～シ)
dxdata = [17557, 18601, 19708, 20879, 22121, 23436, 24830, 26306, 27871, 29528, 31284, 33144]
dx_array = [0]*128

def f(note) : return 8372*pow(2, (note-120)/12)
def T(note) : return 1000000/f(note)
def dx(note): return round(65536*32*2/T(note))
for i in range(0, 128): dx_array[i] = dx(i)

S = ''
for note in range(128):
	s = f'{dx_array[note]:5}, '
	S += s
	if note%12 == 11: print(S); S=''
*/
