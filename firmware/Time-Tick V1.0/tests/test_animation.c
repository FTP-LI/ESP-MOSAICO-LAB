// SPDX-License-Identifier: GPL-3.0-only
#include "pet_animation.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    tt_animation_t a={0};
    tt_animation_set(&a,TT_ANIM_WALK,false,100);
    assert(tt_animation_frame(&a,100000)==0); /* Time alone cannot make stationary feet step. */
    for (unsigned i=1;i<=6;++i) {
        tt_animation_walk(&a,8*256);
        assert(tt_animation_frame(&a,100)==i%6);
    }
    tt_animation_walk(&a,12*256); unsigned before=tt_animation_frame(&a,100);
    tt_animation_set(&a,TT_ANIM_WALK,true,500);
    assert(a.left && tt_animation_frame(&a,500)==before); /* Turning does not reset the stride. */
    for (int clip=TT_ANIM_EAT;clip<=TT_ANIM_WASH;++clip) {
        tt_animation_set(&a,(tt_animation_clip_t)clip,false,1000);
        for (unsigned frame=0;frame<6;++frame) {
            assert(tt_animation_busy(&a,1000+frame*150));
            assert(tt_animation_frame(&a,1000+frame*150)==frame);
        }
        uint32_t end=1000+tt_animation_duration(a.clip);
        assert(tt_animation_busy(&a,end-1) && !tt_animation_busy(&a,end));
        tt_animation_set(&a,a.clip,false,end+1);
        assert(a.started==end+1 && tt_animation_frame(&a,end+1)==0);
    }
    tt_animation_set(&a,TT_ANIM_IDLE,false,0);
    tt_animation_set(&a,TT_ANIM_EAT,false,UINT32_MAX-500);
    assert(tt_animation_busy(&a,100) && tt_animation_frame(&a,100)==4);
    assert(!tt_animation_busy(&a,2000));
    puts("PASS: distance-locked footsteps, direction changes, all action frames, completion/restart and clock wrap");
    return 0;
}
