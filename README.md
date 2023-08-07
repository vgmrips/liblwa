# Light-Weight Audio library (liblwa)

liblwa is a light-weight library that provides a C API for easy audio output on multiple platforms.

Currently, the following audio APIs are supported:

- Windows:
    - WinMM (Windows Multimedia API)
    - DirectSound
    - XAudio2
    - WASAPI (Windows Audio Session API)
- Unix and Linux:
    - OSS (Open Sound System)
    - SADA (Solaris Audio Device Architecture)
    - ALSA (Advanced Linux Sound Architecture)
    - PulseAudio
- Mac OS X:
    - Core Audio
- other:
    - WAVE file writer
    - [libao](https://xiph.org/ao/)

The library provides functions to output sound using either a callback function or an active "push" function.  
In addition to that, it also features a few simple wrappers around OS functions for threads, mutexes and signals.  
The audio API wrappers are kept simple (no resampling or mixing), thus it introduces barely any latency.

## Simple example of using the library

```c
#include <liblwa/output/lwao.h>

#define SAMPLE_SIZE 2 // opts->numChannels * opts->numBitsPerSmpl / 8
void main(void)
{
    const uint8_t lwad_driver_id = 0; // change to select between e.g. WinMM/DirectSound or ALSA/PulseAudio
    uint8_t ret;
    void* audDrv;
    LWAO_OPTS* opts;

    lwaoInit(); // initialize liblwa
    ret = lwaodInit(lwad_driver_id, &audDrv); // create audio driver instance (but don't connect to a device yet)

    // set output format
    opts = lwaodGetOptions(audDrv);
    opts->sampleRate = 44100;
    opts->numChannels = 1;
    opts->numBitsPerSmpl = 16;

    lwaodSetCallback(audDrv, FillBuffer, NULL); // set audio callback
    ret = lwaodStart(audDrv, 0);  // connect to audio device and start stream

    // --- run main application logic here ---
    // The function "FillBuffer" will be called when new audio data can be sent.
    while(1) { /* ... */ }

    ret = lwaodStop(audDrv);  // stop the audio stream
    lwaodDeinit(&audDrv); // destroy audio driver instance (will NULL the pointer)
    lwaoDeinit(); // deinitialize liblwa
}

uint32_t FillBuffer(void* drv, void* user, uint32_t bufSize, void* data)
{
    uint32_t smplCount = bufSize / SAMPLE_SIZE;
    int16_t* smplPtr16 = (int16_t*)data;
    uint32_t curSmpl;
    for (curSmpl = 0; curSmpl < smplCount; curSmpl ++)
    {
        // output a square wave
        smplPtr16[curSmpl] = ((curSmpl / (smplCount / 2)) < 1) ? +0x1000 : -0x1000;
    }
    return curSmpl * SAMPLE_SIZE;
}
```

## Various notes

### Prefixes

- `lwa` - general library prefix
- `lwao` - audio **o**utput API
  - `lwaod` - audio output driver API
- `lwau` - **u**tils API

### Migration from libvgm

The library was originally a part of [libvgm](https://github.com/ValleyBell/libvgm) and was eventually split off.
Most of the split is related to naming, so migrating from libvgm to liblwa can be done mostly using search-and-replace.

- prefix replacement via sed, audio output part:

	```bash
	sed -i \
		-e 's,\<Audio_,lwao,g' \
		-e 's,\<AudioDrv_,lwaod,g' \
		-e 's,\<audDrv_,lwaoDrv_,g' \
		-e 's,AUDDRV_INFO,LWAO_DINFO,g' \
		-e 's,AUDIO_DRV,LWAO_DRIVER,g' \
		-e 's,AUDIO_DEV_LIST,LWAO_DEV_LIST,g' \
		-e 's,AUDIO_,LWAO_,g' \
		-e 's,AUDDRV_,LWAO_DRV_,g' \
		-e 's,AUDFUNC,LWAOFUNC,g' \
		-e 's,ADRV,LWAO_D,g' \
		-e 's,AERR,LWAO_ERR,g' \
		<files>
	sed -i \
		-e 's,\<\(ALSA\|CoreAudio\|DSound\|LibAO\|OSS\|Pulse\|SADA\|WASAPI\|WavWrt\|WinMM\|XAudio2\)_,lwaod\1_,g' \
		<files>
	```

- prefix replacement via sed, utility part:

	```bash
	sed -i \
		-e 's,OS_MUTEX,LWAU_MUTEX,g' \
		-e 's,OSMutex,lwauMutex,g' \
		-e 's,OS_SIGNAL,LWAU_SIGNAL,g' \
		-e 's,OSSignal,lwauSignal,g' \
		-e 's,OS_THREAD,LWAU_THREAD,g' \
		-e 's,OS_THR_FUNC,LWAU_THR_FUNC,g' \
		-e 's,OSThread,lwauThread,g' \
		<files>
	```

- moved header files:

	```bash
	sed -i \
		-e 's,stdtype.h,liblwa/lwa_types.h,g' \
		<files>
	```

  **Note:** `common_def.h` was removed.

- moved header files, utilities part:

	```bash
	sed -i \
		-e 's,"utils/,"liblwa/utils/,g' \
		-e 's,<utils/,<liblwa/utils/,g' \
		<files>
	```
- moved header files, audio output part:

	```bash
	sed -i \
		-e 's,"audio/Audio,"liblwa/output/Audio,g' \
		-e 's,<audio/Audio,<liblwa/output/Audio,g' \
		-e 's,AudioStructs.h,lwao_structs.h,g' \
		-e 's,AudioStream.h,lwao.h,g' \
		-e 's,AudioStream_SpcDrvFuns.h,lwao_drvfuncs.h,g' \
		<files>
	```
