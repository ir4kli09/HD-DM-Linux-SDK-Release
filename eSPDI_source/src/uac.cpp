#include "uac/aconfig.h"
#include <stdio.h>
#include <malloc.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <locale.h>
#include <alsa/asoundlib.h>
#include <assert.h>
#include <termios.h>
#include <signal.h>
#include <sys/poll.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <endian.h>
#include "uac/gettext.h"
#include "uac/formats.h"
#include "uac/version.h"
#include <sys/time.h>
#include "uac.h"

#ifdef SND_CHMAP_API_VERSION
#define CONFIG_SUPPORT_CHMAP	1
#endif

#ifndef LLONG_MAX
#define LLONG_MAX    9223372036854775807LL
#endif

#ifndef le16toh
#include <asm/byteorder.h>
#define le16toh(x) __le16_to_cpu(x)
#define be16toh(x) __be16_to_cpu(x)
#define le32toh(x) __le32_to_cpu(x)
#define be32toh(x) __be32_to_cpu(x)
#endif

#define DEFAULT_FORMAT		SND_PCM_FORMAT_U8
#define DEFAULT_SPEED 		8000

#define FORMAT_DEFAULT		-1
#define FORMAT_RAW		0
#define FORMAT_VOC		1
#define FORMAT_WAVE		2
#define FORMAT_AU		3

static void init_stdin(void);

/* global data */

static snd_pcm_sframes_t (*readi_func)(snd_pcm_t *handle, void *buffer, snd_pcm_uframes_t size);
static snd_pcm_sframes_t (*writei_func)(snd_pcm_t *handle, const void *buffer, snd_pcm_uframes_t size);
static snd_pcm_sframes_t (*readn_func)(snd_pcm_t *handle, void **bufs, snd_pcm_uframes_t size);
static snd_pcm_sframes_t (*writen_func)(snd_pcm_t *handle, void **bufs, snd_pcm_uframes_t size);

enum {
    VUMETER_NONE,
    VUMETER_MONO,
    VUMETER_STEREO
};

static void set_params(void);
static char *command;
static snd_pcm_t *handle;
static struct {
    snd_pcm_format_t format;
    unsigned int channels;
    unsigned int rate;
} hwparams, rhwparams;
//static int timelimit = 0;
//static int sampleslimit = 0;
static int quiet_mode = 0;
static int file_type = FORMAT_DEFAULT;
static int open_mode = SND_PCM_NONBLOCK;
static snd_pcm_stream_t stream = SND_PCM_STREAM_PLAYBACK;
static int mmap_flag = 0;
static int interleaved = 1;
static int nonblock = 0;
static volatile sig_atomic_t in_aborting = 0;
static u_char *audiobuf = NULL;
static snd_pcm_uframes_t chunk_size = 0;
static unsigned period_time = 0;
static unsigned buffer_time = 0;
static snd_pcm_uframes_t period_frames = 0;
static snd_pcm_uframes_t buffer_frames = 4000;
static int avail_min = -1;
static int start_delay = 0;
static int stop_delay = 0;
static int monotonic = 0;
static int interactive = 0;
static int can_pause = 0;
static int fatal_errors = 0;
static int verbose = 0;
static int vumeter = VUMETER_NONE;
//static int buffer_pos = 0;
static size_t significant_bits_per_sample, bits_per_sample, bits_per_frame;
static size_t chunk_bytes;
static int test_position = 0;
static int test_coef = 8;
static int test_nowait = 0;
static snd_output_t *log;
//static long long max_file_size = 0;
//static int max_file_time = 0;
//static int use_strftime = 0;
volatile static int recycle_capture_file = 0;
static long term_c_lflag = -1;
//static int dump_hw_params = 0;

static int fd = -1;
//static off64_t pbrec_count = LLONG_MAX, fdcount;
//static int vocmajor, vocminor;

static char *pidfile_name = NULL;
FILE *pidf = NULL;
static int pidfile_written = 0;

#ifdef CONFIG_SUPPORT_CHMAP
static snd_pcm_chmap_t *channel_map = NULL; /* chmap to override */
static unsigned int *hw_map = NULL; /* chmap to follow */
#endif

/* needed prototypes */

static void done_stdin(void);

// static void capture(char *filename);
// static void capturev(char **filenames, unsigned int count);

static void suspend(void);
#if 0
static const struct fmt_capture {
    void (*start) (int fd, size_t count);
    void (*end) (int fd);
    char *what;
    long long max_filesize;
} fmt_rec_table[] = {
    {	NULL,		NULL,		N_("raw data"),		LLONG_MAX },
};
#endif
#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 95)
#define error(...) do {\
    fprintf(stderr, "%s: %s:%d: ", command, __FUNCTION__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); \
    putc('\n', stderr); \
} while (0)
#else
#define error(args...) do {\
    fprintf(stderr, "%s: %s:%d: ", command, __FUNCTION__, __LINE__); \
    fprintf(stderr, ##args); \
    putc('\n', stderr); \
} while (0)
#endif

/*
 *	Subroutine to clean up before exit.
 */
static void prg_exit(int code)
{
    done_stdin();
    if (handle)
        snd_pcm_close(handle);
    if (pidfile_written)
        remove (pidfile_name);
    //exit(code);
    for(;;);
}
#if 0 // not used
static void signal_handler(int sig)
{
    if (in_aborting)
        return;

    in_aborting = 1;
    if (verbose==2)
        putchar('\n');
    if (!quiet_mode)
        fprintf(stderr, _("Aborted by signal %s...\n"), strsignal(sig));
    if (handle)
        snd_pcm_abort(handle);
    if (sig == SIGABRT) {
        /* do not call snd_pcm_close() and abort immediately */
        handle = NULL;
        prg_exit(EXIT_FAILURE);
    }
    signal(sig, SIG_DFL);
}

/* call on SIGUSR1 signal. */
static void signal_handler_recycle (int sig)
{
    /* flag the capture loop to start a new output file */
    recycle_capture_file = 1;
}
#endif
enum {
    OPT_VERSION = 1,
    OPT_PERIOD_SIZE,
    OPT_BUFFER_SIZE,
    OPT_DISABLE_RESAMPLE,
    OPT_DISABLE_CHANNELS,
    OPT_DISABLE_FORMAT,
    OPT_DISABLE_SOFTVOL,
    OPT_TEST_POSITION,
    OPT_TEST_COEF,
    OPT_TEST_NOWAIT,
    OPT_MAX_FILE_TIME,
    OPT_PROCESS_ID_FILE,
    OPT_USE_STRFTIME,
    OPT_DUMP_HWPARAMS,
    OPT_FATAL_ERRORS,
};


int get_device_name(char *getName, char *deviceName)
{
    snd_ctl_t *handle;
    int card, err, dev;
    snd_ctl_card_info_t *info;
    snd_pcm_info_t *pcminfo;
    snd_ctl_card_info_alloca(&info);
    snd_pcm_info_alloca(&pcminfo);
    stream = SND_PCM_STREAM_CAPTURE;

    card = -1;
    if (snd_card_next(&card) < 0 || card < 0) {
        error(_("no soundcards found..."));
        return -1;
    }

    while (card >= 0) {
        char name[32];
        sprintf(name, "hw:%d", card);
        printf("name: %s\n", name);
        if ((err = snd_ctl_open(&handle, name, 0)) < 0) {
            error("control open (%i): %s", card, snd_strerror(err));
            goto next_card;
        }
        if ((err = snd_ctl_card_info(handle, info)) < 0) {
            error("control hardware info (%i): %s", card, snd_strerror(err));
            snd_ctl_close(handle);
            goto next_card;
        }
        dev = -1;
        while (1) {
            if (snd_ctl_pcm_next_device(handle, &dev)<0)
                error("snd_ctl_pcm_next_device");
            if (dev < 0)
                break;
            snd_pcm_info_set_device(pcminfo, dev);
            snd_pcm_info_set_subdevice(pcminfo, 0);
            snd_pcm_info_set_stream(pcminfo, stream);
            if ((err = snd_ctl_pcm_info(handle, pcminfo)) < 0) {
                if (err != -ENOENT)
                    error("control digital audio info (%i): %s", card, snd_strerror(err));
                continue;
            }

            if (strcmp(deviceName, snd_ctl_card_info_get_name(info)) == 0) {
                sprintf(getName ,"hw:%d,0", card);
                printf("getName: %s\n", getName);
                return 0;
            }
        }
        snd_ctl_close(handle);
    next_card:
        if (snd_card_next(&card) < 0) {
            error("snd_card_next");
            break;
        }
    }
    return -1;
}

int uac_name(char *inputName, char *outputName)
{
    if(get_device_name(outputName, inputName) != 0)
        return -1;
    return 0;
}

int uac_init(char *deviceName)
{
    char *pcm_name;
    int err;

    snd_pcm_info_t *info;
    char buffer[32];
    char *device_name;

    if(get_device_name(buffer, deviceName) != 0)
        return -1;

    device_name = buffer;
    snd_pcm_info_alloca(&info);

    err = snd_output_stdio_attach(&log, stderr, 0);
    chunk_size = -1;
    stream = SND_PCM_STREAM_CAPTURE;
    pcm_name = device_name;
    rhwparams.format = SND_PCM_FORMAT_S16_LE;
    nonblock = 1;
	open_mode |= SND_PCM_NONBLOCK;
	rhwparams.format = SND_PCM_FORMAT_S16_LE;
	file_type = SND_PCM_FORMAT_S16_LE;
    rhwparams.rate = 32000;
    rhwparams.channels = 2;
    start_delay = 1;
    printf("open_mode = %d\n", open_mode);
    printf("pcm_name = %s\n", pcm_name);
    err = snd_pcm_open(&handle, pcm_name, stream, open_mode);
    if (err < 0) {
        error(_("audio open error: %s"), snd_strerror(err));
        return -1;
    }

    if ((err = snd_pcm_info(handle, info)) < 0) {
        error(_("info error: %s"), snd_strerror(err));
        return 1;
    }

    err = snd_pcm_nonblock(handle, 0);
    if (err < 0) {
        error(_("nonblock setting error: %s"), snd_strerror(err));
        return 1;
    }

    chunk_size = 1024;
    audiobuf = (u_char *)malloc(1024);
    if (audiobuf == NULL) {
        error(_("not enough memory"));
        return 1;
    }

    writei_func = snd_pcm_writei;
    readi_func = snd_pcm_readi;
    writen_func = snd_pcm_writen;
    readn_func = snd_pcm_readn;
    hwparams = rhwparams;   
    init_stdin();
    return 0;
}

static int setup_chmap(void)
{
    snd_pcm_chmap_t *chmap = channel_map;
    char mapped[hwparams.channels];
    snd_pcm_chmap_t *hw_chmap;
    unsigned int ch, i;
    int err;

    if (!chmap)
        return 0;

    if (chmap->channels != hwparams.channels) {
        error(_("Channel numbers don't match between hw_params and channel map"));
        return -1;
    }
    err = snd_pcm_set_chmap(handle, chmap);
    if (!err)
        return 0;

    hw_chmap = snd_pcm_get_chmap(handle);
    if (!hw_chmap) {
        fprintf(stderr, _("Warning: unable to get channel map\n"));
        return 0;
    }

    if (hw_chmap->channels == chmap->channels &&
        !memcmp(hw_chmap, chmap, 4 * (chmap->channels + 1))) {
        /* maps are identical, so no need to convert */
        free(hw_chmap);
        return 0;
    }

    hw_map =(unsigned int *)calloc(hwparams.channels, sizeof(int));
    if (!hw_map) {
        error(_("not enough memory"));
        return -1;
    }

    memset(mapped, 0, sizeof(mapped));
    for (ch = 0; ch < hw_chmap->channels; ch++) {
        if (chmap->pos[ch] == hw_chmap->pos[ch]) {
            mapped[ch] = 1;
            hw_map[ch] = ch;
            continue;
        }
        for (i = 0; i < hw_chmap->channels; i++) {
            if (!mapped[i] && chmap->pos[ch] == hw_chmap->pos[i]) {
                mapped[i] = 1;
                hw_map[ch] = i;
                break;
            }
        }
        if (i >= hw_chmap->channels) {
            char buf[256];
            error(_("Channel %d doesn't match with hw_parmas"), ch);
            snd_pcm_chmap_print(hw_chmap, sizeof(buf), buf);
            fprintf(stderr, "hardware chmap = %s\n", buf);
            return -1;
        }
    }
    free(hw_chmap);
    return 0;
}

static void set_params(void)
{
    snd_pcm_hw_params_t *params;
    snd_pcm_sw_params_t *swparams;
    snd_pcm_uframes_t buffer_size;
    int err;
    size_t n;

    unsigned int rate;
    snd_pcm_uframes_t start_threshold, stop_threshold;
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_sw_params_alloca(&swparams);
    err = snd_pcm_hw_params_any(handle, params);
    if (err < 0) {
        error(_("Broken configuration for this PCM: no configurations available"));
        prg_exit(EXIT_FAILURE);
    }

    if (mmap_flag) {
        snd_pcm_access_mask_t *mask = (snd_pcm_access_mask_t *)alloca(snd_pcm_access_mask_sizeof());
        snd_pcm_access_mask_none(mask);
        snd_pcm_access_mask_set(mask, SND_PCM_ACCESS_MMAP_INTERLEAVED);
        snd_pcm_access_mask_set(mask, SND_PCM_ACCESS_MMAP_NONINTERLEAVED);
        snd_pcm_access_mask_set(mask, SND_PCM_ACCESS_MMAP_COMPLEX);
        err = snd_pcm_hw_params_set_access_mask(handle, params, mask);
    } else if (interleaved) {
        err = snd_pcm_hw_params_set_access(handle, params,
                           SND_PCM_ACCESS_RW_INTERLEAVED);
    } else {
        err = snd_pcm_hw_params_set_access(handle, params,
                           SND_PCM_ACCESS_RW_NONINTERLEAVED);
    }
    if (err < 0) {
        error(_("Access type not available"));
        prg_exit(EXIT_FAILURE);
    }
    err = snd_pcm_hw_params_set_format(handle, params, hwparams.format);
    if (err < 0) {
        error(_("Sample format non available"));
        prg_exit(EXIT_FAILURE);
    }
    err = snd_pcm_hw_params_set_channels(handle, params, hwparams.channels);
    if (err < 0) {
        error(_("Channels count non available"));
        prg_exit(EXIT_FAILURE);
    }

#if 0
    err = snd_pcm_hw_params_set_periods_min(handle, params, 2);
    assert(err >= 0);
#endif
    rate = hwparams.rate;
    err = snd_pcm_hw_params_set_rate_near(handle, params, &hwparams.rate, 0);
    assert(err >= 0);
    if ((float)rate * 1.05 < hwparams.rate || (float)rate * 0.95 > hwparams.rate) {
        if (!quiet_mode) {
            char plugex[64];
            const char *pcmname = snd_pcm_name(handle);
            fprintf(stderr, _("Warning: rate is not accurate (requested = %iHz, got = %iHz)\n"), rate, hwparams.rate);
            if (! pcmname || strchr(snd_pcm_name(handle), ':'))
                *plugex = 0;
            else
                snprintf(plugex, sizeof(plugex), "(-Dplug:%s)",
                     snd_pcm_name(handle));
            fprintf(stderr, _("         please, try the plug plugin %s\n"),
                plugex);
        }
    }
    rate = hwparams.rate;
    if (buffer_time == 0 && buffer_frames == 0) {
        err = snd_pcm_hw_params_get_buffer_time_max(params,
                                &buffer_time, 0);
        assert(err >= 0);
        if (buffer_time > 500000)
            buffer_time = 500000;
    }
    if (period_time == 0 && period_frames == 0) {
        if (buffer_time > 0)
            period_time = buffer_time / 4;
        else
            period_frames = buffer_frames / 4;
    }

    if (period_time > 0)
        err = snd_pcm_hw_params_set_period_time_near(handle, params,
                                 &period_time, 0);
    else
        err = snd_pcm_hw_params_set_period_size_near(handle, params,
                                 &period_frames, 0);
    assert(err >= 0);
    if (buffer_time > 0) {
        err = snd_pcm_hw_params_set_buffer_time_near(handle, params,
                                 &buffer_time, 0);
    } else {
        err = snd_pcm_hw_params_set_buffer_size_near(handle, params,
                                 &buffer_frames);
    }

    err = snd_pcm_hw_params_get_period_size(params, &chunk_size, 0);

    assert(err >= 0);
    monotonic = snd_pcm_hw_params_is_monotonic(params);

    can_pause = snd_pcm_hw_params_can_pause(params);

    err = snd_pcm_hw_params(handle, params);
    if (err < 0) {
        error(_("Unable to install hw params:"));
        snd_pcm_hw_params_dump(params, log);
        prg_exit(EXIT_FAILURE);
    }

    err = snd_pcm_hw_params_get_period_size(params, &chunk_size, 0);

    if (err < 0) {
        error(_("snd_pcm_hw_params_get_period_size not available"));
        prg_exit(EXIT_FAILURE);
    }

    err = snd_pcm_hw_params_get_buffer_size(params, &buffer_size);
    if (err < 0) {
        error(_("snd_pcm_hw_params_get_period_size not available"));
        prg_exit(EXIT_FAILURE);
    }
    if (chunk_size == buffer_size) {
        error(_("Can't use period equal to buffer size (%lu == %lu)"),
              chunk_size, buffer_size);
        prg_exit(EXIT_FAILURE);
    }
    printf("chunk_size = %ld\n", chunk_size);
    printf("buffer_size = %ld\n", buffer_size);
    snd_pcm_sw_params_current(handle, swparams);
    if (avail_min < 0)
        n = chunk_size;
    else
        n = (double) rate * avail_min / 1000000;
    err = snd_pcm_sw_params_set_avail_min(handle, swparams, n);

    /* round up to closest transfer boundary */
    n = buffer_size;
    if (start_delay <= 0) {
        start_threshold = n + (double) rate * start_delay / 1000000;
    } else
        start_threshold = (double) rate * start_delay / 1000000;
    if (start_threshold < 1)
        start_threshold = 1;
    if (start_threshold > n)
        start_threshold = n;
    err = snd_pcm_sw_params_set_start_threshold(handle, swparams, start_threshold);
    assert(err >= 0);
    if (stop_delay <= 0)
        stop_threshold = buffer_size + (double) rate * stop_delay / 1000000;
    else
        stop_threshold = (double) rate * stop_delay / 1000000;
    err = snd_pcm_sw_params_set_stop_threshold(handle, swparams, stop_threshold);
    assert(err >= 0);

    if (snd_pcm_sw_params(handle, swparams) < 0) {
        error(_("unable to install sw params:"));
        snd_pcm_sw_params_dump(swparams, log);
        prg_exit(EXIT_FAILURE);
    }

    if (setup_chmap())
        prg_exit(EXIT_FAILURE);

    if (verbose)
        snd_pcm_dump(handle, log);

    bits_per_sample = snd_pcm_format_physical_width(hwparams.format);
    significant_bits_per_sample = snd_pcm_format_width(hwparams.format);
    bits_per_frame = bits_per_sample * hwparams.channels;
    chunk_bytes = chunk_size * bits_per_frame / 8;

    audiobuf = (unsigned char *)realloc(audiobuf, chunk_bytes);
    if (audiobuf == NULL) {
        error(_("not enough memory"));
        prg_exit(EXIT_FAILURE);
    }
    // fprintf(stderr, "real chunk_size = %i, frags = %i, total = %i\n", chunk_size, setup.buf.block.frags, setup.buf.block.frags * chunk_size);

    /* stereo VU-meter isn't always available... */
    if (vumeter == VUMETER_STEREO) {
        if (hwparams.channels != 2 || !interleaved || verbose > 2)
            vumeter = VUMETER_MONO;
    }

    /* show mmap buffer arragment */
    if (mmap_flag && verbose) {
        const snd_pcm_channel_area_t *areas;
        snd_pcm_uframes_t offset, size = chunk_size;
        int i;
        err = snd_pcm_mmap_begin(handle, &areas, &offset, &size);
        if (err < 0) {
            error(_("snd_pcm_mmap_begin problem: %s"), snd_strerror(err));
            prg_exit(EXIT_FAILURE);
        }
        for (i = 0; i < (int)hwparams.channels; i++)
            fprintf(stderr, "mmap_area[%i] = %p,%u,%u (%u)\n", i, areas[i].addr, areas[i].first, areas[i].step, snd_pcm_format_physical_width(hwparams.format));
        /* not required, but for sure */
        snd_pcm_mmap_commit(handle, offset, 0);
    }

    buffer_frames = buffer_size;	/* for position test */
}

static void init_stdin(void)
{
    struct termios term;
    long flags;

    if (!interactive)
        return;
    if (!isatty(fileno(stdin))) {
        interactive = 0;
        return;
    }
    tcgetattr(fileno(stdin), &term);
    term_c_lflag = term.c_lflag;
    if (fd == fileno(stdin))
        return;
    flags = fcntl(fileno(stdin), F_GETFL);
    if (flags < 0 || fcntl(fileno(stdin), F_SETFL, flags|O_NONBLOCK) < 0)
        fprintf(stderr, _("stdin O_NONBLOCK flag setup failed\n"));
    term.c_lflag &= ~ICANON;
    tcsetattr(fileno(stdin), TCSANOW, &term);
}

static void done_stdin(void)
{
    struct termios term;

    if (!interactive)
        return;
    if (fd == fileno(stdin) || term_c_lflag == -1)
        return;
    tcgetattr(fileno(stdin), &term);
    term.c_lflag = term_c_lflag;
    tcsetattr(fileno(stdin), TCSANOW, &term);
}

static void do_pause(void)
{
    int err;
    unsigned char b;

    if (!can_pause) {
        fprintf(stderr, _("\rPAUSE command ignored (no hw support)\n"));
        return;
    }
    if (snd_pcm_state(handle) == SND_PCM_STATE_SUSPENDED)
        suspend();

    err = snd_pcm_pause(handle, 1);
    if (err < 0) {
        error(_("pause push error: %s"), snd_strerror(err));
        return;
    }
    while (1) {
        while (read(fileno(stdin), &b, 1) != 1);
        if (b == ' ' || b == '\r') {
            while (read(fileno(stdin), &b, 1) == 1);
            if (snd_pcm_state(handle) == SND_PCM_STATE_SUSPENDED)
                suspend();
            err = snd_pcm_pause(handle, 0);
            if (err < 0)
                error(_("pause release error: %s"), snd_strerror(err));
            return;
        }
    }
}

static void check_stdin(void)
{
    unsigned char b;

    if (!interactive)
        return;
    if (fd != fileno(stdin)) {
        while (read(fileno(stdin), &b, 1) == 1) {
            if (b == ' ' || b == '\r') {
                while (read(fileno(stdin), &b, 1) == 1);
                fprintf(stderr, _("\r=== PAUSE ===                                                            "));
                fflush(stderr);
            do_pause();
                fprintf(stderr, "                                                                          \r");
                fflush(stderr);
            }
        }
    }
}

#ifndef timersub
#define	timersub(a, b, result) \
do { \
    (result)->tv_sec = (a)->tv_sec - (b)->tv_sec; \
    (result)->tv_usec = (a)->tv_usec - (b)->tv_usec; \
    if ((result)->tv_usec < 0) { \
        --(result)->tv_sec; \
        (result)->tv_usec += 1000000; \
    } \
} while (0)
#endif

#ifndef timermsub
#define	timermsub(a, b, result) \
do { \
    (result)->tv_sec = (a)->tv_sec - (b)->tv_sec; \
    (result)->tv_nsec = (a)->tv_nsec - (b)->tv_nsec; \
    if ((result)->tv_nsec < 0) { \
        --(result)->tv_sec; \
        (result)->tv_nsec += 1000000000L; \
    } \
} while (0)
#endif

/* I/O error handler */
static void xrun(void)
{
    snd_pcm_status_t *status;
    int res;

    snd_pcm_status_alloca(&status);
    if ((res = snd_pcm_status(handle, status))<0) {
        error(_("status error: %s"), snd_strerror(res));
        prg_exit(EXIT_FAILURE);
    }
    if (snd_pcm_status_get_state(status) == SND_PCM_STATE_XRUN) {
        if (fatal_errors) {
            error(_("fatal %s: %s"),
                    stream == SND_PCM_STREAM_PLAYBACK ? _("underrun") : _("overrun"),
                    snd_strerror(res));
            prg_exit(EXIT_FAILURE);
        }
        if (monotonic) {
#ifdef HAVE_CLOCK_GETTIME
            struct timespec now, diff, tstamp;
            clock_gettime(CLOCK_MONOTONIC, &now);
            snd_pcm_status_get_trigger_htstamp(status, &tstamp);
            timermsub(&now, &tstamp, &diff);
            fprintf(stderr, _("%s!!! (at least %.3f ms long)\n"),
                stream == SND_PCM_STREAM_PLAYBACK ? _("underrun") : _("overrun"),
                diff.tv_sec * 1000 + diff.tv_nsec / 1000000.0);
#else
            fprintf(stderr, "%s !!!\n", _("underrun"));
#endif
        } else {
            struct timeval now, diff, tstamp;
            gettimeofday(&now, 0);
            snd_pcm_status_get_trigger_tstamp(status, &tstamp);
            timersub(&now, &tstamp, &diff);
            fprintf(stderr, _("%s!!! (at least %.3f ms long)\n"),
                stream == SND_PCM_STREAM_PLAYBACK ? _("underrun") : _("overrun"),
                diff.tv_sec * 1000 + diff.tv_usec / 1000.0);
        }
        if (verbose) {
            fprintf(stderr, _("Status:\n"));
            snd_pcm_status_dump(status, log);
        }
        if ((res = snd_pcm_prepare(handle))<0) {
            error(_("xrun: prepare error: %s"), snd_strerror(res));
            prg_exit(EXIT_FAILURE);
        }
        return;		/* ok, data should be accepted again */
    } if (snd_pcm_status_get_state(status) == SND_PCM_STATE_DRAINING) {
        if (verbose) {
            fprintf(stderr, _("Status(DRAINING):\n"));
            snd_pcm_status_dump(status, log);
        }
        if (stream == SND_PCM_STREAM_CAPTURE) {
            fprintf(stderr, _("capture stream format change? attempting recover...\n"));
            if ((res = snd_pcm_prepare(handle))<0) {
                error(_("xrun(DRAINING): prepare error: %s"), snd_strerror(res));
                prg_exit(EXIT_FAILURE);
            }
            return;
        }
    }
    if (verbose) {
        fprintf(stderr, _("Status(R/W):\n"));
        snd_pcm_status_dump(status, log);
    }
    error(_("read/write error, state = %s"), snd_pcm_state_name(snd_pcm_status_get_state(status)));
    prg_exit(EXIT_FAILURE);
}

/* I/O suspend handler */
static void suspend(void)
{
    int res;

    if (!quiet_mode)
        fprintf(stderr, _("Suspended. Trying resume. ")); fflush(stderr);
    while ((res = snd_pcm_resume(handle)) == -EAGAIN)
        sleep(1);	/* wait until suspend flag is released */
    if (res < 0) {
        if (!quiet_mode)
            fprintf(stderr, _("Failed. Restarting stream. ")); fflush(stderr);
        if ((res = snd_pcm_prepare(handle)) < 0) {
            error(_("suspend: prepare error: %s"), snd_strerror(res));
            prg_exit(EXIT_FAILURE);
        }
    }
    if (!quiet_mode)
        fprintf(stderr, _("Done.\n"));
}

static void print_vu_meter_mono(int perc, int maxperc)
{
    const int bar_length = 50;
    char line[80];
    int val;

    for (val = 0; val <= perc * bar_length / 100 && val < bar_length; val++)
        line[val] = '#';
    for (; val <= maxperc * bar_length / 100 && val < bar_length; val++)
        line[val] = ' ';
    line[val] = '+';
    for (++val; val <= bar_length; val++)
        line[val] = ' ';
    if (maxperc > 99)
        sprintf(line + val, "| MAX");
    else
        sprintf(line + val, "| %02i%%", maxperc);
    fputs(line, stderr);
    if (perc > 100)
        fprintf(stderr, _(" !clip  "));
}

static void print_vu_meter_stereo(int *perc, int *maxperc)
{
    const int bar_length = 35;
    char line[80];
    int c;

    memset(line, ' ', sizeof(line) - 1);
    line[bar_length + 3] = '|';

    for (c = 0; c < 2; c++) {
        int p = perc[c] * bar_length / 100;
        char tmp[4];
        if (p > bar_length)
            p = bar_length;
        if (c)
            memset(line + bar_length + 6 + 1, '#', p);
        else
            memset(line + bar_length - p - 1, '#', p);
        p = maxperc[c] * bar_length / 100;
        if (p > bar_length)
            p = bar_length;
        if (c)
            line[bar_length + 6 + 1 + p] = '+';
        else
            line[bar_length - p - 1] = '+';
        if (maxperc[c] > 99)
            sprintf(tmp, "MAX");
        else
            sprintf(tmp, "%02d%%", maxperc[c]);
        if (c)
            memcpy(line + bar_length + 3 + 1, tmp, 3);
        else
            memcpy(line + bar_length, tmp, 3);
    }
    line[bar_length * 2 + 6 + 2] = 0;
    fputs(line, stderr);
}

static void print_vu_meter(signed int *perc, signed int *maxperc)
{
    if (vumeter == VUMETER_STEREO)
        print_vu_meter_stereo(perc, maxperc);
    else
        print_vu_meter_mono(*perc, *maxperc);
}

/* peak handler */
static void compute_max_peak(u_char *data, size_t count)
{
    signed int val, max, perc[2], max_peak[2];
    static	int	run = 0;
    size_t ocount = count;
    int	format_little_endian = snd_pcm_format_little_endian(hwparams.format);
    int ichans, c;

    if (vumeter == VUMETER_STEREO)
        ichans = 2;
    else
        ichans = 1;

    memset(max_peak, 0, sizeof(max_peak));
    switch (bits_per_sample) {
    case 8: {
        signed char *valp = (signed char *)data;
        signed char mask = snd_pcm_format_silence(hwparams.format);
        c = 0;
        while (count-- > 0) {
            val = *valp++ ^ mask;
            val = abs(val);
            if (max_peak[c] < val)
                max_peak[c] = val;
            if (vumeter == VUMETER_STEREO)
                c = !c;
        }
        break;
    }
    case 16: {
        signed short *valp = (signed short *)data;
        signed short mask = snd_pcm_format_silence_16(hwparams.format);
        signed short sval;

        count /= 2;
        c = 0;
        while (count-- > 0) {
            if (format_little_endian)
                sval = le16toh(*valp);
            else
                sval = be16toh(*valp);
            sval = abs(sval) ^ mask;
            if (max_peak[c] < sval)
                max_peak[c] = sval;
            valp++;
            if (vumeter == VUMETER_STEREO)
                c = !c;
        }
        break;
    }
    case 24: {
        unsigned char *valp = data;
        signed int mask = snd_pcm_format_silence_32(hwparams.format);

        count /= 3;
        c = 0;
        while (count-- > 0) {
            if (format_little_endian) {
                val = valp[0] | (valp[1]<<8) | (valp[2]<<16);
            } else {
                val = (valp[0]<<16) | (valp[1]<<8) | valp[2];
            }
            /* Correct signed bit in 32-bit value */
            if (val & (1<<(bits_per_sample-1))) {
                val |= 0xff<<24;	/* Negate upper bits too */
            }
            val = abs(val) ^ mask;
            if (max_peak[c] < val)
                max_peak[c] = val;
            valp += 3;
            if (vumeter == VUMETER_STEREO)
                c = !c;
        }
        break;
    }
    case 32: {
        signed int *valp = (signed int *)data;
        signed int mask = snd_pcm_format_silence_32(hwparams.format);

        count /= 4;
        c = 0;
        while (count-- > 0) {
            if (format_little_endian)
                val = le32toh(*valp);
            else
                val = be32toh(*valp);
            val = abs(val) ^ mask;
            if (max_peak[c] < val)
                max_peak[c] = val;
            valp++;
            if (vumeter == VUMETER_STEREO)
                c = !c;
        }
        break;
    }
    default:
        if (run == 0) {
            fprintf(stderr, _("Unsupported bit size %d.\n"), (int)bits_per_sample);
            run = 1;
        }
        return;
    }
    max = 1 << (significant_bits_per_sample-1);
    if (max <= 0)
        max = 0x7fffffff;

    for (c = 0; c < ichans; c++) {
        if (bits_per_sample > 16)
            perc[c] = max_peak[c] / (max / 100);
        else
            perc[c] = max_peak[c] * 100 / max;
    }

    if (interleaved && verbose <= 2) {
        static int maxperc[2];
        static time_t t=0;
        const time_t tt=time(NULL);
        if(tt>t) {
            t=tt;
            maxperc[0] = 0;
            maxperc[1] = 0;
        }
        for (c = 0; c < ichans; c++)
            if (perc[c] > maxperc[c])
                maxperc[c] = perc[c];

        putc('\r', stderr);
        print_vu_meter(perc, maxperc);
        fflush(stderr);
    }
    else if(verbose==3) {
        fprintf(stderr, _("Max peak (%li samples): 0x%08x "), (long)ocount, max_peak[0]);
        for (val = 0; val < 20; val++)
            if (val <= perc[0] / 5)
                putc('#', stderr);
            else
                putc(' ', stderr);
        fprintf(stderr, " %i%%\n", perc[0]);
        fflush(stderr);
    }
}

static void do_test_position(void)
{
    static long counter = 0;
    static time_t tmr = -1;
    time_t now;
    static float availsum, delaysum, samples;
    static snd_pcm_sframes_t maxavail, maxdelay;
    static snd_pcm_sframes_t minavail, mindelay;
    static snd_pcm_sframes_t badavail = 0, baddelay = 0;
    snd_pcm_sframes_t outofrange;
    snd_pcm_sframes_t avail, delay;
    int err;

    err = snd_pcm_avail_delay(handle, &avail, &delay);
    if (err < 0)
        return;
    outofrange = (test_coef * (snd_pcm_sframes_t)buffer_frames) / 2;
    if (avail > outofrange || avail < -outofrange ||
        delay > outofrange || delay < -outofrange) {
      badavail = avail; baddelay = delay;
      availsum = delaysum = samples = 0;
      maxavail = maxdelay = 0;
      minavail = mindelay = buffer_frames * 16;
      fprintf(stderr, _("Suspicious buffer position (%li total): "
        "avail = %li, delay = %li, buffer = %li\n"),
        ++counter, (long)avail, (long)delay, (long)buffer_frames);
    } else if (verbose) {
        time(&now);
        if (tmr == (time_t) -1) {
            tmr = now;
            availsum = delaysum = samples = 0;
            maxavail = maxdelay = 0;
            minavail = mindelay = buffer_frames * 16;
        }
        if (avail > maxavail)
            maxavail = avail;
        if (delay > maxdelay)
            maxdelay = delay;
        if (avail < minavail)
            minavail = avail;
        if (delay < mindelay)
            mindelay = delay;
        availsum += avail;
        delaysum += delay;
        samples++;
        if (avail != 0 && now != tmr) {
            fprintf(stderr, "BUFPOS: avg%li/%li "
                "min%li/%li max%li/%li (%li) (%li:%li/%li)\n",
                (long)(availsum / samples),
                (long)(delaysum / samples),
                (long)minavail, (long)mindelay,
                (long)maxavail, (long)maxdelay,
                (long)buffer_frames,
                counter, badavail, baddelay);
            tmr = now;
        }
    }
}

/*
 *  read function
 */

static ssize_t pcm_read(u_char *data, size_t rcount)
{
    ssize_t r;
    size_t result = 0;
    size_t count = rcount;

    printf("chunk_size = %ld\n", chunk_size);
    if (count != chunk_size) {
        count = chunk_size;
    }

    while (count > 0 && !in_aborting) {
        if (test_position)
            do_test_position();
        check_stdin();
        r = readi_func(handle, data, count);
        if (test_position)
            do_test_position();
        if (r == -EAGAIN || (r >= 0 && (size_t)r < count)) {
            if (!test_nowait)
                snd_pcm_wait(handle, 100);
        } else if (r == -EPIPE) {
            xrun();
        } else if (r == -ESTRPIPE) {
            suspend();
        } else if (r < 0) {
            error(_("read error: %s"), snd_strerror(r));
            prg_exit(EXIT_FAILURE);
        }
        if (r > 0) {
            if (vumeter)
                compute_max_peak(data, r * hwparams.channels);
            result += r;
            count -= r;
            data += r * bits_per_frame / 8;
        }
    }
    return rcount;
}
#if 0  // not used
static ssize_t pcm_readv(u_char **data, unsigned int channels, size_t rcount)
{
    ssize_t r;
    size_t result = 0;
    size_t count = rcount;

    if (count != chunk_size) {
        count = chunk_size;
    }

    while (count > 0 && !in_aborting) {
        unsigned int channel;
        void *bufs[channels];
        size_t offset = result;
        for (channel = 0; channel < channels; channel++)
            bufs[channel] = data[channel] + offset * bits_per_sample / 8;
        if (test_position)
            do_test_position();
        check_stdin();
        r = readn_func(handle, bufs, count);
        if (test_position)
            do_test_position();
        if (r == -EAGAIN || (r >= 0 && (size_t)r < count)) {
            if (!test_nowait)
                snd_pcm_wait(handle, 100);
        } else if (r == -EPIPE) {
            xrun();
        } else if (r == -ESTRPIPE) {
            suspend();
        } else if (r < 0) {
            error(_("readv error: %s"), snd_strerror(r));
            prg_exit(EXIT_FAILURE);
        }
        if (r > 0) {
            if (vumeter) {
                for (channel = 0; channel < channels; channel++)
                    compute_max_peak(data[channel], r);
            }
            result += r;
            count -= r;
        }
    }
    return rcount;
}

/* that was a big one, perhaps somebody split it :-) */

/* setting the globals for playing raw data */
static void init_raw_data(void)
{
    hwparams = rhwparams;
}

/* calculate the data count to read from/to dsp */
static off64_t calc_count(void)
{
    off64_t count;

    if (timelimit == 0)
        if (sampleslimit == 0)
            count = pbrec_count;
        else
            count = snd_pcm_format_size(hwparams.format, sampleslimit * hwparams.channels);
    else {
        count = snd_pcm_format_size(hwparams.format, hwparams.rate * hwparams.channels);
        count *= (off64_t)timelimit;
    }
    return count < pbrec_count ? count : pbrec_count;
}
#endif
void _dump(unsigned char *buffer, int len)
{
    int i;

    printf("-------------------------------------\n");
    printf("len = %d\n", len);
    for (i = 0; i < len; i++) {
        if (i !=0 && (i % 16 == 0))
            printf("\n");
        printf(" %02x", buffer[i]);
    }
    printf("-------------------------------------\n");
}

int uac_get(unsigned char *buffer, int length)
{
    int len;

    if (length < FRAME_BUFFER_LENGTH)
        return -1;

    len = pcm_read(buffer, length);

    if (len != length) {
        in_aborting = 1;
        printf("pcm_read(audiobuf, f) != f\n");
        return -1;
    }

    _dump(buffer, len);
    return 0;
}



int uac_release(void)
{
    snd_pcm_close(handle);
    handle = NULL;
    free(audiobuf);
    snd_output_close(log);
    snd_config_update_free_global();

    return 0;
}

static ssize_t xwrite(int fd, const void *buf, size_t count)
{
    ssize_t written;
    size_t offset = 0;

    unsigned char *buffer = (unsigned char *)buf;

    while (offset < count) {
        written = write(fd, buffer + offset, count - offset);
        if (written <= 0)
            return written;

        offset += written;
    };

    return offset;
}


int begin_wave(int fd)
{
    WaveHeader h;
    WaveFmtBody f;
    WaveChunkHeader cf, cd;
    int bits;
    u_int tmp;
    u_short tmp2;
    size_t cnt = 2147483648;

    set_params();


    bits = 16;

    h.magic = WAV_RIFF;
    tmp = cnt + sizeof(WaveHeader) + sizeof(WaveChunkHeader) + sizeof(WaveFmtBody) + sizeof(WaveChunkHeader) - 8;
    h.length = LE_INT(tmp);
    h.type = WAV_WAVE;

    cf.type = WAV_FMT;
    cf.length = LE_INT(16);

    f.format = LE_SHORT(WAV_FMT_PCM);
    f.channels = LE_SHORT(2);
    f.sample_fq = LE_INT(16000);

    tmp2 = 2 * snd_pcm_format_physical_width(SND_PCM_FORMAT_FLOAT_LE) / 8;
    f.byte_p_spl = LE_SHORT(tmp2);
    tmp = (u_int) tmp2 * 16000;

    f.byte_p_sec = LE_INT(tmp);
    f.bit_p_spl = LE_SHORT(bits);

    cd.type = WAV_DATA;
    cd.length = LE_INT(cnt);

    if (xwrite(fd, &h, sizeof(WaveHeader)) != sizeof(WaveHeader) ||
        xwrite(fd, &cf, sizeof(WaveChunkHeader)) != sizeof(WaveChunkHeader) ||
        xwrite(fd, &f, sizeof(WaveFmtBody)) != sizeof(WaveFmtBody) ||
        xwrite(fd, &cd, sizeof(WaveChunkHeader)) != sizeof(WaveChunkHeader)) {
        error(_("write error"));
        return -1;
    }

    return 0;
}


int end_wave(int fd, size_t length)
{				/* only close output */
    WaveChunkHeader cd;
    off64_t length_seek;
    off64_t filelen;
    u_int rifflen;

    length_seek = sizeof(WaveHeader) +
              sizeof(WaveChunkHeader) +
              sizeof(WaveFmtBody);
    cd.type = WAV_DATA;
    cd.length = length > 0x7fffffff ? LE_INT(0x7fffffff) : LE_INT(length);
    filelen = length + 2*sizeof(WaveChunkHeader) + sizeof(WaveFmtBody) + 4;
    rifflen = filelen > 0x7fffffff ? LE_INT(0x7fffffff) : LE_INT(filelen);
    if (lseek64(fd, 4, SEEK_SET) == 4)
        xwrite(fd, &rifflen, 4);
    if (lseek64(fd, length_seek, SEEK_SET) == length_seek)
        xwrite(fd, &cd, sizeof(WaveChunkHeader));
    if (fd != 1)
        close(fd);

    return 0;
}

