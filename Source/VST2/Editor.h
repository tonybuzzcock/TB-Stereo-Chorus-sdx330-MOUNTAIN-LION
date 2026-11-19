// Editor.h
// Plain C++ interface between the VST wrapper and the Cocoa editor (PanelView.mm), so
// the wrapper itself stays free of Objective-C. The editor only exists on the Mac;
// elsewhere the plugin reports no editor.

#ifndef TBSDX_EDITOR_H
#define TBSDX_EDITOR_H

namespace tbsdx
{

// The 19" 1U face (482 x 44 mm, 10.95:1) at half the width and twice the height
// (2.74:1), 1100 x 402, then the plugin strip along the bottom
enum { kEditorWidth = 1100, kFaceHeight = 402, kStripHeight = 92, kEditorHeight = kFaceHeight + kStripHeight };

class EditorHost
{
public:
    virtual ~EditorHost() {}
    virtual float getParam (int index) const = 0;
    virtual void  getParamDisplay (int index, char* text8) const = 0;   // the LCD's value text
    virtual void  guiBeginEdit (int index) = 0;
    virtual void  guiSetParam (int index, float value) = 0;           // also notifies the host
    virtual void  guiEndEdit (int index) = 0;
    // Input level meter: peak at the converter, 1.0 = full scale; side 0 = L, 1 = R
    virtual float getMeter (int side) const = 0;

    // Program memory (numbers 1..300; 1-200 user, 201-300 preset)
    virtual void  refreshPrograms() = 0;                         // re-read the file
    virtual void  programName (int number, char* text, int size) const = 0;
    virtual bool  loadProgram (int number) = 0;                  // every program parameter + Program (host told)
    virtual bool  writeProgram (int number) = 0;                 // current settings -> user program
    virtual bool  renameProgram (int number, const char* name) = 0;
    virtual bool  programEdited() const = 0;                     // settings differ from the stored program
};

#if defined(__APPLE__)
void* editorOpen (void* parentView, EditorHost* host);
void  editorClose (void* handle);
#endif

} // namespace tbsdx

#endif
