#ifndef _SYNTH
#define _SYNTH

#include <Arduino.h>
#include "tables.h"

#define DIFF 1
#define CHA 2
#define CHB 3
#define CHC 4

#define SINE     0
#define RAMP     1
#define TRIANGLE 2
#define SQUARE   3
#define NOISE    4
#define SAW      5
#define A        6 
#define B        7
#define C        8
#define D        9
#define E        10
#define F        11
#define G        12
#define H        13
#define I        14

#define FSample 20000.0  

// ================================================================
// [关键修复] 变量类型修正
// AVR 的 unsigned int 是 16位，ESP32 是 32位。
// 必须强制使用 uint16_t 来利用整数溢出特性，否则声音频率会慢6万倍！
// ================================================================

// 1. 相位累加器 & 频率字 -> 必须 16位
volatile uint16_t PCW[16] = {0}; 
volatile uint16_t FTW[16] = {1000, 200, 300, 400,1000, 200, 300, 400,1000, 200, 300, 400,1000, 200, 300, 400};

// 2. 振幅 -> 8位
volatile uint8_t AMP[16] = {255, 255, 255, 255,255, 255, 255, 255,255, 255, 255, 255,255, 255, 255, 255};

// 3. 音高 -> 16位
volatile uint16_t PITCH[16] = {500, 500, 500, 500,500, 500, 500, 500,500, 500, 500, 500,500, 500, 500, 500};

// 4. 调制 -> int (保持原样即可，或者 int16_t)
volatile int MOD[16] = {20, 0, 64, 127,20, 0, 64, 127,20, 0, 64, 127,20, 0, 64, 127};

// 5. [注意] 地址指针 -> 必须 32位 (uint32_t 或 unsigned int)
// 因为 ESP32 的内存地址是 32位的，用 16位存不下
volatile uint32_t wavs[16];                                  
volatile uint32_t envs[16];                                  

// 6. 包络累加器 -> 必须 16位
volatile uint16_t EPCW[16] = {0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000};
volatile uint16_t EFTW[16] = {10, 10, 10, 10, 10, 10, 10, 10,10, 10, 10, 10, 10, 10, 10, 10};

volatile uint8_t divider = 8;                             
volatile unsigned int tim = 0;
volatile uint8_t tik = 0;
volatile uint8_t output_mode;

unsigned char volume[16]={0}; // 默认为0 (最大音量)
typedef void (*AudioBack)(uint8_t* audio_buf,uint16_t audio_buf_len);

class MSynth
{
  public:
        
    MSynth()
    {
      // [修改] 配合 DMA，增加缓冲区大小
      // 官方示例用了 2048，我们折中用 1024 (约50ms延迟)，保证稳定性
      _audioBufLength = 1024; 
      
      _audioBufIndex = 0;
      _instrumentId = 8;
      _trackCount = 16;
      _audioBuf = (uint8_t*)malloc(_audioBufLength);
    }
    ~MSynth()
    {
      free(_audioBuf);
      _audioBufLength = 0;
    }

    void begin(AudioBack audioBack)
    {
      _audioBack = audioBack;
      output_mode=CHA;
      for(int i=0;i<_trackCount;i++)
      {
        wavs[i] = (uint32_t)EmptyTable;
        // [关键] 防止崩溃的初始化
        envs[i] = (uint32_t)Env0;
        volume[i] = 0;
      }
    }
    
    // --- 这里的 Helper 函数保留 ---
    void setInstrument(uint8_t instrumentId) { _instrumentId = instrumentId>14?14:(instrumentId<0?0:instrumentId); }
    void setVolume(unsigned char voice,uint8_t vol) { volume[voice] = vol; }
    
    void setupVoice(unsigned char voice, unsigned char wave, unsigned char pitch, unsigned char env, unsigned char length, unsigned int mod)
    {
      setWave(voice,wave);
      setPitch(voice,pitch);
      setEnvelope(voice,env);
      setLength(voice,length);
      setMod(voice,mod);
    }

    void setWave(unsigned char voice, unsigned char wave)
    {
      switch (wave)
      {
        case SINE: wavs[voice] = (uint32_t)SinTable; break;
        case TRIANGLE: wavs[voice] = (uint32_t)TriangleTable; break;
        case SQUARE: wavs[voice] = (uint32_t)SquareTable; break;
        case SAW: wavs[voice] = (uint32_t)SawTable; break;
        case RAMP: wavs[voice] = (uint32_t)RampTable; break;
        case NOISE: wavs[voice] = (uint32_t)NoiseTable; break;
        case A: wavs[voice] = (uint32_t)ATable; break;
        case B: wavs[voice] = (uint32_t)BTable; break;
        case C: wavs[voice] = (uint32_t)CTable; break;
        case D: wavs[voice] = (uint32_t)DTable; break;
        case E: wavs[voice] = (uint32_t)ETable; break;
        case F: wavs[voice] = (uint32_t)FTable; break;
        case G: wavs[voice] = (uint32_t)GTable; break;
        case H: wavs[voice] = (uint32_t)HTable; break;
        default: wavs[voice] = (uint32_t)ITable; break;
      }
    }

    void setPitch(unsigned char voice,unsigned char MIDInote) { PITCH[voice]=(uint16_t)(PITCHS[MIDInote]); }

    void setEnvelope(unsigned char voice, unsigned char env)
    {
      switch (env) {
      case 0: envs[voice] = (uint32_t)Env0; break;
      case 1: envs[voice] = (uint32_t)Env1; break;
      case 2: envs[voice] = (uint32_t)Env2; break;
      case 3: envs[voice] = (uint32_t)Env3; break;
      case 4: envs[voice] = (uint32_t)Env4; break;
      default: envs[voice] = (uint32_t)Env0; break;
      }
    }

    void setLength(unsigned char voice,unsigned char length) { EFTW[voice]=(uint16_t)(EFTWS[length]); }
    void setMod(unsigned char voice,unsigned char mod) { MOD[voice]=(int)mod-64; }
    void setFrequency(unsigned char voice,float f) { PITCH[voice]=f/(FSample/65535.0); }
    void setTime(unsigned char voice,float t) { EFTW[voice]=(1.0/t)/(FSample/(32767.5*10.0)); }

    void trigger(unsigned char voice)
    {
      EPCW[voice]=0;
      FTW[voice]=PITCH[voice];
    }

    void render()
    {
      divider++;
      if(!(divider%=_trackCount))
        tik=1;
      
      // ESP32 上 PCW 是 16位，EPCW 是 16位，这里强制转换取高字节
      if (!(((uint8_t*)&EPCW[divider])[1]&0x80))
      {
        uint8_t*address = (uint8_t*)(envs[divider]);
        AMP[divider] = address[((uint8_t*)&(EPCW[divider]+=EFTW[divider]))[1]];
      }
      else
        AMP[divider] = 0;

      int32_t data = 0;
      for(int i=0;i<_trackCount;i++)
      {
        int8_t*address = (int8_t*)(wavs[i]);
        // [关键] PCW[i] 是 uint16_t，加法会自动溢出回绕，模拟 AVR 行为
        data += (address[((uint8_t *)&(PCW[i] += FTW[i]))[1]] * AMP[i]) >> (volume[i] + 6);
      }
      
      data = data / _trackCount;
      int output = 127 + data * 4;
      if(output > 255) output = 255;
      if(output < 0) output = 0;

      _audioBuf[_audioBufIndex] = (uint8_t)output;
      _audioBufIndex++;
      
      if(_audioBufIndex>=_audioBufLength)
      {
          _audioBufIndex = 0;
          (*_audioBack)(_audioBuf,_audioBufLength);
      }
      
      FTW[divider] = PITCH[divider] + (int) (((PITCH[divider]>>6)*(EPCW[divider]>>6))/128)*MOD[divider];
    }
    
    void addNote(int track, int note,int8_t time=64){
      track = track%_trackCount;
      setupVoice(track, _instrumentId, note, 1, time, 64);
      trigger(track);
    }
    
  private:
    AudioBack _audioBack;
    uint8_t *_audioBuf;
    uint8_t _trackCount;
    int _audioBufIndex;
    int _instrumentId;
    uint16_t _audioBufLength;
};

#endif