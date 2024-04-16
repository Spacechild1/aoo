/* Copyright (c) 2010-Now Christof Ressi, Winfried Ritsch and others. 
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

#include "aoo_common.hpp"

#include "common/time.hpp"

#if AOO_USE_CODEC_OPUS
#include "aoo/codec/aoo_opus.h"
#endif

#include "string.h"
#include "stdlib.h"

#ifdef _WIN32
# include <windows.h>
#else // _WIN32
# include <dlfcn.h>
#endif

// setup function
#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#elif __GNUC__ >= 4
#define EXPORT __attribute__((visibility("default")))
#else
#define EXPORT
#endif

t_class *dejitter_class;

struct t_dejitter
{
    static constexpr const char *bindsym = "aoo dejitter";

    t_dejitter();
    ~t_dejitter();

    aoo::time_tag osctime() const {
        return d_osctime_adjusted;
    }

    t_pd d_header;
    int d_refcount;
private:
    t_clock *d_clock;
    aoo::time_tag d_last_osctime;
    aoo::time_tag d_osctime_adjusted;
    double d_last_big_delta = 0;
#if DEJITTER_CHECK
    double d_last_logical_time = -1;
#endif

    void update();

    static void tick(t_dejitter *x) {
        x->update();
        clock_delay(x->d_clock, sys_getblksize()); // once per DSP tick
    }
};

t_dejitter::t_dejitter()
    : d_header(dejitter_class), d_refcount(1)
{
    pd_bind(&d_header, gensym(bindsym));
    d_clock = clock_new(this, (t_method)tick);
    clock_setunit(d_clock, 1, 1); // use samples
    clock_delay(d_clock, 0);
}

t_dejitter::~t_dejitter(){
    pd_unbind(&d_header, gensym(bindsym));
    clock_free(d_clock);
}

// This is called exactly once per DSP tick and before any other clocks in AOO objects.
void t_dejitter::update()
{
#if DEJITTER_CHECK
    // check if this is really called once per DSP tick
    auto logical_time = clock_getlogicaltime();
    if (logical_time == d_last_logical_time) {
        bug("aoo dejitter");
    }
    d_last_logical_time = logical_time;
#endif
    aoo::time_tag osctime = aoo::time_tag::now();
    if (d_last_osctime.is_empty()) {
        d_osctime_adjusted = osctime;
    } else {
    #if DEJITTER_DEBUG
        auto last_adjusted = d_osctime_adjusted;
    #endif
        auto delta = aoo::time_tag::duration(d_last_osctime, osctime);
        auto period = (double)sys_getblksize() / (double)sys_getsr();
        // check if the current delta is lower than the nominal delta (with some tolerance).
        // If this is the case, we advance the adjusted OSC time by the nominal delta.
        if ((delta / period) < (1.0 - DEJITTER_TOLERANCE)) {
            // Don't advance if the previous big delta was larger than DEJITTER_MAXDELTA;
            // this makes sure that we catch up if the scheduler blocked.
            if (d_last_big_delta <= DEJITTER_MAXDELTA) {
                d_osctime_adjusted += aoo::time_tag::from_seconds(period);
            } else if (osctime > d_osctime_adjusted) {
                // set to actual time, but only if larger.
                d_osctime_adjusted = osctime;
            }
        } else {
            // set to actual time, but only if larger; never let time go backwards!
            if (osctime > d_osctime_adjusted) {
                d_osctime_adjusted = osctime;
            }
            d_last_big_delta = delta;
        }
    #if DEJITTER_DEBUG
        auto adjusted_delta = aoo::time_tag::duration(last_adjusted, d_osctime_adjusted);
        if (adjusted_delta == 0) {
            // get actual (negative) delta
            adjusted_delta = aoo::time_tag::duration(osctime, last_adjusted);
        }
        auto error = std::abs(adjusted_delta - period);
        LOG_DEBUG("dejitter: real delta: " << (delta * 1000.0)
                  << " ms, adjusted delta: " << (adjusted_delta * 1000.0)
                  << " ms, error: " << (error * 1000.0) << " ms");
        LOG_DEBUG("dejitter: real time: " << osctime
                  << ", adjusted time: " << d_osctime_adjusted);
    #endif
    }
    d_last_osctime = osctime;
}

t_dejitter * dejitter_get() {
    auto x = (t_dejitter *)pd_findbyclass(gensym(t_dejitter::bindsym), dejitter_class);
    if (x) {
        x->d_refcount++;
    } else {
        x = new t_dejitter();
    }
    return x;
}

void dejitter_release(t_dejitter *x) {
    if (--x->d_refcount == 0) {
        // last instance
        delete x;
    } else if (x->d_refcount < 0) {
        bug("dejitter_release: negative refcount!");
    }
}

uint64_t dejitter_osctime(t_dejitter *x) {
    return x->osctime();
}

static void aoo_dejitter_setup(){
    dejitter_class = class_new(gensym("aoo dejitter"), 0, 0,
                               sizeof(t_dejitter), CLASS_PD, A_NULL);
}

// make sure we only actually query the time once per DSP tick.
// This is used in aoo_send~ and aoo_receive~, but also in aoo_client,
// if dejitter is disabled.
uint64_t get_osctime(){
    thread_local double lastclocktime = -1;
    thread_local aoo::time_tag osctime;

    auto now = clock_getlogicaltime();
    if (now != lastclocktime){
        osctime = aoo::time_tag::now();
        lastclocktime = now;
    }
    return osctime;
}

static AooNtpTime g_start_time;

double get_elapsed_ms(AooNtpTime tt) {
    return aoo::time_tag::duration(g_start_time, tt);
}

//---------------------------------------------------//

static t_class *aoo_class;

struct t_aoo
{
    t_object x_obj;
};

void aoo_multichannel(t_aoo *x)
{
    t_atom a;
#ifdef PD_HAVE_MULTICHANNEL
    SETFLOAT(&a, g_signal_setmultiout != nullptr);
#else
    SETFLOAT(&a, -1); // compiled without multichannel support
#endif
    outlet_anything(x->x_obj.ob_outlet, gensym("multichannel"), 1, &a);
}

void * aoo_new() {
    auto x = (t_aoo *)pd_new(aoo_class);
    outlet_new(&x->x_obj, &s_list);
    return x;
}

//---------------------------------------------------//

t_signal_setmultiout g_signal_setmultiout;

void aoo_send_tilde_setup(void);
void aoo_receive_tilde_setup(void);
void aoo_node_setup(void);
void aoo_server_setup(void);
void aoo_client_setup(void);

extern "C" EXPORT void aoo_setup(void)
{
    post("AOO (audio over OSC) %s", aoo_getVersionString());
    post("  (c) 2020 Christof Ressi, Winfried Ritsch, et al.");
    startpost("Available codecs: PCM");
#if AOO_USE_CODEC_OPUS
    startpost(", %s", opus_get_version_string());
#endif
    endpost();

    aoo_initialize(NULL);

    if (auto [ok, msg] = aoo::check_ntp_server(); ok){
        post("%s", msg.c_str());
    } else {
        pd_error(0, "%s", msg.c_str());
    }

    post("");

    g_start_time = aoo::time_tag::now();

#ifdef PD_HAVE_MULTICHANNEL
    // runtime check for multichannel support:
#ifdef _WIN32
    // get a handle to the module containing the Pd API functions.
    // NB: GetModuleHandle("pd.dll") does not cover all cases.
    HMODULE module;
    if (GetModuleHandleEx(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCSTR)&pd_typedmess, &module)) {
        g_signal_setmultiout = (t_signal_setmultiout)(void *)GetProcAddress(
            module, "signal_setmultiout");
    }
#else
    // search recursively, starting from the main program
    g_signal_setmultiout = (t_signal_setmultiout)dlsym(
        dlopen(nullptr, RTLD_NOW), "signal_setmultiout");
#endif
#endif // PD_HAVE_MULTICHANNEL

    aoo_class = class_new(gensym("aoo"), (t_newmethod)aoo_new, 0,
                          sizeof(t_aoo), 0, A_NULL);
    class_addmethod(aoo_class, (t_method)aoo_multichannel, gensym("multichannel"), A_NULL);

    aoo_dejitter_setup();
    aoo_node_setup();

    aoo_send_tilde_setup();
    aoo_receive_tilde_setup();
    aoo_server_setup();
    aoo_client_setup();
}
