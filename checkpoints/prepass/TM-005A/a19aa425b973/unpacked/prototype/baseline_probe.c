#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "tinyusb.h"
#include "tiltmouse/usb_hid_mouse.h"
#include "tiltmouse/usb_hid_mouse_report.h"
#include "tiltmouse/mouse_motion.h"
#include "tiltmouse/mouse_transport.h"

static bool mounted=true, ready=true, accepting=true;
static unsigned attempts, accepted;
static unsigned char last_bytes[3];
static tinyusb_config_t installed;
bool tud_mounted(void) { return mounted; }
bool tud_hid_ready(void) { return ready; }
bool tud_hid_report(uint8_t id, const void *report, uint16_t size)
{
    ++attempts;
    assert(id==0 && size==3);
    if (!accepting) { return false; }
    memcpy(last_bytes, report, 3); ++accepted; return true;
}
esp_err_t tinyusb_driver_install(const tinyusb_config_t *config)
{
    installed=*config; return ESP_OK;
}
static void event(int id)
{
    tinyusb_event_t e={id}; installed.event_cb(&e, installed.event_arg);
}
int main(void)
{
    assert(tiltmouse_usb_hid_mouse_send(true,false,1,2)==ESP_ERR_INVALID_STATE);
    assert(tiltmouse_usb_hid_mouse_init()==ESP_OK);
    assert(tiltmouse_usb_hid_mouse_init()==ESP_ERR_INVALID_STATE);
    assert(tiltmouse_usb_hid_mouse_send(true,false,7,-2)==ESP_OK);
    assert(last_bytes[0]==1 && last_bytes[1]==7 && last_bytes[2]==254);
    ready=false;
    assert(tiltmouse_usb_hid_mouse_release_all()==ESP_ERR_INVALID_STATE);
    unsigned before=accepted;
    ready=true;event(TINYUSB_EVENT_RESUMED);
    assert(accepted==before && last_bytes[0]==1); /* no automatic release */
    accepting=false;
    assert(tiltmouse_usb_hid_mouse_send(false,false,9,0)==ESP_FAIL);
    assert(accepted==before);
    accepting=true;
    assert(tiltmouse_usb_hid_mouse_release_all()==ESP_OK);
    assert(last_bytes[0]==0 && last_bytes[1]==0 && last_bytes[2]==0);
    event(TINYUSB_EVENT_SUSPENDED);
    assert(tiltmouse_usb_hid_mouse_send(false,false,8,0)==ESP_ERR_INVALID_STATE);
    event(TINYUSB_EVENT_RESUMED);mounted=false;
    assert(tiltmouse_usb_hid_mouse_send(false,false,8,0)==ESP_ERR_INVALID_STATE);
    mounted=true;

    unsigned comparisons=0;
    for (int x=-127;x<=127;++x) {
        for (int y=-127;y<=127;++y) {
            for (unsigned b=0;b<4;++b) {
                tm_mouse_report_t proposed;
                assert(tm_mouse_report_make(x,y,b,&proposed));
                const tiltmouse_usb_hid_mouse_report_t legacy=
                    tiltmouse_usb_hid_mouse_make_report((b&1)!=0,(b&2)!=0,
                                                      (int8_t)x,(int8_t)y);
                assert(proposed.dx==legacy.x && proposed.dy==legacy.y &&
                       proposed.buttons==legacy.buttons);
                ++comparisons;
            }
        }
    }
    tm_mouse_report_t proposed;
    assert(!tm_mouse_report_make(-128,0,0,&proposed));
    assert(proposed.kind==TM_REPORT_INVALID && proposed.dx==0);
    assert(!tm_mouse_report_make(128,0,0,&proposed));
    assert(!tm_mouse_report_make(0,0,4,&proposed));
    assert(!tm_mouse_report_make(0,0,0,NULL));
    assert(tiltmouse_usb_hid_mouse_make_report(false,false,-128,0).x==-128);
    proposed=tm_mouse_report_release();
    assert(proposed.kind==TM_REPORT_RELEASE_ALL && proposed.buttons==0 && proposed.dx==0);

    tiltmouse_mouse_motion_config_t cfg=tiltmouse_mouse_motion_default_config();
    cfg.x.deadzone_deg=0;cfg.x.max_tilt_deg=10;cfg.x.response_exponent=1;
    cfg.x.gain_counts_per_s=10;cfg.x.max_velocity_counts_per_s=10;
    cfg.max_report_dt_s=.1f;
    tiltmouse_orientation_pose_t pose={.roll_deg=2.5f,.neutral_valid=true};
    tiltmouse_mouse_motion_state_t state={0};
    tiltmouse_mouse_delta_t delta={0};tiltmouse_mouse_velocity_t velocity={0};
    int sum=0;
    for (int i=0;i<4;++i) {
        assert(tiltmouse_mouse_motion_step(&state,&cfg,&pose,.1f,&delta,&velocity)==
               TILTMOUSE_MOUSE_MOTION_OK);
        sum+=delta.x;
    }
    assert(sum==1 && fabsf(state.residual_x)<.00001f);
    /* The caller's output is unchanged on early invalid-argument return. */
    delta.x=77;delta.y=66;
    assert(tiltmouse_mouse_motion_step(&state,&cfg,&pose,-1,&delta,&velocity)==
           TILTMOUSE_MOUSE_MOTION_ERR_INVALID_ARGUMENT);
    assert(delta.x==77 && delta.y==66);
    cfg.x.gain_counts_per_s=1000;cfg.x.max_velocity_counts_per_s=1000;
    cfg.max_report_dt_s=1;cfg.max_delta_per_report=10;pose.roll_deg=10;
    assert(tiltmouse_mouse_motion_step(&state,&cfg,&pose,1,&delta,&velocity)==
           TILTMOUSE_MOUSE_MOTION_OK);
    assert(delta.x==10 && state.residual_x==0); /* clipped excess is discarded */
    tiltmouse_mouse_motion_reset(&state);assert(state.residual_x==0);
    printf("{\"payload_equivalence_cases\":%u,\"backend_attempts\":%u,"
           "\"backend_acceptances\":%u,\"fractional_four_steps_sum\":%d,"
           "\"early_error_preserves_sentinel_delta\":true,"
           "\"failed_release_not_retained_by_baseline\":true,"
           "\"scope\":\"original source + fake backend, not TinyUSB target\"}\n",
           comparisons,attempts,accepted,sum);
    return 0;
}
