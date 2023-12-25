#include <FspTimer.h>
#include <AGTimerR4.h>
#define INTERVAL_SAMPLING 32  // micro sec

static FspTimer fsp_timer;



// 音痴でないノート番号 36～103
// xxx.h のフィルタ作ること @@@
// 1. 40～103をのみ発音する(64音階、6ビット)  0..39,104..127の ノートオン/ノートオフを無視する
// 2. 12音全て発音中のノートオンを無視する  それを覚えておいて、ノートオフが出てきたら無視する

#define N_NOTE  21  // 最大同時発音数 21

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
const uint16_t dx_array[] = { // 8オクターブ
  137, 145, 153, 163, 172, 183, 193, 205, 217, 230, 244, 258, 
  274, 290, 307, 326, 345, 366, 387, 411, 435, 461, 488, 517, 
  548, 581, 615, 652, 691, 732, 775, 822, 870, 922, 977, 1035, 
  1097, 1162, 1231, 1304, 1382, 1464, 1551, 1644, 1741, 1845, 1955, 2071, 
  2194, 2325, 2463, 2609, 2765, 2929, 3103, 3288, 3483, 3691, 3910, 4143, 
  4389, 4650, 4927, 5219, 5530, 5859, 6207, 6576, 6967, 7382, 7821, 8286, 
  8778, 9300, 9854, 10439, 11060, 11718, 12415, 13153, 13935, 14764, 15642, 16572, 
  17557, 18601, 19708, 20879, 22121, 23436, 24830, 26306, 27871, 29528, 31284, 33144
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

  fsp_timer.set_duty_cycle(out, CHANNEL_B);   // D1==CHANNEL_A(==0), D0==CHANNEL_B(==1)
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

    dx = dx_array[notenum - 24];  // note 0--23 は使わない
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

void init_hard(void) {
  fsp_timer.begin(TIMER_MODE_PWM, GPT_TIMER, 4, 256, 128, TIMER_SOURCE_DIV_1); // GTIOC4A,4B  48Mz/256=187.5kHz
  fsp_timer.open();

    R_PFS->PORT[3].PIN[1].PmnPFS_b.PMR = 0;     //D0を汎用入出力に設定
  //R_PFS->PORT[3].PIN[2].PmnPFS_b.PMR = 0;     //D1を汎用入出力に設定
    R_PFS->PORT[3].PIN[1].PmnPFS_b.PSEL = 0x03; //D0をGPT端子として使用
  //R_PFS->PORT[3].PIN[2].PmnPFS_b.PSEL = 0x03; //D1をGPT端子として使用
    R_PFS->PORT[3].PIN[1].PmnPFS_b.PMR = 1;     //D0を周辺機能用の端子に設定
  //R_PFS->PORT[3].PIN[2].PmnPFS_b.PMR = 1;     //D1を周辺機能用の端子に設定

  //R_GPT4->GTIOR_b.GTIOA = 0x06;   //GTIOCA端子機能選択
    R_GPT4->GTIOR_b.GTIOB = 0x19;   //GTIOCB端子機能選択
  //R_GPT4->GTIOR_b.OAE = 1;        //GTIOCA端子出力許可
    R_GPT4->GTIOR_b.OBE = 1;        //GTIOCB端子出力許可

  fsp_timer.start();

  AGTimer.init(INTERVAL_SAMPLING, timerCallback);
}

void note_off_all(void) {
  for (int i = 0; i < N_NOTE; i++) {
    Unit[i].vel = 0;
    Unit[i].dx  = 0;
    Unit[i].n   = 0;
    Unit[i].ch  = 0;
  }
}

void setup() {
  //Serial.begin(115200); while(!Serial); Serial.println(); // for debug
  init_hard();

  for(;;) {
    play_score();
    note_off_all();
    delay(500);
  }
}

void loop() {
}
