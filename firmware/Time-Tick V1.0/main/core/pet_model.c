// SPDX-License-Identifier: GPL-3.0-only
#include "pet_model.h"
#include <string.h>
#include <limits.h>
static uint16_t change(uint16_t value, int delta)
{
    int v=(int)value+delta; return (uint16_t)(v<0?0:v>1000?1000:v);
}
void tt_pet_default(tt_pet_book_t *b)
{
    memset(b,0,sizeof(*b)); b->schema=3;
    for (unsigned i=0;i<3;++i) b->pets[i]=(tt_pet_state_t){.food=700,.joy=750,.energy=850,.hygiene=800};
}
bool tt_pet_valid(const tt_pet_book_t *b)
{
    if ((b->schema!=2 && b->schema!=3) || b->focus.badges>=1u<<TT_FOCUS_BADGES) return false;
    for (unsigned i=0;i<3;++i) {
        const tt_pet_state_t *p=&b->pets[i];
        if (p->food>1000 || p->joy>1000 || p->energy>1000 || p->hygiene>1000 || p->bond>1000 ||
            p->xp>900 || p->resting>1) return false;
    }
    return true;
}
bool tt_pet_decode(tt_pet_book_t *b,const void *data,size_t size)
{
    tt_pet_default(b);
    if (!data) return false;
    typedef struct { uint32_t schema; tt_pet_state_t pets[3]; } legacy_t;
    tt_pet_book_t candidate; tt_pet_default(&candidate);
    if (size==sizeof(candidate)) memcpy(&candidate,data,size);
    else if (size==sizeof(legacy_t)) {
        legacy_t old; memcpy(&old,data,size);
        if (old.schema!=1) return false;
        memcpy(candidate.pets,old.pets,sizeof(old.pets));
    } else return false;
    if (!tt_pet_valid(&candidate)) return false;
    if (candidate.schema==2 || size==sizeof(legacy_t)) {
        unsigned best=0;
        for (unsigned i=1;i<3;++i) if (candidate.pets[i].xp>candidate.pets[best].xp) best=i;
        candidate.pets[0]=candidate.pets[best];
        candidate.schema=3;
    }
    *b=candidate; return true;
}
unsigned tt_pet_level(const tt_pet_state_t *p) { return 1+p->xp/100; }
const char *tt_pet_stage(const tt_pet_state_t *p)
{
    return p->xp>=600?"Soulmate":p->xp>=200?"Good friend":"Little newcomer";
}
static void count(uint32_t *v) { if (*v<UINT32_MAX) ++*v; }
bool tt_pet_tick(tt_pet_book_t *b,tt_pet_runtime_t *r,unsigned active,int64_t now)
{
    if (active>=3) return false;
    if (!r->started || now<r->last_ms) { r->started=true; r->last_ms=now; return false; }
    r->elapsed_ms+=(uint64_t)(now-r->last_ms); r->last_ms=now;
    uint64_t seconds=r->elapsed_ms/1000; r->elapsed_ms%=1000;
    if (!seconds) return false;
    /* Companionship has no upkeep penalty. Recover below-comfort needs automatically;
     * earned bond/XP and higher values never decay, even while the timer runs. */
    if (seconds>3600) seconds=3600;
    tt_pet_state_t *p=&b->pets[active];
    for (uint64_t i=0;i<seconds;++i) {
        count(&p->seconds);
        if (p->seconds%60==0) {
            if (p->food<700) p->food=(uint16_t)(p->food+50>700?700:p->food+50);
            if (p->joy<750) p->joy=(uint16_t)(p->joy+50>750?750:p->joy+50);
            if (p->hygiene<800) p->hygiene=(uint16_t)(p->hygiene+50>800?800:p->hygiene+50);
            if (!p->resting && p->energy<850) p->energy=(uint16_t)(p->energy+50>850?850:p->energy+50);
        }
        if (p->energy<150) p->resting=1;
        if (p->resting) {
            p->energy=change(p->energy,8);
            if (p->energy>=1000) p->resting=0;
        }
    }
    return true;
}
tt_pet_result_t tt_pet_act(tt_pet_book_t *b,tt_pet_runtime_t *r,unsigned index,tt_pet_action_t a,int64_t now)
{
    if (index>=3 || a>=TT_PET_ACTIONS) return TT_PET_WAIT;
    tt_pet_state_t *p=&b->pets[index];
    if (now<r->cooldown[index][a]) return TT_PET_WAIT;
    if (a!=TT_PET_REST) p->resting=0;
    unsigned xp=0;
    switch (a) {
    case TT_PET_FEED:
        p->food=change(p->food,250); p->joy=change(p->joy,30);
        count(&p->meals); xp=8; break;
    case TT_PET_PLAY:
        p->joy=change(p->joy,220);
        count(&p->games); xp=12; break;
    case TT_PET_CLEAN:
        p->hygiene=change(p->hygiene,350); p->joy=change(p->joy,25);
        count(&p->baths); xp=8; break;
    case TT_PET_REST: p->resting=!p->resting; break;
    case TT_PET_CUDDLE:
        p->joy=change(p->joy,30); count(&p->cuddles); xp=2; break;
    default: return TT_PET_WAIT;
    }
    if (xp) { p->bond=change(p->bond,a==TT_PET_CUDDLE?10:15); p->xp=p->xp+xp>900?900:p->xp+xp; }
    r->cooldown[index][a]=now+(a==TT_PET_CUDDLE?30000:a==TT_PET_REST?2000:5000);
    return TT_PET_OK;
}
static uint32_t saturated_add(uint32_t a,uint32_t b) { return UINT32_MAX-a<b?UINT32_MAX:a+b; }
unsigned tt_focus_badge_progress(const tt_focus_progress_t *p,unsigned badge)
{
    const uint32_t values[]={p->sessions,p->longest,p->seconds,p->sessions,p->seconds,p->sessions};
    const uint32_t goals[]={1,1500,3600,10,18000,50};
    if (badge>=TT_FOCUS_BADGES) return 0;
    return values[badge]>=goals[badge]?100:(unsigned)((uint64_t)values[badge]*100/goals[badge]);
}
const char *tt_focus_badge_title(unsigned badge)
{
    static const char *names[]={"First focus / 1 session","Deep roots / 25 min","One good hour / 60 min",
        "Little steps / 10 sessions","Growing grove / 5 hours","Steady explorer / 50 sessions"};
    return badge<TT_FOCUS_BADGES?names[badge]:"";
}
uint32_t tt_pet_focus_complete(tt_pet_book_t *b,unsigned pet,uint32_t seconds)
{
    if (pet>=3 || seconds<60 || seconds>86400) return 0;
    tt_focus_progress_t *f=&b->focus; uint32_t before=f->badges;
    f->sessions=saturated_add(f->sessions,1); f->seconds=saturated_add(f->seconds,seconds);
    if (seconds>f->longest) f->longest=seconds;
    for (unsigned i=0;i<TT_FOCUS_BADGES;++i) if (tt_focus_badge_progress(f,i)==100) f->badges|=1u<<i;
    tt_pet_state_t *p=&b->pets[pet];
    unsigned xp=10+(seconds/60>50?50:seconds/60);
    p->xp=p->xp+xp>900?900:p->xp+xp; p->bond=change(p->bond,20);
    return f->badges & ~before;
}
const char *tt_pet_result_text(tt_pet_result_t r,tt_pet_action_t a,bool resting)
{
    switch (r) {
    case TT_PET_WAIT: return "A little moment, please";
    case TT_PET_FULL: return "My tummy is full";
    case TT_PET_TIRED: return "A snack and some rest first";
    case TT_PET_ASLEEP: return "Sleeping; wake me to play";
    case TT_PET_CLEAN_ALREADY: return "Already fresh and clean";
    case TT_PET_SAVE_FAILED: return "Care could not be saved; retry";
    default: break;
    }
    static const char *messages[]={"Yum, thank you!","Again, again!","All fresh and fluffy!","Sweet dreams","That feels lovely"};
    return a==TT_PET_REST && !resting?"Ready for a new adventure":messages[a<TT_PET_ACTIONS?a:0];
}
