// SPDX-License-Identifier: GPL-3.0-only
#include "pet_model.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void)
{
    tt_pet_book_t b,decoded; tt_pet_runtime_t r={0};
    tt_pet_default(&b); assert(tt_pet_valid(&b)); assert(tt_pet_level(&b.pets[0])==1);
    tt_pet_state_t other=b.pets[1];
    assert(tt_pet_act(&b,&r,0,TT_PET_FEED,100)==TT_PET_OK);
    assert(b.pets[0].food==950 && b.pets[0].meals==1 && b.pets[0].xp==8);
    tt_pet_book_t saved=b;
    assert(tt_pet_act(&b,&r,0,TT_PET_FEED,200)==TT_PET_WAIT);
    assert(!memcmp(&saved,&b,sizeof(b)));
    assert(tt_pet_act(&b,&r,0,TT_PET_FEED,6000)==TT_PET_OK);
    assert(tt_pet_act(&b,&r,0,TT_PET_PLAY,6000)==TT_PET_OK);
    assert(b.pets[0].energy==850 && b.pets[0].games==1);
    assert(tt_pet_act(&b,&r,0,TT_PET_CLEAN,6000)==TT_PET_OK);
    assert(b.pets[0].hygiene==1000);
    assert(tt_pet_act(&b,&r,0,TT_PET_CLEAN,12000)==TT_PET_OK);
    assert(tt_pet_act(&b,&r,0,TT_PET_REST,12000)==TT_PET_OK);
    assert(tt_pet_act(&b,&r,0,TT_PET_PLAY,12000)==TT_PET_OK);
    tt_pet_tick(&b,&r,0,12000); tt_pet_tick(&b,&r,0,22000);
    assert(b.pets[0].energy==850);
    tt_pet_tick(&b,&r,0,52000); assert(b.pets[0].energy>=850 && !b.pets[0].resting);
    assert(!memcmp(&b.pets[1],&other,sizeof(other)));
    assert(tt_pet_act(&b,&r,0,TT_PET_CUDDLE,52000)==TT_PET_OK);
    assert(tt_pet_act(&b,&r,0,TT_PET_CUDDLE,72000)==TT_PET_WAIT);
    b.pets[0].xp=898; b.pets[0].bond=999;
    assert(tt_pet_act(&b,&r,0,TT_PET_PLAY,72000)==TT_PET_OK);
    assert(b.pets[0].xp==900 && b.pets[0].bond==1000 && tt_pet_level(&b.pets[0])==10);
    b.pets[0].food=1; b.pets[0].hygiene=1; b.pets[0].joy=1; b.pets[0].energy=100;
    tt_pet_tick(&b,&r,0,172000);
    assert(tt_pet_valid(&b) && b.pets[0].food>1 && b.pets[0].energy>100);
    /* Weeks of unattended use require no feeding/cleaning and never lose earned growth. */
    for (int i=1;i<=24*14;++i) tt_pet_tick(&b,&r,0,172000+(int64_t)i*3600000);
    assert(b.pets[0].food>=700 && b.pets[0].joy>=750 && b.pets[0].hygiene>=800 && b.pets[0].energy>=850);
    assert(b.pets[0].xp==900 && b.pets[0].bond==1000);
    assert(!memcmp(&b.pets[1],&other,sizeof(other)));
    assert(tt_pet_decode(&decoded,&b,sizeof(b)));
    assert(!memcmp(&decoded,&b,sizeof(b)));
    assert(!tt_pet_decode(&decoded,&b,sizeof(b)-1) && decoded.pets[0].food==700);
    b.schema=99; assert(!tt_pet_decode(&decoded,&b,sizeof(b)));
    b.schema=2; b.pets[2].joy=1001; assert(!tt_pet_decode(&decoded,&b,sizeof(b)));
    assert(tt_pet_act(&decoded,&r,3,TT_PET_FEED,200000)==TT_PET_WAIT);
    tt_pet_default(&b);
    struct { uint32_t schema; tt_pet_state_t pets[3]; } legacy={.schema=1};
    memcpy(legacy.pets,b.pets,sizeof(legacy.pets)); legacy.pets[1].xp=222;
    assert(tt_pet_decode(&decoded,&legacy,sizeof(legacy)) && decoded.schema==3 && decoded.pets[1].xp==222 && decoded.pets[0].xp==222 && decoded.focus.sessions==0);
    assert(tt_pet_focus_complete(&b,0,5)==0 && b.focus.sessions==0);
    assert(tt_pet_focus_complete(&b,0,60)==1 && b.focus.sessions==1);
    assert(b.focus.seconds==60 && b.pets[0].xp>0 && b.pets[1].xp==0);
    assert(tt_pet_focus_complete(&b,0,1500)==2 && b.focus.longest==1500);
    for (int i=0;i<48;++i) tt_pet_focus_complete(&b,1,1500);
    assert(b.focus.sessions==50 && b.focus.badges==63);
    for (unsigned i=0;i<TT_FOCUS_BADGES;++i) assert(tt_focus_badge_progress(&b.focus,i)==100);
    assert(tt_pet_decode(&decoded,&b,sizeof(b)) && decoded.focus.badges==63);
    tt_focus_progress_t progress=b.focus; tt_pet_tick(&b,&r,0,1000000000);
    assert(!memcmp(&progress,&b.focus,sizeof(progress)));
    puts("PASS: needs, cooldowns, sleep/recovery, level cap, independent pets, blob validation");
    return 0;
}
