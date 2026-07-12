#include "cockpit_alarm.h"
#include "xplaneConnect.h"
#include <SDL2/SDL2_gfxPrimitives.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct { int warning,caution,fire,stall,overspeed,connected; } AlarmState;
struct CockpitAlarm { SDL_mutex *lock; SDL_Thread *thread; SDL_AudioDeviceID audio; SDL_atomic_t stop; AlarmState state,last; Uint32 last_sound; };

static int alarm_thread(void *arg)
{
    CockpitAlarm *a=arg;XPCSocket sock=openUDP("127.0.0.1");
    const char *drefs[]={"sim/cockpit2/annunciators/master_warning","sim/cockpit2/annunciators/master_caution","sim/cockpit2/annunciators/engine_fires","sim/cockpit2/annunciators/stall_warning","sim/cockpit2/annunciators/overspeed"};float v[5][8];
    while(!SDL_AtomicGet(&a->stop)){AlarmState s={0};if(XPCSocket_IsOpen(sock)&&getDREFs(sock,drefs,5,v)){s.warning=v[0][0]>.5f;s.caution=v[1][0]>.5f;s.fire=v[2][0]>.5f||v[2][1]>.5f;s.stall=v[3][0]>.5f;s.overspeed=v[4][0]>.5f;s.connected=1;}SDL_LockMutex(a->lock);a->state=s;SDL_UnlockMutex(a->lock);SDL_Delay(100);}
    closeUDP(sock);return 0;
}

static void queue_tone(CockpitAlarm *a,int severe)
{
    Sint16 samples[11025];double hz=severe?880.0:520.0;for(int i=0;i<11025;i++){double gate=severe?1.0:((i/2205)%2?0.25:1.0);samples[i]=(Sint16)(sin(2.0*3.141592653589793*hz*i/44100.0)*9000.0*gate);}SDL_ClearQueuedAudio(a->audio);SDL_QueueAudio(a->audio,samples,sizeof(samples));SDL_PauseAudioDevice(a->audio,0);
}

CockpitAlarm *CockpitAlarm_Create(void)
{
    CockpitAlarm *a=calloc(1,sizeof(*a));SDL_AudioSpec want={0};if(!a)return NULL;a->lock=SDL_CreateMutex();want.freq=44100;want.format=AUDIO_S16SYS;want.channels=1;want.samples=2048;a->audio=SDL_OpenAudioDevice(NULL,0,&want,NULL,0);a->thread=SDL_CreateThread(alarm_thread,"XPlane alarms",a);return a;
}

void CockpitAlarm_Update(CockpitAlarm *a)
{
    AlarmState s;Uint32 now;if(!a)return;SDL_LockMutex(a->lock);s=a->state;SDL_UnlockMutex(a->lock);now=SDL_GetTicks();int severe=s.warning||s.fire||s.stall||s.overspeed;int caution=s.caution;if((severe||caution)&&(memcmp(&s,&a->last,sizeof(s))||now-a->last_sound>2500)){queue_tone(a,severe);a->last_sound=now;}if(!severe&&!caution&&a->audio)SDL_ClearQueuedAudio(a->audio);a->last=s;
}

void CockpitAlarm_Render(CockpitAlarm *a,SDL_Renderer *r,int width)
{
    AlarmState s;int flash=(SDL_GetTicks()/350)%2;if(!a||!r)return;SDL_LockMutex(a->lock);s=a->state;SDL_UnlockMutex(a->lock);
    if((s.warning||s.fire||s.stall||s.overspeed)&&flash){roundedBoxRGBA(r,width/2-170,18,width/2-10,70,8,220,15,20,245);stringRGBA(r,width/2-130,40,"WARNING",255,255,255,255);}
    if(s.caution&&flash){roundedBoxRGBA(r,width/2+10,18,width/2+170,70,8,235,175,10,245);stringRGBA(r,width/2+55,40,"CAUTION",20,20,10,255);}
    if(!s.connected){stringRGBA(r,width-155,18,"X-PLANE OFFLINE",180,185,190,255);}
}

void CockpitAlarm_Destroy(CockpitAlarm *a){if(!a)return;SDL_AtomicSet(&a->stop,1);if(a->thread)SDL_WaitThread(a->thread,NULL);if(a->audio)SDL_CloseAudioDevice(a->audio);if(a->lock)SDL_DestroyMutex(a->lock);free(a);}
int CockpitAlarm_SelfTest(void){AlarmState s={0};s.fire=1;return (s.warning||s.fire||s.stall||s.overspeed)&&!s.caution;}
