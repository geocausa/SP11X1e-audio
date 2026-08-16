/*
 * SP11 genuine Windows Spatial Audio object oracle generator.
 *
 * Self-contained on purpose: this file carries only the minimal public Win32
 * COM ABI declarations it needs, so it can be cross-built for Windows ARM64
 * from Linux without a Windows SDK installation.
 *
 * Safety: the default render endpoint is forced to 1% master volume before
 * streaming starts.  Failure to enforce that cap aborts before any audio is
 * produced.  The previous endpoint volume is restored on normal exit.
 *
 * Test stream: one mono float32/48-kHz spatial object, dynamic when available
 * (position +1m on X/right), otherwise static FrontCenter, deterministic
 * 997-Hz sine, 0.05 amplitude, 30 seconds.
 */

#include <stdint.h>
#include <stddef.h>

#define DLLIMPORT __declspec(dllimport)
#define WINAPI __stdcall
#define STDMETHODCALLTYPE __stdcall

typedef int32_t HRESULT;
typedef uint32_t DWORD;
typedef uint32_t UINT32;
typedef uint32_t ULONG;
typedef uint16_t WORD;
typedef uint16_t VARTYPE;
typedef int32_t BOOL;
typedef void *HANDLE;
typedef const void *LPCVOID;
typedef void *LPVOID;
typedef const uint16_t *LPCWSTR;
typedef uint8_t BYTE;

#define S_OK ((HRESULT)0)
#define SUCCEEDED(hr) ((HRESULT)(hr) >= 0)
#define FAILED(hr) ((HRESULT)(hr) < 0)
#define CLSCTX_INPROC_SERVER 0x1u
#define CLSCTX_ALL 0x17u
#define COINIT_MULTITHREADED 0x0u
#define WAIT_OBJECT_0 0u
#define WAIT_TIMEOUT 258u
#define STD_ERROR_HANDLE ((DWORD)-12)
#define VT_BLOB 65u

#define AUDIO_OBJECT_DYNAMIC 0x1u
#define AUDIO_OBJECT_FRONT_CENTER 0x8u
#define AUDIO_CATEGORY_SOUND_EFFECTS 5u

#define TEST_RATE 48000u
#define TEST_SECONDS 30u
#define TEST_TOTAL_FRAMES ((uint64_t)TEST_RATE * TEST_SECONDS)
#define TEST_AMPLITUDE 0.05
#define TEST_SIN_W 0.13013684267905246
#define TEST_TWO_COS_W 1.982992084883374
#define SAFE_ENDPOINT_VOLUME 0.01f

typedef struct GUID {
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t Data4[8];
} GUID;
typedef const GUID *REFIID;

typedef struct BLOB {
    uint32_t cbSize;
    BYTE *pBlobData;
} BLOB;

typedef struct PROPVARIANT {
    VARTYPE vt;
    WORD wReserved1, wReserved2, wReserved3;
    union { BLOB blob; uint64_t align[2]; } u;
} PROPVARIANT;

#pragma pack(push, 1)
typedef struct WAVEFORMATEX {
    WORD wFormatTag;
    WORD nChannels;
    DWORD nSamplesPerSec;
    DWORD nAvgBytesPerSec;
    WORD nBlockAlign;
    WORD wBitsPerSample;
    WORD cbSize;
} WAVEFORMATEX;
#pragma pack(pop)

_Static_assert(sizeof(WAVEFORMATEX) == 18, "WAVEFORMATEX ABI");
_Static_assert(sizeof(PROPVARIANT) == 24, "PROPVARIANT ABI");

typedef struct SpatialAudioObjectRenderStreamActivationParams {
    const WAVEFORMATEX *ObjectFormat;
    uint32_t StaticObjectTypeMask;
    UINT32 MinDynamicObjectCount;
    UINT32 MaxDynamicObjectCount;
    uint32_t Category;
    HANDLE EventHandle;
    void *NotifyObject;
} SpatialAudioObjectRenderStreamActivationParams;
_Static_assert(sizeof(SpatialAudioObjectRenderStreamActivationParams) == 40, "Spatial activation ABI");

/* Win32 imports. */
DLLIMPORT HRESULT WINAPI CoInitializeEx(LPVOID, DWORD);
DLLIMPORT void WINAPI CoUninitialize(void);
DLLIMPORT HRESULT WINAPI CoCreateInstance(REFIID, LPVOID, DWORD, REFIID, LPVOID *);
DLLIMPORT void WINAPI CoTaskMemFree(LPVOID);
DLLIMPORT HANDLE WINAPI CreateEventW(LPVOID, BOOL, BOOL, LPCWSTR);
DLLIMPORT BOOL WINAPI CloseHandle(HANDLE);
DLLIMPORT DWORD WINAPI WaitForSingleObject(HANDLE, DWORD);
DLLIMPORT HANDLE WINAPI GetStdHandle(DWORD);
DLLIMPORT BOOL WINAPI WriteFile(HANDLE, LPCVOID, DWORD, DWORD *, LPVOID);
DLLIMPORT void WINAPI ExitProcess(UINT32);

/* Minimal COM forward declarations. */
typedef struct IMMDeviceEnumerator IMMDeviceEnumerator;
typedef struct IMMDevice IMMDevice;
typedef struct IAudioFormatEnumerator IAudioFormatEnumerator;
typedef struct ISpatialAudioClient ISpatialAudioClient;
typedef struct ISpatialAudioObjectRenderStream ISpatialAudioObjectRenderStream;
typedef struct ISpatialAudioObject ISpatialAudioObject;
typedef struct IAudioEndpointVolume IAudioEndpointVolume;

typedef struct IMMDeviceEnumeratorVtbl {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(IMMDeviceEnumerator *, REFIID, void **);
    ULONG (STDMETHODCALLTYPE *AddRef)(IMMDeviceEnumerator *);
    ULONG (STDMETHODCALLTYPE *Release)(IMMDeviceEnumerator *);
    HRESULT (STDMETHODCALLTYPE *EnumAudioEndpoints)(IMMDeviceEnumerator *, int, DWORD, void **);
    HRESULT (STDMETHODCALLTYPE *GetDefaultAudioEndpoint)(IMMDeviceEnumerator *, int, int, IMMDevice **);
    HRESULT (STDMETHODCALLTYPE *GetDevice)(IMMDeviceEnumerator *, LPCWSTR, IMMDevice **);
    HRESULT (STDMETHODCALLTYPE *RegisterEndpointNotificationCallback)(IMMDeviceEnumerator *, void *);
    HRESULT (STDMETHODCALLTYPE *UnregisterEndpointNotificationCallback)(IMMDeviceEnumerator *, void *);
} IMMDeviceEnumeratorVtbl;
struct IMMDeviceEnumerator { const IMMDeviceEnumeratorVtbl *lpVtbl; };

typedef struct IMMDeviceVtbl {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(IMMDevice *, REFIID, void **);
    ULONG (STDMETHODCALLTYPE *AddRef)(IMMDevice *);
    ULONG (STDMETHODCALLTYPE *Release)(IMMDevice *);
    HRESULT (STDMETHODCALLTYPE *Activate)(IMMDevice *, REFIID, DWORD, PROPVARIANT *, void **);
    HRESULT (STDMETHODCALLTYPE *OpenPropertyStore)(IMMDevice *, DWORD, void **);
    HRESULT (STDMETHODCALLTYPE *GetId)(IMMDevice *, uint16_t **);
    HRESULT (STDMETHODCALLTYPE *GetState)(IMMDevice *, DWORD *);
} IMMDeviceVtbl;
struct IMMDevice { const IMMDeviceVtbl *lpVtbl; };

typedef struct IAudioFormatEnumeratorVtbl {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(IAudioFormatEnumerator *, REFIID, void **);
    ULONG (STDMETHODCALLTYPE *AddRef)(IAudioFormatEnumerator *);
    ULONG (STDMETHODCALLTYPE *Release)(IAudioFormatEnumerator *);
    HRESULT (STDMETHODCALLTYPE *GetCount)(IAudioFormatEnumerator *, UINT32 *);
    HRESULT (STDMETHODCALLTYPE *GetFormat)(IAudioFormatEnumerator *, UINT32, WAVEFORMATEX **);
} IAudioFormatEnumeratorVtbl;
struct IAudioFormatEnumerator { const IAudioFormatEnumeratorVtbl *lpVtbl; };

typedef struct ISpatialAudioClientVtbl {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(ISpatialAudioClient *, REFIID, void **);
    ULONG (STDMETHODCALLTYPE *AddRef)(ISpatialAudioClient *);
    ULONG (STDMETHODCALLTYPE *Release)(ISpatialAudioClient *);
    HRESULT (STDMETHODCALLTYPE *GetStaticObjectPosition)(ISpatialAudioClient *, uint32_t, float *, float *, float *);
    HRESULT (STDMETHODCALLTYPE *GetNativeStaticObjectTypeMask)(ISpatialAudioClient *, uint32_t *);
    HRESULT (STDMETHODCALLTYPE *GetMaxDynamicObjectCount)(ISpatialAudioClient *, UINT32 *);
    HRESULT (STDMETHODCALLTYPE *GetSupportedAudioObjectFormatEnumerator)(ISpatialAudioClient *, IAudioFormatEnumerator **);
    HRESULT (STDMETHODCALLTYPE *GetMaxFrameCount)(ISpatialAudioClient *, const WAVEFORMATEX *, UINT32 *);
    HRESULT (STDMETHODCALLTYPE *IsAudioObjectFormatSupported)(ISpatialAudioClient *, const WAVEFORMATEX *);
    HRESULT (STDMETHODCALLTYPE *IsSpatialAudioStreamAvailable)(ISpatialAudioClient *, REFIID, const PROPVARIANT *);
    HRESULT (STDMETHODCALLTYPE *ActivateSpatialAudioStream)(ISpatialAudioClient *, const PROPVARIANT *, REFIID, void **);
} ISpatialAudioClientVtbl;
struct ISpatialAudioClient { const ISpatialAudioClientVtbl *lpVtbl; };

typedef struct ISpatialAudioObjectRenderStreamVtbl {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(ISpatialAudioObjectRenderStream *, REFIID, void **);
    ULONG (STDMETHODCALLTYPE *AddRef)(ISpatialAudioObjectRenderStream *);
    ULONG (STDMETHODCALLTYPE *Release)(ISpatialAudioObjectRenderStream *);
    HRESULT (STDMETHODCALLTYPE *GetAvailableDynamicObjectCount)(ISpatialAudioObjectRenderStream *, UINT32 *);
    HRESULT (STDMETHODCALLTYPE *GetService)(ISpatialAudioObjectRenderStream *, REFIID, void **);
    HRESULT (STDMETHODCALLTYPE *Start)(ISpatialAudioObjectRenderStream *);
    HRESULT (STDMETHODCALLTYPE *Stop)(ISpatialAudioObjectRenderStream *);
    HRESULT (STDMETHODCALLTYPE *Reset)(ISpatialAudioObjectRenderStream *);
    HRESULT (STDMETHODCALLTYPE *BeginUpdatingAudioObjects)(ISpatialAudioObjectRenderStream *, UINT32 *, UINT32 *);
    HRESULT (STDMETHODCALLTYPE *EndUpdatingAudioObjects)(ISpatialAudioObjectRenderStream *);
    HRESULT (STDMETHODCALLTYPE *ActivateSpatialAudioObject)(ISpatialAudioObjectRenderStream *, uint32_t, ISpatialAudioObject **);
} ISpatialAudioObjectRenderStreamVtbl;
struct ISpatialAudioObjectRenderStream { const ISpatialAudioObjectRenderStreamVtbl *lpVtbl; };

typedef struct ISpatialAudioObjectVtbl {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(ISpatialAudioObject *, REFIID, void **);
    ULONG (STDMETHODCALLTYPE *AddRef)(ISpatialAudioObject *);
    ULONG (STDMETHODCALLTYPE *Release)(ISpatialAudioObject *);
    HRESULT (STDMETHODCALLTYPE *GetBuffer)(ISpatialAudioObject *, BYTE **, UINT32 *);
    HRESULT (STDMETHODCALLTYPE *SetEndOfStream)(ISpatialAudioObject *, UINT32);
    HRESULT (STDMETHODCALLTYPE *IsActive)(ISpatialAudioObject *, BOOL *);
    HRESULT (STDMETHODCALLTYPE *GetAudioObjectType)(ISpatialAudioObject *, uint32_t *);
    HRESULT (STDMETHODCALLTYPE *SetPosition)(ISpatialAudioObject *, float, float, float);
    HRESULT (STDMETHODCALLTYPE *SetVolume)(ISpatialAudioObject *, float);
} ISpatialAudioObjectVtbl;
struct ISpatialAudioObject { const ISpatialAudioObjectVtbl *lpVtbl; };

typedef struct IAudioEndpointVolumeVtbl {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(IAudioEndpointVolume *, REFIID, void **);
    ULONG (STDMETHODCALLTYPE *AddRef)(IAudioEndpointVolume *);
    ULONG (STDMETHODCALLTYPE *Release)(IAudioEndpointVolume *);
    void *RegisterControlChangeNotify;
    void *UnregisterControlChangeNotify;
    void *GetChannelCount;
    void *SetMasterVolumeLevel;
    HRESULT (STDMETHODCALLTYPE *SetMasterVolumeLevelScalar)(IAudioEndpointVolume *, float, const GUID *);
    void *GetMasterVolumeLevel;
    HRESULT (STDMETHODCALLTYPE *GetMasterVolumeLevelScalar)(IAudioEndpointVolume *, float *);
} IAudioEndpointVolumeVtbl;
struct IAudioEndpointVolume { const IAudioEndpointVolumeVtbl *lpVtbl; };

static const GUID CLSID_MMDeviceEnumerator = {0xbcde0395,0xe52f,0x467c,{0x8e,0x3d,0xc4,0x57,0x92,0x91,0x69,0x2e}};
static const GUID IID_IMMDeviceEnumerator = {0xa95664d2,0x9614,0x4f35,{0xa7,0x46,0xde,0x8d,0xb6,0x36,0x17,0xe6}};
static const GUID IID_ISpatialAudioClient = {0xbbf8e066,0xaaaa,0x49be,{0x9a,0x4d,0xfd,0x2a,0x85,0x8e,0xa2,0x7f}};
static const GUID IID_ISpatialAudioObjectRenderStream = {0xbab5f473,0xb423,0x477b,{0x85,0xf5,0xb5,0xa3,0x32,0xa0,0x41,0x53}};
static const GUID IID_IAudioEndpointVolume = {0x5cdf2c82,0x841e,0x4546,{0x97,0x22,0x0c,0xf7,0x40,0x78,0x22,0x9a}};

static HANDLE g_log;

void *memset(void *dst, int c, size_t n) {
    BYTE *p=(BYTE *)dst; while(n--) *p++=(BYTE)c; return dst;
}
void *memcpy(void *dst, const void *src, size_t n) {
    BYTE *d=(BYTE *)dst; const BYTE *s=(const BYTE *)src; while(n--) *d++=*s++; return dst;
}

static void write_raw(const char *s, DWORD n) {
    DWORD done=0; if(g_log && g_log!=(HANDLE)(intptr_t)-1) WriteFile(g_log,s,n,&done,0);
}
static void log_s(const char *s) { DWORD n=0; while(s[n]) n++; write_raw(s,n); }
static void log_nl(void) { write_raw("\r\n",2); }
static void log_hex32(uint32_t v) {
    static const char h[]="0123456789ABCDEF"; char b[10]; b[0]='0';b[1]='x';
    for(int i=0;i<8;i++) b[2+i]=h[(v>>(28-4*i))&15]; write_raw(b,10);
}
static void log_u32(uint32_t v) {
    char b[11]; int n=0; if(!v){write_raw("0",1);return;} while(v){b[n++]=(char)('0'+v%10);v/=10;}
    while(n) write_raw(&b[--n],1);
}
static void log_hr(const char *step, HRESULT hr) { log_s(step); log_s(" hr="); log_hex32((uint32_t)hr); log_nl(); }

static int exact_object_format(const WAVEFORMATEX *f) {
    return f && f->wFormatTag==3 && f->nChannels==1 && f->nSamplesPerSec==TEST_RATE &&
           f->nBlockAlign==4 && f->wBitsPerSample==32;
}

static UINT32 run_probe(void) {
    HRESULT hr=S_OK;
    IMMDeviceEnumerator *enumerator=0;
    IMMDevice *device=0;
    IAudioEndpointVolume *endpoint_volume=0;
    ISpatialAudioClient *client=0;
    IAudioFormatEnumerator *formats=0;
    WAVEFORMATEX *format=0;
    ISpatialAudioObjectRenderStream *stream=0;
    ISpatialAudioObject *object=0;
    HANDLE event=0;
    float old_volume=0.0f;
    int volume_capped=0;
    int com_started=0;
    uint32_t object_type=0;
    UINT32 exit_code=1;

    g_log=GetStdHandle(STD_ERROR_HANDLE);
    log_s("SP11_SPATIAL_OBJECT_ORACLE start\r\n");

    hr=CoInitializeEx(0,COINIT_MULTITHREADED);
    log_hr("CoInitializeEx",hr);
    if(FAILED(hr)) goto cleanup;
    com_started=1;

    hr=CoCreateInstance(&CLSID_MMDeviceEnumerator,0,CLSCTX_INPROC_SERVER,&IID_IMMDeviceEnumerator,(void **)&enumerator);
    log_hr("CoCreateInstance(MMDeviceEnumerator)",hr);
    if(FAILED(hr)) goto cleanup;

    hr=enumerator->lpVtbl->GetDefaultAudioEndpoint(enumerator,0,0,&device); /* eRender/eConsole */
    log_hr("GetDefaultAudioEndpoint",hr);
    if(FAILED(hr)) goto cleanup;

    /* Safety gate: no object stream is activated unless physical endpoint volume is <=1%. */
    hr=device->lpVtbl->Activate(device,&IID_IAudioEndpointVolume,CLSCTX_ALL,0,(void **)&endpoint_volume);
    log_hr("Activate(IAudioEndpointVolume)",hr);
    if(FAILED(hr)) goto cleanup;
    hr=endpoint_volume->lpVtbl->GetMasterVolumeLevelScalar(endpoint_volume,&old_volume);
    log_hr("GetMasterVolumeLevelScalar",hr);
    if(FAILED(hr)) goto cleanup;
    hr=endpoint_volume->lpVtbl->SetMasterVolumeLevelScalar(endpoint_volume,SAFE_ENDPOINT_VOLUME,0);
    log_hr("SetMasterVolumeLevelScalar(1pct)",hr);
    if(FAILED(hr)) goto cleanup;
    volume_capped=1;
    {
        float verify=1.0f;
        hr=endpoint_volume->lpVtbl->GetMasterVolumeLevelScalar(endpoint_volume,&verify);
        log_hr("VerifyMasterVolume",hr);
        if(FAILED(hr) || verify>0.0101f) { log_s("SAFETY_VOLUME_VERIFY_FAIL\r\n"); goto cleanup; }
    }
    log_s("SAFETY_VOLUME=1_PERCENT\r\n");

    hr=device->lpVtbl->Activate(device,&IID_ISpatialAudioClient,CLSCTX_INPROC_SERVER,0,(void **)&client);
    log_hr("Activate(ISpatialAudioClient)",hr);
    if(FAILED(hr)) goto cleanup;

    {
        UINT32 max_dynamic=0;
        hr=client->lpVtbl->GetMaxDynamicObjectCount(client,&max_dynamic);
        log_hr("GetMaxDynamicObjectCount",hr);
        if(FAILED(hr)) goto cleanup;
        log_s("max_dynamic=");log_u32(max_dynamic);log_nl();
    }

    hr=client->lpVtbl->GetSupportedAudioObjectFormatEnumerator(client,&formats);
    log_hr("GetSupportedAudioObjectFormatEnumerator",hr);
    if(FAILED(hr)) goto cleanup;
    {
        UINT32 count=0;
        hr=formats->lpVtbl->GetCount(formats,&count);
        log_hr("FormatEnumerator.GetCount",hr);
        if(FAILED(hr)) goto cleanup;
        log_s("format_count=");log_u32(count);log_nl();
        for(UINT32 i=0;i<count;i++) {
            WAVEFORMATEX *candidate=0;
            hr=formats->lpVtbl->GetFormat(formats,i,&candidate);
            if(SUCCEEDED(hr) && exact_object_format(candidate)) { format=candidate; break; }
            if(candidate) CoTaskMemFree(candidate);
        }
    }
    if(!format) { log_s("NO_FLOAT32_MONO_48K_OBJECT_FORMAT\r\n"); goto cleanup; }
    log_s("OBJECT_FORMAT=float32 mono 48000\r\n");

    hr=client->lpVtbl->IsAudioObjectFormatSupported(client,format);
    log_hr("IsAudioObjectFormatSupported",hr);
    if(FAILED(hr)) goto cleanup;
    {
        UINT32 max_frames=0;
        hr=client->lpVtbl->GetMaxFrameCount(client,format,&max_frames);
        log_hr("GetMaxFrameCount",hr);
        if(FAILED(hr)) goto cleanup;
        log_s("max_frame_count=");log_u32(max_frames);log_nl();
    }

    event=CreateEventW(0,0,0,0);
    if(!event) { log_s("CreateEventW FAIL\r\n"); goto cleanup; }

    SpatialAudioObjectRenderStreamActivationParams params;
    memset(&params,0,sizeof(params));
    params.ObjectFormat=format;
    params.StaticObjectTypeMask=AUDIO_OBJECT_FRONT_CENTER;
    params.MinDynamicObjectCount=0;
    params.MaxDynamicObjectCount=1;
    params.Category=AUDIO_CATEGORY_SOUND_EFFECTS;
    params.EventHandle=event;

    PROPVARIANT pv;
    memset(&pv,0,sizeof(pv));
    pv.vt=VT_BLOB;
    pv.u.blob.cbSize=(uint32_t)sizeof(params);
    pv.u.blob.pBlobData=(BYTE *)&params;

    hr=client->lpVtbl->ActivateSpatialAudioStream(client,&pv,&IID_ISpatialAudioObjectRenderStream,(void **)&stream);
    log_hr("ActivateSpatialAudioStream",hr);
    if(FAILED(hr)) goto cleanup;

    hr=stream->lpVtbl->Start(stream);
    log_hr("SpatialStream.Start",hr);
    if(FAILED(hr)) goto cleanup;

    uint64_t submitted=0;
    double osc_prev=0.0, osc_cur=TEST_SIN_W;
    int ready_logged=0;
    while(submitted<TEST_TOTAL_FRAMES) {
        DWORD wr=WaitForSingleObject(event,1000);
        if(wr!=WAIT_OBJECT_0) {
            log_s("WAIT_FAIL_OR_TIMEOUT code=");log_u32(wr);log_nl();
            goto stop_stream;
        }
        UINT32 available=0,frames=0;
        hr=stream->lpVtbl->BeginUpdatingAudioObjects(stream,&available,&frames);
        if(FAILED(hr)) { log_hr("BeginUpdatingAudioObjects",hr); goto stop_stream; }

        if(!object) {
            object_type = available ? AUDIO_OBJECT_DYNAMIC : AUDIO_OBJECT_FRONT_CENTER;
            hr=stream->lpVtbl->ActivateSpatialAudioObject(stream,object_type,&object);
            log_hr(object_type==AUDIO_OBJECT_DYNAMIC ? "ActivateSpatialAudioObject(Dynamic)" : "ActivateSpatialAudioObject(FrontCenter)",hr);
            if(FAILED(hr)) { stream->lpVtbl->EndUpdatingAudioObjects(stream); goto stop_stream; }
        }

        BYTE *bytes=0; UINT32 byte_count=0;
        hr=object->lpVtbl->GetBuffer(object,&bytes,&byte_count);
        if(FAILED(hr) || !bytes || byte_count<frames*4u) {
            log_hr("SpatialObject.GetBuffer",hr);
            stream->lpVtbl->EndUpdatingAudioObjects(stream);
            goto stop_stream;
        }
        float *samples=(float *)bytes;
        uint64_t remain=TEST_TOTAL_FRAMES-submitted;
        UINT32 valid=(remain<frames)?(UINT32)remain:frames;
        for(UINT32 i=0;i<frames;i++) {
            float out=0.0f;
            if(i<valid) {
                out=(float)(TEST_AMPLITUDE*osc_prev);
                double next=TEST_TWO_COS_W*osc_cur-osc_prev;
                osc_prev=osc_cur; osc_cur=next;
            }
            samples[i]=out;
        }
        if(object_type==AUDIO_OBJECT_DYNAMIC) {
            hr=object->lpVtbl->SetPosition(object,1.0f,0.0f,0.0f); /* 1 m to listener's right */
            if(FAILED(hr)) { log_hr("SpatialObject.SetPosition",hr); stream->lpVtbl->EndUpdatingAudioObjects(stream); goto stop_stream; }
        }
        hr=object->lpVtbl->SetVolume(object,1.0f);
        if(FAILED(hr)) { log_hr("SpatialObject.SetVolume",hr); stream->lpVtbl->EndUpdatingAudioObjects(stream); goto stop_stream; }
        if(valid<frames) {
            hr=object->lpVtbl->SetEndOfStream(object,valid);
            if(FAILED(hr)) { log_hr("SpatialObject.SetEndOfStream",hr); stream->lpVtbl->EndUpdatingAudioObjects(stream); goto stop_stream; }
        }
        hr=stream->lpVtbl->EndUpdatingAudioObjects(stream);
        if(FAILED(hr)) { log_hr("EndUpdatingAudioObjects",hr); goto stop_stream; }
        submitted+=valid;
        if(!ready_logged) {
            log_s("OBJECT_ACTIVE type=");log_s(object_type==AUDIO_OBJECT_DYNAMIC?"dynamic":"front_center_static");
            log_s(" frames_per_pass=");log_u32(frames);log_s(" amplitude=0.05 freq=997 position_x=1m\r\n");
            log_s("READY_FOR_AUDIODG_FULL_DUMP\r\n");
            ready_logged=1;
        }
    }
    exit_code=0;

stop_stream:
    if(stream) {
        HRESULT shr=stream->lpVtbl->Stop(stream);
        log_hr("SpatialStream.Stop",shr);
    }
    log_s("submitted_frames=");log_u32((uint32_t)submitted);log_nl();

cleanup:
    if(object) object->lpVtbl->Release(object);
    if(stream) stream->lpVtbl->Release(stream);
    if(format) CoTaskMemFree(format);
    if(formats) formats->lpVtbl->Release(formats);
    if(client) client->lpVtbl->Release(client);
    if(volume_capped && endpoint_volume) {
        HRESULT rhr=endpoint_volume->lpVtbl->SetMasterVolumeLevelScalar(endpoint_volume,old_volume,0);
        log_hr("RestoreMasterVolume",rhr);
    }
    if(endpoint_volume) endpoint_volume->lpVtbl->Release(endpoint_volume);
    if(device) device->lpVtbl->Release(device);
    if(enumerator) enumerator->lpVtbl->Release(enumerator);
    if(event) CloseHandle(event);
    if(com_started) CoUninitialize();
    log_s(exit_code==0?"SPATIAL_ORACLE_RESULT PASS\r\n":"SPATIAL_ORACLE_RESULT FAIL\r\n");
    return exit_code;
}

void mainCRTStartup(void) { ExitProcess(run_probe()); }
