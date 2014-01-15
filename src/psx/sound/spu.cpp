/***************************************************************************
                            spu.c  -  description
                             -------------------
    begin                : Wed May 15 2002
    copyright            : (C) 2002 by Pete Bernert
    email                : BlackDove@addcom.de
 ***************************************************************************/
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version. See also the license.txt file for *
 *   additional informations.                                              *
 *                                                                         *
 ***************************************************************************/

#include <QTime>
#include "stdafx.h"
#include "spu.h"
#include "psxmem.h"
#include "plugins.h"

#define _IN_SPU

#include "externals.h"
#include "regs.h"

// globals

// psx buffer / addresses

unsigned short  regArea[10*1024];
unsigned short  spuMem[256*1024];
unsigned char * spuMemC;
unsigned char * pSpuIrq=0;
unsigned char * pMixIrq=0;

// user settings

int             iVolume=3;
int             iXAPitch=1;
int             iDebugMode=0;
int             iRecordMode=0;
int             iUseReverb=2;
int             iUseInterpolation=2;
int             iDisStereo=0;

// MAIN infos struct for each channel

SPUCHAN         s_chan[MAXCHAN+1];                     // channel + 1 infos (1 is security for fmod handling)
REVERBInfo      rvb;

unsigned long   dwNoiseVal=1;                          // global noise generator
int             iSpuAsyncWait=0;

unsigned short  spuCtrl=0;                             // some vars to store psx reg infos
unsigned short  spuStat=0;
unsigned short  spuIrq=0;
unsigned long   spuAddr=0xffffffff;                    // address into spu mem
int             bSpuInit=0;
int             bSPUIsOpen=0;

unsigned long dwNewChannel=0;                          // flags for faster testing, if new channel starts
unsigned long dwChannelOn=0;

void (*cddavCallback)(unsigned short,unsigned short)=0;

// certain globals (were local before, but with the new timeproc I need em global)

static const int f[5][2] = {   {    0,  0  },
                        {   60,  0  },
                        {  115, -52 },
                        {   98, -55 },
                        {  122, -60 } };
int SSumLR[NSSIZE*2];
int iFMod[NSSIZE];
int iCycle = 0;

int lastch=-1;             // last channel processed on spu irq in timer mode
static int lastns=0;       // last ns pos

////////////////////////////////////////////////////////////////////////
// CODE AREA
////////////////////////////////////////////////////////////////////////

// dirty inline func includes

#include "reverb.cpp"
#include "adsr.cpp"

////////////////////////////////////////////////////////////////////////
// helpers for simple interpolation

//
// easy interpolation on upsampling, no special filter, just "Pete's common sense" tm
//
// instead of having n equal sample values in a row like:
//       ____
//           |____
//
// we compare the current delta change with the next delta change.
//
// if curr_delta is positive,
//
//  - and next delta is smaller (or changing direction):
//         \.
//          -__
//
//  - and next delta significant (at least twice) bigger:
//         --_
//            \.
//
//  - and next delta is nearly same:
//          \.
//           \.
//
//
// if curr_delta is negative,
//
//  - and next delta is smaller (or changing direction):
//          _--
//         /
//
//  - and next delta significant (at least twice) bigger:
//            /
//         __- 
//
//  - and next delta is nearly same:
//           /
//          /
//


static inline void InterpolateUp(int ch)
{
 if(s_chan[ch].SB[32]==1)                              // flag == 1? calc step and set flag... and don't change the value in this pass
  {
   const int id1=s_chan[ch].SB[30]-s_chan[ch].SB[29];  // curr delta to next val
   const int id2=s_chan[ch].SB[31]-s_chan[ch].SB[30];  // and next delta to next-next val :)

   s_chan[ch].SB[32]=0;

   if(id1>0)                                           // curr delta positive
    {
     if(id2<id1)
      {s_chan[ch].SB[28]=id1;s_chan[ch].SB[32]=2;}
     else
     if(id2<(id1<<1))
      s_chan[ch].SB[28]=(id1*s_chan[ch].sinc)/0x10000L;
     else
      s_chan[ch].SB[28]=(id1*s_chan[ch].sinc)/0x20000L; 
    }
   else                                                // curr delta negative
    {
     if(id2>id1)
      {s_chan[ch].SB[28]=id1;s_chan[ch].SB[32]=2;}
     else
     if(id2>(id1<<1))
      s_chan[ch].SB[28]=(id1*s_chan[ch].sinc)/0x10000L;
     else
      s_chan[ch].SB[28]=(id1*s_chan[ch].sinc)/0x20000L; 
    }
  }
 else
 if(s_chan[ch].SB[32]==2)                              // flag 1: calc step and set flag... and don't change the value in this pass
  {
   s_chan[ch].SB[32]=0;

   s_chan[ch].SB[28]=(s_chan[ch].SB[28]*s_chan[ch].sinc)/0x20000L;
   if(s_chan[ch].sinc<=0x8000)
        s_chan[ch].SB[29]=s_chan[ch].SB[30]-(s_chan[ch].SB[28]*((0x10000/s_chan[ch].sinc)-1));
   else s_chan[ch].SB[29]+=s_chan[ch].SB[28];
  }
 else                                                  // no flags? add bigger val (if possible), calc smaller step, set flag1
  s_chan[ch].SB[29]+=s_chan[ch].SB[28];
}

//
// even easier interpolation on downsampling, also no special filter, again just "Pete's common sense" tm
//

static inline void InterpolateDown(int ch)
{
 if(s_chan[ch].sinc>=0x20000L)                                 // we would skip at least one val?
  {
   s_chan[ch].SB[29]+=(s_chan[ch].SB[30]-s_chan[ch].SB[29])/2; // add easy weight
   if(s_chan[ch].sinc>=0x30000L)                               // we would skip even more vals?
    s_chan[ch].SB[29]+=(s_chan[ch].SB[31]-s_chan[ch].SB[30])/2;// add additional next weight
  }
}

////////////////////////////////////////////////////////////////////////
// helpers for gauss interpolation

#define gval0 (((short*)(&s_chan[ch].SB[29]))[gpos])
#define gval(x) (((short*)(&s_chan[ch].SB[29]))[(gpos+x)&3])

#include "gauss_i.h"

////////////////////////////////////////////////////////////////////////

#include "xa.cpp"

////////////////////////////////////////////////////////////////////////
// START SOUND... called by main thread to setup a new sound on a channel
////////////////////////////////////////////////////////////////////////

static inline void StartSound(int ch)
{
 StartADSR(ch);
 StartREVERB(ch);

 // fussy timing issues - do in VoiceOn
 //s_chan[ch].pCurr=s_chan[ch].pStart;                   // set sample start
 //s_chan[ch].bStop=0;
 //s_chan[ch].bOn=1;

 s_chan[ch].s_1=0;                                     // init mixing vars
 s_chan[ch].s_2=0;
 s_chan[ch].iSBPos=28;

 s_chan[ch].SB[29]=0;                                  // init our interpolation helpers
 s_chan[ch].SB[30]=0;

 if(iUseInterpolation>=2)                              // gauss interpolation?
      {s_chan[ch].spos=0x30000L;s_chan[ch].SB[28]=0;}  // -> start with more decoding
 else {s_chan[ch].spos=0x10000L;s_chan[ch].SB[31]=0;}  // -> no/simple interpolation starts with one 44100 decoding

 dwNewChannel&=~(1<<ch);                               // clear new channel bit
}

////////////////////////////////////////////////////////////////////////
// ALL KIND OF HELPERS
////////////////////////////////////////////////////////////////////////

static inline void VoiceChangeFrequency(int ch)
{
 s_chan[ch].iUsedFreq=s_chan[ch].iActFreq;             // -> take it and calc steps
 s_chan[ch].sinc=s_chan[ch].iRawPitch<<4;
 if(!s_chan[ch].sinc) s_chan[ch].sinc=1;
 if(iUseInterpolation==1) s_chan[ch].SB[32]=1;         // -> freq change in simle imterpolation mode: set flag
}

////////////////////////////////////////////////////////////////////////

static inline void FModChangeFrequency(int ch,int ns)
{
 int NP=s_chan[ch].iRawPitch;

 NP=((32768L+iFMod[ns])*NP)/32768L;

 if(NP>0x3fff) NP=0x3fff;
 if(NP<0x1)    NP=0x1;

 NP=(44100L*NP)/(4096L);                               // calc frequency

 s_chan[ch].iActFreq=NP;
 s_chan[ch].iUsedFreq=NP;
 s_chan[ch].sinc=(((NP/10)<<16)/4410);
 if(!s_chan[ch].sinc) s_chan[ch].sinc=1;
 if(iUseInterpolation==1)                              // freq change in simple interpolation mode
 s_chan[ch].SB[32]=1;
 iFMod[ns]=0;
}                    

////////////////////////////////////////////////////////////////////////

// noise handler... just produces some noise data
// surely wrong... and no noise frequency (spuCtrl&0x3f00) will be used...
// and sometimes the noise will be used as fmod modulation... pfff

static inline int iGetNoiseVal(int ch)
{
 int fa;

 if((dwNoiseVal<<=1)&0x80000000L)
  {
   dwNoiseVal^=0x0040001L;
   fa=((dwNoiseVal>>2)&0x7fff);
   fa=-fa;
  }
 else fa=(dwNoiseVal>>2)&0x7fff;

 // mmm... depending on the noise freq we allow bigger/smaller changes to the previous val
 fa=s_chan[ch].iOldNoise+((fa-s_chan[ch].iOldNoise)/((0x001f-((spuCtrl&0x3f00)>>9))+1));
 if(fa>32767L)  fa=32767L;
 if(fa<-32767L) fa=-32767L;              
 s_chan[ch].iOldNoise=fa;

 if(iUseInterpolation<2)                               // no gauss/cubic interpolation?
 s_chan[ch].SB[29] = fa;                               // -> store noise val in "current sample" slot
 return fa;
}                                 

////////////////////////////////////////////////////////////////////////

static inline void StoreInterpolationVal(int ch,int fa)
{
 if(s_chan[ch].bFMod==2)                               // fmod freq channel
  s_chan[ch].SB[29]=fa;
 else
  {
   if((spuCtrl&0x4000)==0) fa=0;                       // muted?
   else                                                // else adjust
    {
     if(fa>32767L)  fa=32767L;
     if(fa<-32767L) fa=-32767L;              
    }

   if(iUseInterpolation>=2)                            // gauss/cubic interpolation
    {     
     int gpos = s_chan[ch].SB[28];
     gval0 = fa;          
     gpos = (gpos+1) & 3;
     s_chan[ch].SB[28] = gpos;
    }
   else
   if(iUseInterpolation==1)                            // simple interpolation
    {
     s_chan[ch].SB[28] = 0;                    
     s_chan[ch].SB[29] = s_chan[ch].SB[30];            // -> helpers for simple linear interpolation: delay real val for two slots, and calc the two deltas, for a 'look at the future behaviour'
     s_chan[ch].SB[30] = s_chan[ch].SB[31];
     s_chan[ch].SB[31] = fa;
     s_chan[ch].SB[32] = 1;                            // -> flag: calc new interolation
    }
   else s_chan[ch].SB[29]=fa;                          // no interpolation
  }
}

////////////////////////////////////////////////////////////////////////

static inline int iGetInterpolationVal(int ch)
{
 int fa;

 if(s_chan[ch].bFMod==2) return s_chan[ch].SB[29];

 switch(iUseInterpolation)
  {   
   //--------------------------------------------------//
   case 3:                                             // cubic interpolation
    {
     long xd;int gpos;
     xd = ((s_chan[ch].spos) >> 1)+1;
     gpos = s_chan[ch].SB[28];

     fa  = gval(3) - 3*gval(2) + 3*gval(1) - gval0;
     fa *= (xd - (2<<15)) / 6;
     fa >>= 15;
     fa += gval(2) - gval(1) - gval(1) + gval0;
     fa *= (xd - (1<<15)) >> 1;
     fa >>= 15;
     fa += gval(1) - gval0;
     fa *= xd;
     fa >>= 15;
     fa = fa + gval0;

    } break;
   //--------------------------------------------------//
   case 2:                                             // gauss interpolation
    {
     int vl, vr;int gpos;
     vl = (s_chan[ch].spos >> 6) & ~3;
     gpos = s_chan[ch].SB[28];
     vr=(gauss[vl]*gval0)&~2047;
     vr+=(gauss[vl+1]*gval(1))&~2047;
     vr+=(gauss[vl+2]*gval(2))&~2047;
     vr+=(gauss[vl+3]*gval(3))&~2047;
     fa = vr>>11;
    } break;
   //--------------------------------------------------//
   case 1:                                             // simple interpolation
    {
     if(s_chan[ch].sinc<0x10000L)                      // -> upsampling?
          InterpolateUp(ch);                           // --> interpolate up
     else InterpolateDown(ch);                         // --> else down
     fa=s_chan[ch].SB[29];
    } break;
   //--------------------------------------------------//
   default:                                            // no interpolation
    {
     fa=s_chan[ch].SB[29];                  
    } break;
   //--------------------------------------------------//
  }

 return fa;
}

////////////////////////////////////////////////////////////////////////
// MAIN SPU FUNCTION
// here is the main job handler... thread, timer or direct func call
// basically the whole sound processing is done in this fat func!
////////////////////////////////////////////////////////////////////////

// 5 ms waiting phase, if buffer is full and no new sound has to get started
// .. can be made smaller (smallest val: 1 ms), but bigger waits give
// better performance

#define PAUSE_W 5
#define PAUSE_L 5000

////////////////////////////////////////////////////////////////////////

static int soundBufferIndex = 0;
static int soundBufferBase = 0;
static const int SoundBufferSize = 8192;
static short soundBuffer[SoundBufferSize];

void MAINThread()
{
 int s_1,s_2,fa,ns,ns_from,ns_to;
 unsigned char * start;unsigned int nSample;
 int ch,predict_nr,shift_factor,flags,d,s;

   ns_from=0;
   ns_to=NSSIZE;
   ch=0;
   if(lastch>=0)                                       // will be -1 if no continue is pending
    {
     ch=lastch; ns_from=lastns+1; lastch=-1;           // -> setup all kind of vars to continue
    }

   //--------------------------------------------------//
   //- main channel loop                              -// 
   //--------------------------------------------------//
    {
     for(;ch<MAXCHAN;ch++)                             // loop em all... we will collect 1 ms of sound of each playing channel
      {
       if(dwNewChannel&(1<<ch)) StartSound(ch);        // start new sound
       if(!(dwChannelOn&(1<<ch))) continue;            // channel not playing? next

       if(s_chan[ch].iActFreq!=s_chan[ch].iUsedFreq)   // new psx frequency?
        VoiceChangeFrequency(ch);

       for(ns=ns_from;ns<ns_to;ns++)                   // loop until 1 ms of data is reached
        {
         int sval;

         if(s_chan[ch].bFMod==1 && iFMod[ns])          // fmod freq channel
          FModChangeFrequency(ch,ns);

         while(s_chan[ch].spos>=0x10000L)
          {
           if(s_chan[ch].iSBPos==28)                   // 28 reached?
            {
             start=s_chan[ch].pCurr;                   // set up the current pos

             if (start == (unsigned char*)-1)          // special "stop" sign
              {
               dwChannelOn&=~(1<<ch);                  // -> turn everything off
               s_chan[ch].ADSRX.EnvelopeVol=0;
               goto ENDX;                              // -> and done for this channel
              }

             s_chan[ch].iSBPos=0;

             //////////////////////////////////////////// spu irq handler here? mmm... do it later

             s_1=s_chan[ch].s_1;
             s_2=s_chan[ch].s_2;

             predict_nr=(int)*start;start++;
             shift_factor=predict_nr&0xf;
             predict_nr >>= 4;
             flags=(int)*start;start++;

             // -------------------------------------- // 

             for (nSample=0;nSample<28;start++)      
              {
               d=(int)*start;
               s=((d&0xf)<<12);
               if(s&0x8000) s|=0xffff0000;

               fa=(s >> shift_factor);
               fa=fa + ((s_1 * f[predict_nr][0])>>6) + ((s_2 * f[predict_nr][1])>>6);
               s_2=s_1;s_1=fa;
               s=((d & 0xf0) << 8);

               s_chan[ch].SB[nSample++]=fa;

               if(s&0x8000) s|=0xffff0000;
               fa=(s>>shift_factor);
               fa=fa + ((s_1 * f[predict_nr][0])>>6) + ((s_2 * f[predict_nr][1])>>6);
               s_2=s_1;s_1=fa;

               s_chan[ch].SB[nSample++]=fa;
              }

             //////////////////////////////////////////// irq check

			 if(spuCtrl&0x40)         // some callback and irq active?
              {
               if((pSpuIrq >  start-16 &&              // irq address reached?
                   pSpuIrq <= start) ||
                  ((flags&1) &&                        // special: irq on looping addr, when stop/loop flag is set 
                   (pSpuIrq >  s_chan[ch].pLoop-16 &&
                    pSpuIrq <= s_chan[ch].pLoop)))
               {
				 SPUirq();
                }
              }

             //////////////////////////////////////////// flag handler

             if((flags&4) && (!s_chan[ch].bIgnoreLoop))
              s_chan[ch].pLoop=start-16;               // loop adress

             if(flags&1)                               // 1: stop/loop
              {
               // We play this block out first...
               //if(!(flags&2))                          // 1+2: do loop... otherwise: stop
               if(flags!=3 || s_chan[ch].pLoop==NULL)  // PETE: if we don't check exactly for 3, loop hang ups will happen (DQ4, for example)
                {                                      // and checking if pLoop is set avoids crashes, yeah
                 start = (unsigned char*)-1;
                }
               else
                {
                 start = s_chan[ch].pLoop;
                }
              }

             s_chan[ch].pCurr=start;                   // store values for next cycle
             s_chan[ch].s_1=s_1;
             s_chan[ch].s_2=s_2;
            }

           fa=s_chan[ch].SB[s_chan[ch].iSBPos++];      // get sample data

           StoreInterpolationVal(ch,fa);               // store val for later interpolation

           s_chan[ch].spos -= 0x10000L;
          }

         if(s_chan[ch].bNoise)
              fa=iGetNoiseVal(ch);                     // get noise val
         else fa=iGetInterpolationVal(ch);             // get sample val

         sval = (MixADSR(ch) * fa) / 1023;  // mix adsr

         if(s_chan[ch].bFMod==2)                       // fmod freq channel
          iFMod[ns]=sval;                              // -> store 1T sample data, use that to do fmod on next channel
         else                                          // no fmod freq channel
          {
           //////////////////////////////////////////////
           // ok, left/right sound volume (psx volume goes from 0 ... 0x3fff)

           SSumLR[ns*2]  +=(sval*s_chan[ch].iLeftVolume)/0x4000L;
           SSumLR[ns*2+1]+=(sval*s_chan[ch].iRightVolume)/0x4000L;

           //////////////////////////////////////////////
           // now let us store sound data for reverb    

           if(s_chan[ch].bRVBActive) StoreREVERB(ch,ns,sval);
          }

         ////////////////////////////////////////////////
         // ok, go on until 1 ms data of this channel is collected

         s_chan[ch].spos += s_chan[ch].sinc;
        }
ENDX:   ;
      }
    }

  //---------------------------------------------------//
  //- here we have another 1 ms of sound data
  //---------------------------------------------------//
  // mix XA infos (if any)

  MixXA();
  
  ///////////////////////////////////////////////////////
  // mix all channels (including reverb) into one buffer
  for (ns = 0; ns < NSSIZE*2; )
   {
    SSumLR[ns] += MixREVERBLeft(ns/2);

	d = SSumLR[ns]; SSumLR[ns] = 0;
    if (d < -32767) d = -32767; if (d > 32767) d = 32767;
	soundBuffer[soundBufferIndex] = d;
    ns++;

    SSumLR[ns] += MixREVERBRight();

	d = SSumLR[ns]; SSumLR[ns] = 0;
	if(d < -32767) d = -32767; if(d > 32767) d = 32767;
	soundBuffer[soundBufferIndex+1] = d;
	soundBufferIndex = (soundBufferIndex + 2) % SoundBufferSize;
    ns++;
   }

  //////////////////////////////////////////////////////                   
  // special irq handling in the decode buffers (0x0000-0x1000)
  // we know: 
  // the decode buffers are located in spu memory in the following way:
  // 0x0000-0x03ff  CD audio left
  // 0x0400-0x07ff  CD audio right
  // 0x0800-0x0bff  Voice 1
  // 0x0c00-0x0fff  Voice 3
  // and decoded data is 16 bit for one sample
  // we assume: 
  // even if voices 1/3 are off or no cd audio is playing, the internal
  // play positions will move on and wrap after 0x400 bytes.
  // Therefore: we just need a pointer from spumem+0 to spumem+3ff, and 
  // increase this pointer on each sample by 2 bytes. If this pointer
  // (or 0x400 offsets of this pointer) hits the spuirq address, we generate
  // an IRQ. Only problem: the "wait for cpu" option is kinda hard to do here
  // in some of Peops timer modes. So: we ignore this option here (for now).

  if(pMixIrq)
   {
    for(ns=0;ns<NSSIZE;ns++)
     {
      if((spuCtrl&0x40) && pSpuIrq && pSpuIrq<spuMemC+0x1000)                 
       {
        for(ch=0;ch<4;ch++)
         {
          if(pSpuIrq>=pMixIrq+(ch*0x400) && pSpuIrq<pMixIrq+(ch*0x400)+2)
		   SPUirq();
         }
       }
      pMixIrq+=2;if(pMixIrq>spuMemC+0x3ff) pMixIrq=spuMemC;
     }
   }

  InitREVERB();
}

int spuFillBuffer(char *stream, int length) {
	int sampleLength = (soundBufferIndex - soundBufferBase + SoundBufferSize) % SoundBufferSize;
	if (sampleLength > length / 2)
		sampleLength = length / 2;
	if ((soundBufferBase + sampleLength) >= SoundBufferSize) {
		u32 partialLength = SoundBufferSize - soundBufferBase;
		memcpy((u16 *)stream, soundBuffer+soundBufferBase, partialLength*2);
		soundBufferBase = 0;
		u32 remainderLength = sampleLength - partialLength;
		memcpy((u16 *)stream + partialLength, soundBuffer, remainderLength*2);
		soundBufferBase = remainderLength;
	} else {
		memcpy((u16 *)stream, soundBuffer+soundBufferBase, sampleLength*2);
		soundBufferBase += sampleLength;
	}
	return sampleLength * 2;
}

// XA AUDIO

void SPU_playADPCMchannel(xa_decode_t *xap)
{
 if(!xap)       return;
 if(!xap->freq) return;                                // no xa freq ? bye

 FeedXA(xap);                                          // call main XA feeder
}

// CDDA AUDIO
void SPU_playCDDAchannel(short *pcm, int nbytes)
{
 if (!pcm)      return;
 if (nbytes<=0) return;

 FeedCDDA((unsigned char *)pcm, nbytes);
}

// SETUPTIMER: init of certain buffers and threads/timers
void SetupTimer(void)
{
 memset(SSumLR,0,sizeof(SSumLR));                      // init some mixing buffers
 memset(iFMod,0,NSSIZE*sizeof(int));
 bSpuInit=1;                                           // flag: we are inited
}

// REMOVETIMER: kill threads/timers
void RemoveTimer(void)
{
 bSpuInit=0;
}

// SETUPSTREAMS: init most of the spu buffers
void SetupStreams(void)
{ 
 int i;

 if(iUseReverb==1) i=88200*2;
 else              i=NSSIZE*2;

 sRVBStart = (int *)malloc(i*4);                       // alloc reverb buffer
 memset(sRVBStart,0,i*4);
 sRVBEnd  = sRVBStart + i;
 sRVBPlay = sRVBStart;

 XAStart =                                             // alloc xa buffer
  (uint32_t *)malloc(44100 * sizeof(uint32_t));
 XAEnd   = XAStart + 44100;
 XAPlay  = XAStart;
 XAFeed  = XAStart;

 CDDAStart =                                           // alloc cdda buffer
  (uint32_t *)malloc(16384 * sizeof(uint32_t));
 CDDAEnd   = CDDAStart + 16384;
 CDDAPlay  = CDDAStart;
 CDDAFeed  = CDDAStart;

 for(i=0;i<MAXCHAN;i++)                                // loop sound channels
  {
// we don't use mutex sync... not needed, would only 
// slow us down:
//   s_chan[i].hMutex=CreateMutex(NULL,FALSE,NULL);
   s_chan[i].ADSRX.SustainLevel = 0xf;                 // -> init sustain
   s_chan[i].pLoop=spuMemC;
   s_chan[i].pStart=spuMemC;
   s_chan[i].pCurr=spuMemC;
  }

  pMixIrq=spuMemC;                                     // enable decoded buffer irqs by setting the address
}

// REMOVESTREAMS: free most buffer
void RemoveStreams(void)
{ 
 free(sRVBStart);                                      // free reverb buffer
 sRVBStart = NULL;
 free(XAStart);                                        // free XA buffer
 XAStart = NULL;
 free(CDDAStart);                                      // free CDDA buffer
 CDDAStart = NULL;
}

// INIT/EXIT STUFF

// SPUINIT: this func will be called first by the main emu
long SPU_init(void)
{
 spuMemC = (unsigned char *)spuMem;                    // just small setup
 memset((void *)&rvb, 0, sizeof(REVERBInfo));
 InitADSR();

 iVolume = 3;
 iReverbOff = -1;
 spuIrq = 0;
 spuAddr = 0xffffffff;
 spuMemC = (unsigned char *)spuMem;
 pMixIrq = 0;
 memset((void *)s_chan, 0, (MAXCHAN + 1) * sizeof(SPUCHAN));
 pSpuIrq = 0;
 lastch = -1;

 iCycle = 0; 
 
 iVolume=2;
 iXAPitch=0;
 iUseReverb=0;
 iUseInterpolation=0;
 iDisStereo=0;
 SetupStreams();                                       // prepare streaming

 return 0;
}

long SPU_open(void)
{
 if (bSPUIsOpen) return 0;                             // security for some stupid main emus

 SetupTimer();                                         // timer for feeding data

 bSPUIsOpen = 1;


 return PSE_SPU_ERR_SUCCESS;
}

// SPUCLOSE: called before shutdown
long SPU_close(void)
{
 if (!bSPUIsOpen) return 0;                            // some security

 bSPUIsOpen = 0;                                       // no more open

 RemoveTimer();                                        // no more feeding

 return 0;
}

// SPUSHUTDOWN: called by main emu on final exit
long SPU_shutdown(void)
{
 SPU_close();
 RemoveStreams();                                      // no more streaming

 return 0;
}

void SPU_registerCDDAVolume(void (*CDDAVcallback)(unsigned short,unsigned short))
{
 cddavCallback = CDDAVcallback;
}

// vim:shiftwidth=1:expandtab

void SPUirq() {
	psxHu32ref(0x1070) |= SWAPu32(0x200);
}
