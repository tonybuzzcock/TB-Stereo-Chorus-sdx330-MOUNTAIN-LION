// SdxVST.cpp
// VST 2.4 wrapper for TB SDX-330 (the Roland SDX-330 Dimensional Expander).
//
// - Parameters (Params.h): the front panel's INPUT LEVEL L / R, OUTPUT LEVEL and BYPASS,
//   the current program number and the system's Direct Mix, then every parameter of the
//   16 algorithms and the EQ. All are automatable; the host saves them with the session,
//   so reopening a session restores the plugin exactly as it was left.
// - Program memory (Programs.h): user programs 1-200 (named, shared by every instance)
//   and the 100 factory programs 201-300, selected and written from the panel as on the
//   unit (PROGRAM / alpha dial / MEMORY).
// - BYPASS (the panel button, as on the unit) crossfades to the dry input over about
//   10 ms, then passes the input untouched. It isn't tied to the host's bypass, so
//   PatchWork and the other plugins in it are unaffected; programs leave it alone.
// - Stereo in, stereo out; no latency; tail for the ambience and flanger.
// - Plain C++98; the editor uses only Cocoa APIs present in Mac OS X 10.8.5.

#include "vst2_abi.h"
#include "Params.h"
#include "Editor.h"
#include "Programs.h"
#include "../DSP/Sdx330.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#if defined(__SSE__) || defined(__x86_64__) || defined(_M_X64)
  #include <xmmintrin.h>
  #define TBSDX_HAVE_SSE 1
#endif

namespace
{

const VstInt32 kUniqueID    = VST_FOURCC ('T', 'B', 'd', 'x');
const VstInt32 kVersion     = 1000;       // 1.0.0
const VstInt32 kNumChannels = 2;
const char* const kPluginName = "TB SDX-330";

using namespace tbsdx;

void copyString (char* dst, const char* src, size_t maxLen)
{
    std::strncpy (dst, src, maxLen);
    dst[maxLen] = 0;
}

class SdxVST : public EditorHost
{
public:
    explicit SdxVST (audioMasterCallback hostCallback)
        : host (hostCallback), sampleRate (44100.0), editor (0), byp (0.0f), bypK (0.01f),
          cacheNumber (-1)
    {
        std::memset (&effect, 0, sizeof (effect));
        effect.magic = kEffectMagic;
        effect.dispatcher = &dispatcherProc;
        effect.process = &processAccumulateProc;
        effect.setParameter = &setParameterProc;
        effect.getParameter = &getParameterProc;
        effect.processReplacing = &processReplacingProc;
        effect.processDoubleReplacing = 0;
        effect.numPrograms = 1;
        effect.numParams = kNumParams;
        effect.numInputs = kNumChannels;
        effect.numOutputs = kNumChannels;
        effect.flags = effFlagsCanReplacing;
#if defined(__APPLE__)
        effect.flags |= effFlagsHasEditor;
#endif
        effect.initialDelay = Sdx330::latencySamples();
        effect.ioRatio = 1.0f;
        effect.object = this;
        effect.uniqueID = kUniqueID;
        effect.version = kVersion;

        for (int i = 0; i < kNumParams; ++i) params[i] = defaultNorm (i);
        // Power on as the unit does: program 201 STEREO CHORUS
        float v[kNumParams];
        for (int i = 0; i < kNumParams; ++i) v[i] = params[i];
        programs.get (201, v);
        for (int i = kAlgo; i < kNumParams; ++i) params[i] = v[i];
        params[kProgram] = programValue (201);

        copyString (vstProgName, "Default", kVstMaxProgNameLen);
        for (int i = 0; i < kNumParams; ++i) unit.setParam (i, params[i]);
        prepare();
    }

    ~SdxVST()
    {
#if defined(__APPLE__)
        if (editor) editorClose (editor);
#endif
    }

    AEffect* getAEffect() { return &effect; }

    //==========================================================================
    // EditorHost
    float getParam (int index) const { return (index >= 0 && index < kNumParams) ? params[index] : 0.0f; }
    void getParamDisplay (int index, char* text) const
    {
        if (index >= 0 && index < kNumParams) formatParamFor (index, params[index], algoIndex (params[kAlgo]), text, kVstMaxParamStrLen + 1);
        else text[0] = 0;
    }
    float getMeter (int side) const { return unit.meter (side); }
    void guiBeginEdit (int index) { if (host) host (&effect, audioMasterBeginEdit, index, 0, 0, 0.0f); }
    void guiEndEdit (int index)   { if (host) host (&effect, audioMasterEndEdit, index, 0, 0, 0.0f); }
    void guiSetParam (int index, float value)
    {
        setParameter (index, value);
        if (host && index >= 0 && index < kNumParams)
            host (&effect, audioMasterAutomate, index, 0, 0, params[index]);
    }

    void refreshPrograms() { programs.load(); cacheNumber = -1; }
    void programName (int number, char* text, int size) const
    {
        if (! text || size <= 0) return;
        const std::string n = programs.name (number);
        std::strncpy (text, n.c_str(), (size_t) size - 1);
        text[size - 1] = 0;
    }
    bool loadProgram (int number)
    {
        if (number < 1 || number > kNumPrograms) return false;
        float v[kNumParams];
        for (int i = 0; i < kNumParams; ++i) v[i] = params[i];
        programs.get (number, v);
        for (int i = kAlgo; i < kNumParams; ++i)
        {
            if (params[i] == v[i]) continue;
            guiBeginEdit (i); guiSetParam (i, v[i]); guiEndEdit (i);
        }
        guiBeginEdit (kProgram); guiSetParam (kProgram, programValue (number)); guiEndEdit (kProgram);
        cacheNumber = -1;
        return true;
    }
    bool writeProgram (int number)
    {
        if (! ProgramStore::isUser (number)) return false;
        programs.load();                        // keep programs written by other instances
        if (! programs.put (number, params)) return false;
        const bool ok = programs.save();
        guiBeginEdit (kProgram); guiSetParam (kProgram, programValue (number)); guiEndEdit (kProgram);
        cacheNumber = -1;
        return ok;
    }
    bool renameProgram (int number, const char* name)
    {
        programs.load();
        if (! programs.setName (number, name ? name : "")) return false;
        return programs.save();
    }
    bool programEdited() const
    {
        const int number = programNumber (params[kProgram]);
        if (number != cacheNumber)
        {
            for (int i = 0; i < kNumParams; ++i) cache[i] = params[i];
            programs.get (number, cache);
            cacheNumber = number;
        }
        for (int i = kAlgo; i < kNumParams; ++i)
            if (std::fabs (cache[i] - params[i]) > 1e-4f) return true;
        return false;
    }

private:
    //==========================================================================
    static SdxVST* self (AEffect* e) { return static_cast<SdxVST*> (e->object); }

    static VstIntPtr VST_CALL dispatcherProc (AEffect* e, VstInt32 opcode, VstInt32 index,
                                              VstIntPtr value, void* ptr, float opt)
    {
        SdxVST* p = self (e);
        if (opcode == effClose) { delete p; return 1; }
        return p->dispatch (opcode, index, value, ptr, opt);
    }
    static void VST_CALL setParameterProc (AEffect* e, VstInt32 index, float value) { self (e)->setParameter (index, value); }
    static float VST_CALL getParameterProc (AEffect* e, VstInt32 index)
    {
        SdxVST* p = self (e);
        return (index >= 0 && index < kNumParams) ? p->params[index] : 0.0f;
    }
    static void VST_CALL processReplacingProc (AEffect* e, float** in, float** out, VstInt32 n) { self (e)->processReplacing (in, out, n); }
    static void VST_CALL processAccumulateProc (AEffect* e, float** in, float** out, VstInt32 n)
    {
        SdxVST* p = self (e);
        float* tmp[kNumChannels];
        for (int c = 0; c < kNumChannels; ++c) tmp[c] = p->scratch[c];
        VstInt32 done = 0;
        while (done < n)
        {
            const VstInt32 chunk = (n - done) < kScratch ? (n - done) : kScratch;
            float* ins[kNumChannels];
            for (int c = 0; c < kNumChannels; ++c) ins[c] = in[c] + done;
            p->processReplacing (ins, tmp, chunk);
            for (int c = 0; c < kNumChannels; ++c)
                for (VstInt32 i = 0; i < chunk; ++i) out[c][done + i] += tmp[c][i];
            done += chunk;
        }
    }

    //==========================================================================
    VstIntPtr dispatch (VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt)
    {
        switch (opcode)
        {
            case effOpen:              return 0;
            case effSetProgram:        return 0;
            case effGetProgram:        return 0;
            case effSetProgramName:
                if (ptr) copyString (vstProgName, (const char*) ptr, kVstMaxProgNameLen);
                return 0;
            case effGetProgramName:
                if (ptr) copyString ((char*) ptr, vstProgName, kVstMaxProgNameLen);
                return 0;
            case effGetProgramNameIndexed:
                if (ptr && index == 0) { copyString ((char*) ptr, vstProgName, kVstMaxProgNameLen); return 1; }
                return 0;

            case effGetParamLabel:
                if (ptr && index >= 0 && index < kNumParams)
                {
                    const int u = kInfo[index].unit;
                    const char* lab = "";
                    if (u == uHz) lab = "Hz";
                    else if (u == uMs) lab = "ms";
                    else if (u == uDeg) lab = "deg";
                    else if (u == udB) lab = "dB";
                    else if (index >= kDtPit1 && index < kDtPit1 + 6) lab = "cent";
                    copyString ((char*) ptr, lab, kVstMaxParamStrLen);
                }
                return 0;
            case effGetParamDisplay:
                if (ptr && index >= 0 && index < kNumParams)
                {
                    char buf[32];
                    formatParamFor (index, params[index], algoIndex (params[kAlgo]), buf, sizeof (buf));
                    copyString ((char*) ptr, buf, kVstMaxParamStrLen);
                }
                return 0;
            case effGetParamName:
                if (ptr && index >= 0 && index < kNumParams) copyString ((char*) ptr, kInfo[index].vst, kVstMaxParamStrLen);
                return 0;
            case effCanBeAutomated:
                return (index >= 0 && index < kNumParams) ? 1 : 0;

            case effSetSampleRate:
                if (opt > 0.0f) sampleRate = opt;
                return 0;
            case effSetBlockSize:      return 0;
            case effMainsChanged:
                if (value) prepare();
                return 0;

#if defined(__APPLE__)
            case effEditGetRect:
                if (ptr)
                {
                    editRect.top = 0; editRect.left = 0;
                    editRect.bottom = (short) kEditorHeight; editRect.right = (short) kEditorWidth;
                    *(ERect**) ptr = &editRect;
                    return 1;
                }
                return 0;
            case effEditOpen:
                if (editor) { editorClose (editor); editor = 0; }
                if (ptr) editor = editorOpen (ptr, this);
                return editor ? 1 : 0;
            case effEditClose:
                if (editor) { editorClose (editor); editor = 0; }
                return 0;
            case effEditIdle:          return 0;
#endif
            case effGetPlugCategory:   return kPlugCategEffect;
            case effGetEffectName:
                if (ptr) copyString ((char*) ptr, kPluginName, kVstMaxEffectNameLen);
                return 1;
            case effGetProductString:
                if (ptr) copyString ((char*) ptr, kPluginName, kVstMaxProductStrLen);
                return 1;
            case effGetVendorString:
                if (ptr) copyString ((char*) ptr, "TB", kVstMaxVendorStrLen);
                return 1;
            case effGetVendorVersion:  return kVersion;
            case effGetVstVersion:     return 2400;
            case effCanDo:
                if (ptr)
                {
                    const char* s = (const char*) ptr;
                    if (! std::strcmp (s, "plugAsChannelInsert") || ! std::strcmp (s, "plugAsSend")) return 1;
                    if (! std::strcmp (s, "receiveVstEvents") || ! std::strcmp (s, "receiveVstMidiEvent")
                        || ! std::strcmp (s, "sendVstEvents") || ! std::strcmp (s, "sendVstMidiEvent"))
                        return -1;
                }
                return 0;
            case effGetTailSize:       return (VstIntPtr) (sampleRate * 4.0);   // ambience up to 3 s, flanger feedback
            case effStartProcess:      return 0;
            case effStopProcess:       return 0;
            default:                   return 0;
        }
    }

    void setParameter (VstInt32 index, float value)
    {
        if (index < 0 || index >= kNumParams) return;
        if (! (value == value)) return;
        if (value < 0.0f) value = 0.0f;
        if (value > 1.0f) value = 1.0f;
        params[index] = value;
        unit.setParam (index, value);
    }

    void prepare()
    {
        unit.prepare (sampleRate);
        bypK = (float) (1.0 - std::exp (-1.0 / (0.003 * sampleRate)));
        byp = isOn (params[kBypass]) ? 1.0f : 0.0f;
    }

    float applyBypass (float* out, const float* dry, VstInt32 n, float start, float target) const
    {
        float b = start;
        for (VstInt32 i = 0; i < n; ++i)
        {
            b += (target - b) * bypK;
            if (std::fabs (target - b) < 1e-5f) b = target;
            if (b >= 1.0f) out[i] = dry[i];
            else           out[i] += (dry[i] - out[i]) * b;
        }
        return b;
    }

    void processReplacing (float** in, float** out, VstInt32 n)
    {
        if (n <= 0 || ! in || ! out) return;
#if TBSDX_HAVE_SSE
        const unsigned int oldCsr = _mm_getcsr();
        _mm_setcsr (oldCsr | 0x8040);
#endif
        VstInt32 done = 0;
        while (done < n)
        {
            const VstInt32 chunk = (n - done) < kScratch ? (n - done) : kScratch;
            std::memcpy (inBuf[0], in[0] + done, (size_t) chunk * sizeof (float));
            std::memcpy (inBuf[1], in[1] + done, (size_t) chunk * sizeof (float));
            unit.process (inBuf[0], inBuf[1], out[0] + done, out[1] + done, (int) chunk);
            const float target = isOn (params[kBypass]) ? 1.0f : 0.0f;
            if (target != byp || byp != 0.0f)
            {
                const float start = byp;
                byp = applyBypass (out[0] + done, inBuf[0], chunk, start, target);
                applyBypass (out[1] + done, inBuf[1], chunk, start, target);
            }
            done += chunk;
        }
#if TBSDX_HAVE_SSE
        _mm_setcsr (oldCsr);
#endif
    }

    //==========================================================================
    enum { kScratch = 512 };

    AEffect effect;
    audioMasterCallback host;
    double sampleRate;
    float params[kNumParams];
    char vstProgName[kVstMaxProgNameLen + 1];
    Sdx330 unit;
    void* editor;
    ERect editRect;
    ProgramStore programs;
    float scratch[kNumChannels][kScratch];
    float inBuf[kNumChannels][kScratch];
    float byp, bypK;
    mutable float cache[kNumParams];
    mutable int cacheNumber;
};

} // namespace

//==============================================================================
VST_EXPORT AEffect* VSTPluginMain (audioMasterCallback host)
{
    if (! host) return 0;
    SdxVST* plugin = new SdxVST (host);
    return plugin->getAEffect();
}

#if defined(__APPLE__)
VST_EXPORT AEffect* main_macho (audioMasterCallback host)
{
    return VSTPluginMain (host);
}
#endif
