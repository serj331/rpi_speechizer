#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <pthread.h>

#include <alsa/asoundlib.h>

#include <math.h>

#include "BCM2835gpio.h"

//#define ALC_DEBUG

typedef enum {
    SS_BEGIN_PLAY = 0,
    SS_PLAY,
    SS_PLAY_END,
    SS_BEGIN_RECORD,
    SS_RECORD,
    SS_RECORD_END
} SoundState;

volatile SoundState SS = SS_BEGIN_PLAY;//------------------------------

#define G_LED_PIN (23)
#define R_LED_PIN (24)
#define BTN_PIN (26)

#define PCM_PLAY_PERIOD_COUNT (6)
#define PCM_REC_PERIOD_COUNT (10)
#define PCM_PLAY_PERIOD_SIZE (5120)

#define HANNWND_N (PCM_PLAY_PERIOD_SIZE * 2)
static double HannWnd[HANNWND_N];

#define RS_SUM_COUNT 3

#define SF_BL_BASE_COUNT 7
#define SF_BL_RAND_MAX 5
#define SF_BL_RAND_SCALE 4

static const int SPREC_MIN_SIZE = (PCM_PLAY_PERIOD_SIZE * 2) * ((SF_BL_RAND_MAX * SF_BL_RAND_SCALE) + SF_BL_BASE_COUNT) * 2;
static const int SPREC_MAX_SIZE = 100 * 1024 * 1024;

typedef struct {
	uint32_t firtsblock_pos;
	uint32_t curblock_idx;
	uint32_t frame_len; //in blocks
	uint32_t lastblock_pos;    
} SpeechFrameInfo;

typedef struct {
    SpeechFrameInfo rndframe[RS_SUM_COUNT];
    int16_t out[PCM_PLAY_PERIOD_SIZE];
} RndSpeechBlock;

void RSB_reset(RndSpeechBlock* rsb, int sprec_len) {
    for(int i = 0; i < RS_SUM_COUNT; i++) {
        rsb->rndframe[i].frame_len = ((rand() % SF_BL_RAND_MAX) * SF_BL_RAND_SCALE) + SF_BL_BASE_COUNT;
        int k = sprec_len / (PCM_PLAY_PERIOD_SIZE * rsb->rndframe[i].frame_len);
        rsb->rndframe[i].firtsblock_pos = (rand() % k) * (PCM_PLAY_PERIOD_SIZE * rsb->rndframe[i].frame_len);
        rsb->rndframe[i].curblock_idx = 0;
        rsb->rndframe[i].lastblock_pos = 0; 
    }
}

void RSB_process(RndSpeechBlock* rsb, int16_t* sprec, int sprec_len) {
    int32_t sumbuf[PCM_PLAY_PERIOD_SIZE];
    
    for(int i = 0; i < PCM_PLAY_PERIOD_SIZE; i++) {
        sumbuf[i] = 0;
    }

    for(int i = 0; i < RS_SUM_COUNT; i++) {
        if(rsb->rndframe[i].curblock_idx == 0) {
            int16_t* firstblock = &sprec[rsb->rndframe[i].firtsblock_pos];
            int16_t* lastblock = &sprec[rsb->rndframe[i].lastblock_pos];
            for (int j = 0; j < PCM_PLAY_PERIOD_SIZE; j++) {
                sumbuf[j] += (int16_t)((double)lastblock[j] * HannWnd[j + (HANNWND_N / 2)]);
            }
            for (int j = 0; j < PCM_PLAY_PERIOD_SIZE; j++) {
                sumbuf[j] += (int16_t)((double)firstblock[j] * HannWnd[j]);
            }
        } else {
            int16_t* curblock = &sprec[rsb->rndframe[i].firtsblock_pos + (rsb->rndframe[i].curblock_idx * PCM_PLAY_PERIOD_SIZE)];
            for (int j = 0; j < PCM_PLAY_PERIOD_SIZE; j++) {
                sumbuf[j] += curblock[j];
            }
        }
        if (++(rsb->rndframe[i].curblock_idx) >= (rsb->rndframe[i].frame_len - 1)) { //chouse next frame
            rsb->rndframe[i].lastblock_pos = rsb->rndframe[i].firtsblock_pos + (rsb->rndframe[i].curblock_idx * PCM_PLAY_PERIOD_SIZE);
            rsb->rndframe[i].frame_len = ((rand() % SF_BL_RAND_MAX) * SF_BL_RAND_SCALE) + SF_BL_BASE_COUNT;
            int k = sprec_len / (PCM_PLAY_PERIOD_SIZE * rsb->rndframe[i].frame_len);
            rsb->rndframe[i].firtsblock_pos = (rand() % k) * (PCM_PLAY_PERIOD_SIZE * rsb->rndframe[i].frame_len);
            rsb->rndframe[i].curblock_idx = 0;
        }
    }
    
    for(int i = 0; i < PCM_PLAY_PERIOD_SIZE; i++) {
//        int32_t normsa = (int32_t)((double)sumbuf[i] / ((double)RS_SUM_COUNT * 0.707));
//        if(normsa > 32767) {
//            normsa = 32767;
//        }
//        if(normsa < -32768) {
//            normsa = -32768;
//        }
//        rsb->out[i] = (int16_t)normsa;
        rsb->out[i] = sumbuf[i] / RS_SUM_COUNT;
    }
}

void compute_hannwnd() {
    for(int n = 0; n < HANNWND_N; n++) {
        HannWnd[n] = 0.5 * (1.0 - cos((2.0 * M_PI * n) / (HANNWND_N - 1)));
    }
}

volatile double alc_cur_gain = 1.0, alc_dst_gain = 1.0, alc_gain_step = 0.0;

#ifdef ALC_DEBUG
//volatile pthread_mutex_t alci_lock;
//volatile pthread_cond_t alci_wait_v;

void* alc_info_thread() {
    while (1) {
        pthread_mutex_lock((pthread_mutex_t*)&alci_lock);
        pthread_cond_wait((pthread_cond_t*)&alci_wait_v, (pthread_mutex_t*)&alci_lock); 
        printf("cur=%lf, dst=%lf, step=%lf\n", alc_cur_gain, alc_dst_gain, alc_gain_step);
        pthread_mutex_unlock((pthread_mutex_t*)&alci_lock);
    }
}
#endif

void* btn_handler_thread() {
	static int btn_psh_tmr = 0;
	while (1) {
		usleep(50000);
		if (gpio_readpin(BTN_PIN) == 0) {
			btn_psh_tmr++;
            if (btn_psh_tmr >= 40) {
				btn_psh_tmr = 40;
                gpio_writepin(G_LED_PIN, 0);
                gpio_writepin(R_LED_PIN, 1);
			}
		} else {
			if(btn_psh_tmr >= 2 && btn_psh_tmr < 40) {
                if(SS == SS_RECORD) {
                    SS = SS_RECORD_END;
                }
			} else if(btn_psh_tmr >= 40) {
                if(SS == SS_PLAY) {
                    SS = SS_PLAY_END;
                }
            }
			btn_psh_tmr = 0;
		}
	}
}

snd_pcm_t* open_play_dev() {
    snd_pcm_t* pcmdevh;
    
    int err = snd_pcm_open(&pcmdevh, "hw:1,0", SND_PCM_STREAM_PLAYBACK, 0);
    if(err != 0) {
        return NULL;                                  
    }

    snd_pcm_hw_params_t* pcmhwparams = NULL;
    snd_pcm_sw_params_t* pcmswparams = NULL;

	snd_pcm_hw_params_alloca(&pcmhwparams);
    snd_pcm_sw_params_alloca(&pcmswparams);
    
	err = snd_pcm_hw_params_any(pcmdevh, pcmhwparams);
	if(err < 0) {
		return NULL; 
	}
    err = snd_pcm_hw_params_set_access(pcmdevh, pcmhwparams, SND_PCM_ACCESS_RW_INTERLEAVED);
	if(err < 0) {
		return NULL; 
	}
    err = snd_pcm_hw_params_set_format(pcmdevh, pcmhwparams, SND_PCM_FORMAT_S16_LE);
    if(err < 0) {
		return NULL; 
	}
    err = snd_pcm_hw_params_set_channels(pcmdevh, pcmhwparams, 2);
    if(err < 0) {
		return NULL; 
	}
    err = snd_pcm_hw_params_set_rate(pcmdevh, pcmhwparams, 48000, 0);
    if(err < 0) {
		return NULL; 
	} 
    err = snd_pcm_hw_params_set_period_size(pcmdevh, pcmhwparams, PCM_PLAY_PERIOD_SIZE, 0);
    if(err < 0) {
		return NULL; 
	}
    err = snd_pcm_hw_params_set_periods(pcmdevh, pcmhwparams, PCM_PLAY_PERIOD_COUNT, 0);
    if(err < 0) {
		return NULL; 
	}
	err = snd_pcm_hw_params(pcmdevh, pcmhwparams);
	if(err < 0) {
		return NULL; 
	}  
    
    return pcmdevh;
}

snd_pcm_t* open_rec_dev() {
    snd_pcm_t* pcmdevh;
    
    int err = snd_pcm_open(&pcmdevh, "hw:0,0", SND_PCM_STREAM_CAPTURE, 0);
    if(err != 0) {
        return NULL;                                  
    }

    snd_pcm_hw_params_t* pcmhwparams = NULL;
    snd_pcm_sw_params_t* pcmswparams = NULL;

	snd_pcm_hw_params_alloca(&pcmhwparams);
    snd_pcm_sw_params_alloca(&pcmswparams);
    
	err = snd_pcm_hw_params_any(pcmdevh, pcmhwparams);
	if(err < 0) {
		return NULL; 
	}
    err = snd_pcm_hw_params_set_access(pcmdevh, pcmhwparams, SND_PCM_ACCESS_RW_INTERLEAVED);
	if(err < 0) {
		return NULL; 
	}
    err = snd_pcm_hw_params_set_format(pcmdevh, pcmhwparams, SND_PCM_FORMAT_S32_LE);
    if(err < 0) {
		return NULL; 
	}
    err = snd_pcm_hw_params_set_channels(pcmdevh, pcmhwparams, 2);
    if(err < 0) {
		return NULL; 
	}
    err = snd_pcm_hw_params_set_rate(pcmdevh, pcmhwparams, 48000, 0);
    if(err < 0) {
		return NULL; 
	} 
    err = snd_pcm_hw_params_set_period_size(pcmdevh, pcmhwparams, PCM_PLAY_PERIOD_SIZE, 0);
    if(err < 0) {
		return NULL; 
	}
    err = snd_pcm_hw_params_set_periods(pcmdevh, pcmhwparams, PCM_REC_PERIOD_COUNT, 0);
    if(err < 0) {
		return NULL; 
	}
	err = snd_pcm_hw_params(pcmdevh, pcmhwparams);
	if(err < 0) {
		return NULL; 
	}  
    
    return pcmdevh;
}

void close_rec_dev(snd_pcm_t* pcmrec) {
    snd_pcm_drop(pcmrec);
    snd_pcm_close(pcmrec);
}

void setup_rand() {
    int hwrng_fd = open("/dev/hwrng", O_RDONLY);
    if(hwrng_fd > 0) {
        unsigned int seed;
        int n = read(hwrng_fd, &seed, sizeof(seed));
        if(n == sizeof(seed)) {
            srand(seed);
            printf("hwrng read. %X\n", seed);
            return;
        }
        close(hwrng_fd);
    }
    perror("get seed from hwrng error");
}

int mount_sys() {
    int err;
    
//    err = mount("none", "/dev", "devtmpfs", 0, "");
//    if(err != 0) {
//        perror("mount dev error");
//        return err;
//    }
//    puts("dev mounted.");
    
    err = mount("none", "/sys", "sysfs", MS_NOEXEC | MS_NODEV | MS_NOSUID | MS_RELATIME, "");
    if(err != 0) {
        perror("mount sys error");
        return err;
    }
    puts("sysfs mounted.");
    
    err = mount("none", "/proc", "proc", MS_NOEXEC | MS_NODEV | MS_NOSUID | MS_RELATIME, "");
    if(err != 0) {
        perror("mount proc error");
        return err;
    }
    puts("proc mounted.");
    
    return 0;
}

int main(int argc, char **argv) {
    mount_sys();
    
    if(gpio_access() == -1) {
        fputs("access to gpio failed.", stderr);
        exit(EXIT_FAILURE);
    }
    
    gpio_confpin(G_LED_PIN, FSEL_OUT, PUD_OFF);
    gpio_confpin(R_LED_PIN, FSEL_OUT, PUD_OFF);
    gpio_confpin(BTN_PIN, FSEL_IN, PUD_UP);
        
    compute_hannwnd();
    
    setup_rand();
    
    pthread_t butt_th;
	if(pthread_create(&butt_th, NULL, btn_handler_thread, NULL) != 0) {
        fputs("failed to start button thread.", stderr);
        exit(EXIT_FAILURE);
    }
    
#ifdef ALC_DEBUG
    pthread_mutex_init((pthread_mutex_t*)&alci_lock, NULL);
    pthread_t alci_th;
	if(pthread_create(&alci_th, NULL, alc_info_thread, NULL) != 0) {
        fputs("failed to start alc info thread.", stderr);
        exit(EXIT_FAILURE);
    }
#endif

	puts("speechizer(v1.11) started.");

    RndSpeechBlock rsb1;
    RndSpeechBlock rsb2;

    snd_pcm_t* pcmplay = open_play_dev();
    if(pcmplay == NULL) {
        fputs("failed open play device.", stderr);
        exit(EXIT_FAILURE);
    }
    
    snd_pcm_t* pcmrec = NULL;
    
    int sprec_fd = 0, sprec_size = 0;
    int16_t* sprec_mm = NULL;
//    int sprec_playpos = 0;
    int skip_recblocks = 0;
    int r_led_st = 0;

    for(;;) {
        switch(SS) {
            case SS_BEGIN_PLAY: {
                puts("SS_BEGIN_PLAY");
                sprec_fd = open("./sprec.raw", O_RDONLY);
                if(sprec_fd > 0) {
                    struct stat sprec_f_st;
                    if(fstat(sprec_fd, &sprec_f_st) != -1) {
                        sprec_size = (int)sprec_f_st.st_size;
                        if(sprec_size >= SPREC_MIN_SIZE) {
                            printf("sprec.raw file opened. size=%d\n", sprec_size);
                            sprec_mm = mmap(NULL, sprec_size, PROT_READ, MAP_PRIVATE, sprec_fd, 0);
                            if(sprec_mm != MAP_FAILED) {
//                                sprec_playpos = 0;
                                RSB_reset(&rsb1, (sprec_size / 2));
                                RSB_reset(&rsb2, (sprec_size / 2));
                                snd_pcm_prepare(pcmplay);
                                gpio_writepin(G_LED_PIN, 1);
                                gpio_writepin(R_LED_PIN, 0);
                                SS = SS_PLAY;
                                continue;
                            }
                        }
                    }               
                }
                SS = SS_PLAY_END;
            }
            break;
            case SS_PLAY: {
                int16_t playbuf[PCM_PLAY_PERIOD_SIZE * 2]; //2ch

                RSB_process(&rsb1, sprec_mm, (sprec_size / 2));
                RSB_process(&rsb2, sprec_mm, (sprec_size / 2));

                for(int i = 0; i < PCM_PLAY_PERIOD_SIZE; i++) {
//                    playbuf[i * 2 + 0] = sprec_mm[sprec_playpos + i];
//                    playbuf[i * 2 + 1] = sprec_mm[sprec_playpos + i];
                    playbuf[i * 2 + 0] = rsb1.out[i];
                    playbuf[i * 2 + 1] = rsb2.out[i];
                }
//                sprec_playpos += PCM_PLAY_PERIOD_SIZE;
//                if(sprec_playpos >= (sprec_size / 2)) {
//                    sprec_playpos = 0;
//                }

                snd_pcm_sframes_t pcmres = snd_pcm_writei(pcmplay, playbuf, PCM_PLAY_PERIOD_SIZE);
                if (pcmres == -EPIPE) {
                    fputs("SS_PLAY: snd_pcm_writei underrun occurred", stderr);
                    snd_pcm_prepare(pcmplay);
                } else if (pcmres < 0) {
                    fprintf(stderr, "SS_PLAY: snd_pcm_writei error %s\n", snd_strerror(pcmres));
                    snd_pcm_prepare(pcmplay); //?
                } else if (pcmres != PCM_PLAY_PERIOD_SIZE) {
                    fprintf(stderr, "SS_PLAY: snd_pcm_writei short write %d\n", (int)pcmres);
                }
            }
            break;
            case SS_PLAY_END: {
                puts("SS_PLAY_END");
                snd_pcm_drop(pcmplay);
                if(sprec_mm != NULL) {
                    munmap(sprec_mm, sprec_size);
                    sprec_mm = NULL;
                }
                if(sprec_fd > 0) {
                    close(sprec_fd);
                }
                sprec_size = 0;
                sprec_fd = 0;
                gpio_writepin(G_LED_PIN, 0);
                SS = SS_BEGIN_RECORD;
            }
            break;
            case SS_BEGIN_RECORD: {
                puts("SS_BEGIN_RECORD");
                pcmrec = open_rec_dev();
                if(pcmrec != NULL) {
                    sprec_fd = open("./sprec.raw", O_WRONLY | O_CREAT | O_TRUNC);
                    if(sprec_fd > 0) {
                        sprec_size = 0;
                        skip_recblocks = 4;
                        gpio_writepin(G_LED_PIN, 0);
                        r_led_st = 1;
                        gpio_writepin(R_LED_PIN, r_led_st);
                        SS = SS_RECORD;
                        continue;
                    } else {
                        perror("error create sprec.raw");
                    }
                } else {
                    fputs("failed open rec device.", stderr);
                }
                exit(EXIT_FAILURE);
            }
            break;
            case SS_RECORD: {
                int32_t recbuf[PCM_PLAY_PERIOD_SIZE * 2]; //2ch
                snd_pcm_sframes_t pcmres = snd_pcm_readi(pcmrec, recbuf, PCM_PLAY_PERIOD_SIZE);
                if (pcmres == -EPIPE) {
                    fputs("SS_RECORD: snd_pcm_readi overrun occurred ", stderr);
                    snd_pcm_prepare(pcmrec);
                } else if (pcmres < 0) {
                    fprintf(stderr, "SS_RECORD: snd_pcm_readi error %s\n", snd_strerror(pcmres));
                    SS = SS_RECORD_END;
                } else {
                    if(skip_recblocks > 0) {
                        skip_recblocks--;
                        break;
                    }
                    gpio_writepin(R_LED_PIN, (r_led_st = !r_led_st));
                    int16_t se16buf[PCM_PLAY_PERIOD_SIZE];

                    //ALC code
#ifdef ALC_DEBUG
                    pthread_mutex_lock((pthread_mutex_t*)&alci_lock);
#endif
                    //calc rms
                    double msum = 0;
                    for(int i = 0; i < PCM_PLAY_PERIOD_SIZE; i++) {
                        double sa = recbuf[i * 2 + 0] >> 8; //24 bit
                        msum += (sa * sa);
                    }
                    int32_t vrms = (int32_t)sqrt(msum / (double)PCM_PLAY_PERIOD_SIZE);
                    
                    //calc target gain level and gain level step
                    if(vrms < 2000000) { 
                        alc_dst_gain = 2000000 / vrms;
                        if(alc_dst_gain > 256.0) {
                            alc_dst_gain = 256.0;
                        } else if(alc_dst_gain < 1.0) {
                            alc_dst_gain = 1.0;
                        }
                        if(alc_dst_gain > alc_cur_gain) {
                            alc_gain_step = (alc_dst_gain - alc_cur_gain) / 73920.0;
                        } else if(alc_dst_gain < alc_cur_gain) {
                            alc_gain_step = (alc_dst_gain - alc_cur_gain) / 1152.0;
                        }
                    } else if(vrms > 2000000) {
                        alc_cur_gain = 1.0;
                        alc_dst_gain = 1.0;
                        alc_gain_step = 0.0;
                    }
                    
                    for(int i = 0; i < PCM_PLAY_PERIOD_SIZE; i++) {
                        int32_t sa = recbuf[i * 2 + 0] >> 8;//24bit
                        if((alc_gain_step > 0 && ((alc_cur_gain + alc_gain_step) <= alc_dst_gain)) || 
                           (alc_gain_step < 0 && ((alc_cur_gain + alc_gain_step) >= alc_dst_gain))) {
                            alc_cur_gain += alc_gain_step;
                        }
                        sa *= (int)alc_cur_gain;
                        sa >>= 8;

                        //saturate value
                        if(sa > 32767) {
                            sa = 32767;
                        }
                        if(sa < -32768) {
                            sa = -32768;
                        }
                        se16buf[i] = (int16_t)(sa);
                    }
#ifdef ALC_DEBUG
                    pthread_cond_signal((pthread_cond_t*)&alci_wait_v);
                    pthread_mutex_unlock((pthread_mutex_t*)&alci_lock);
#endif
                    //save to file
                    size_t n = write(sprec_fd, se16buf, PCM_PLAY_PERIOD_SIZE * 2);
                    if(n == PCM_PLAY_PERIOD_SIZE * 2) {
                        sprec_size += (PCM_PLAY_PERIOD_SIZE * 2);
                        if(sprec_size >= SPREC_MAX_SIZE) {
                            SS = SS_RECORD_END;
                            break;
                        }
                    } else {
                        perror("SS_PLAY: write() error ");
                        SS = SS_RECORD_END;
                        break;
                    }
                }
            }
            break;
            case SS_RECORD_END: {
                puts("SS_RECORD_END");
                close_rec_dev(pcmrec);
                pcmrec = NULL;
                //try truncate 200ms cut
                if(sprec_size > (PCM_PLAY_PERIOD_SIZE * 2 * 4)) {
                    if(ftruncate(sprec_fd, sprec_size - (PCM_PLAY_PERIOD_SIZE * 2 * 4)) != 0) {
                        perror("SS_RECORD_END: failed truncate file");
                    }
                }
                int r = fsync(sprec_fd);
                printf("fsync()=%d\n", r);
                close(sprec_fd);
                sprec_size = 0;
                sprec_fd = 0;
                r_led_st = 0;
                gpio_writepin(R_LED_PIN, r_led_st);
                SS = SS_BEGIN_PLAY;
            }
            break;
        }        
    }
	return 0;
}
