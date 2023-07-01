# Light-Weight Audio library (liblwa)

TODO ...

## Prefixes

- `lwa` - general library prefix
- `lwao` - audio **o**utput API
  - `lwaod` - audio output driver API
- `lwau` - **u**tils API


## Migration from libvgm

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
		-e 's,stdtype.h,liblwa/stdtype.h,g' \
		-e 's,common_def.h,liblwa/common_def.h,g' \
		<files>
	```

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
