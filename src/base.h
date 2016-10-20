#ifndef _AMINOBASE_H
#define _AMINOBASE_H

#include "gfx.h"
#include "base_js.h"
#include "base_weak.h"
#include "fonts.h"
#include "images.h"

#include <uv.h>
#include "shaders.h"
#include "mathutils.h"
#include <stdio.h>
#include <vector>
#include <stack>
#include <stdlib.h>
#include <string>
#include <map>

#include "freetype-gl.h"
#include "mat4.h"
#include "shader.h"
#include "vertex-buffer.h"
#include "texture-font.h"

extern "C" {
    #include "nanojpeg.h"
    #include "upng.h"
}

#define DEBUG_CRASH false

const int GROUP = 1;
const int RECT  = 2;
const int TEXT  = 3;
const int ANIM  = 4;
const int POLY  = 5;
const int MODEL = 6;

class AminoText;
class AminoGroup;
class AminoAnim;
class AminoRenderer;

/**
 * Amino main class to call from JavaScript.
 *
 * Note: abstract
 */
class AminoGfx : public AminoJSEventObject {
public:
    AminoGfx(std::string name);
    ~AminoGfx();

    static NAN_MODULE_INIT(InitClasses);

    bool addAnimation(AminoAnim *anim);
    void removeAnimation(AminoAnim *anim);

    void deleteTextureAsync(GLuint textureId);
    void deleteBufferAsync(GLuint bufferId);

    //text
    void textUpdateNeeded(AminoText *text);
    amino_atlas_t getAtlasTexture(texture_atlas_t *atlas, bool createIfMissing);

protected:
    bool started = false;
    bool rendering = false;
    Nan::Callback *startCallback = NULL;

    //params
    Nan::Persistent<v8::Object> createParams;

    //renderer
    AminoRenderer *renderer = NULL;
    AminoGroup *root = NULL;
    int viewportW;
    int viewportH;
    bool viewportChanged;
    int32_t swapInterval = 0;
    int rendererErrors = 0;

    //text
    std::vector<AminoText *> textUpdates;

    void updateTextNodes();
    virtual void atlasTextureHasChanged(texture_atlas_t *atlas);
    void updateAtlasTexture(texture_atlas_t *atlas);
    void updateAtlasTextureHandler(AsyncValueUpdate *update, int state);

    //performance (FPS)
    double fpsStart = 0;
    double fpsCycleStart;
    double fpsCycleEnd;
    double fpsCycleMin;
    double fpsCycleMax;
    double fpsCycleAvg;
    int fpsCount;

    double lastFPS = 0;
    double lastCycleStart = 0;
    double lastCycleMax = 0;
    double lastCycleMin = 0;
    double lastCycleAvg = 0;

    //thread
    uv_thread_t thread;
    bool threadRunning = false;
    uv_async_t asyncHandle;

    //properties
    FloatProperty *propX;
    FloatProperty *propY;

    FloatProperty *propW;
    FloatProperty *propH;

    FloatProperty *propR;
    FloatProperty *propG;
    FloatProperty *propB;

    FloatProperty *propOpacity;

    Utf8Property *propTitle;

    BooleanProperty *propShowFPS;

    //animations
    std::vector<AminoAnim *> animations;
    pthread_mutex_t animLock; //Note: short cycles

    //creation
    static void Init(Nan::ADDON_REGISTER_FUNCTION_ARGS_TYPE target, AminoJSObjectFactory* factory);

    void setup() override;

    //abstract methods
    virtual void initRenderer();
    void setupRenderer();
    void addRuntimeProperty();
    virtual void populateRuntimeProperties(v8::Local<v8::Object> &obj);

    virtual void start();
    void ready();

    void startRenderingThread();
    void stopRenderingThread();
    bool isRenderingThreadRunning();
    static void renderingThread(void *arg);
    static void handleRenderEvents(uv_async_t *handle);
    virtual void handleSystemEvents() = 0;

    virtual void render();
    void processAnimations();
    virtual bool bindContext() = 0;
    virtual void renderScene();
    virtual void renderingDone() = 0;
    bool isRendering();

    void destroy() override;

    virtual bool getScreenInfo(int &w, int &h, int &refreshRate, bool &fullscreen) { return false; };
    void updateSize(int w, int h); //call after size event
    void updatePosition(int x, int y); //call after position event

    void fireEvent(v8::Local<v8::Object> &obj);

    bool handleSyncUpdate(AnyProperty *prop, void *data) override;
    virtual void updateWindowSize() = 0;
    virtual void updateWindowPosition() = 0;
    virtual void updateWindowTitle() = 0;

    void setRoot(AminoGroup *group);

private:
    void preInit(Nan::NAN_METHOD_ARGS_TYPE info) override;

    //JS methods
    static NAN_METHOD(Start);
    static NAN_METHOD(Destroy);

    static NAN_METHOD(SetRoot);
    static NAN_METHOD(ClearAnimations);
    static NAN_METHOD(GetStats);
    static NAN_METHOD(GetTime);

    //animation
    void clearAnimations();

    //texture & buffer
    void deleteTexture(AsyncValueUpdate *update, int state);
    void deleteBuffer(AsyncValueUpdate *update, int state);

    //stats
    void getStats(v8::Local<v8::Object> &obj) override;
    void measureRenderingStart();
    void measureRenderingEnd();
};

/**
 * Base class for all rendering nodes.
 *
 * Note: abstract.
 */
class AminoNode : public AminoJSObject {
public:
    int type;

    //location
    FloatProperty *propX;
    FloatProperty *propY;
    FloatProperty *propZ;

    //size (optional)
    FloatProperty *propW = NULL;
    FloatProperty *propH = NULL;

    //origin (optional)
    FloatProperty *propOriginX = NULL;
    FloatProperty *propOriginY = NULL;

    //zoom factor
    FloatProperty *propScaleX;
    FloatProperty *propScaleY;

    //rotation
    FloatProperty *propRotateX;
    FloatProperty *propRotateY;
    FloatProperty *propRotateZ;

    //opacity
    FloatProperty *propOpacity;

    //visibility
    BooleanProperty *propVisible;

    AminoNode(std::string name, int type): AminoJSObject(name), type(type) {
        //empty
    }

    ~AminoNode() {
        //see destroy
    }

    void preInit(Nan::NAN_METHOD_ARGS_TYPE info) override {
        assert(info.Length() >= 1);

        //set amino instance
        v8::Local<v8::Object> jsObj = info[0]->ToObject();
        AminoGfx *obj = Nan::ObjectWrap::Unwrap<AminoGfx>(jsObj);

        assert(obj);

        //bind to queue
        this->setEventHandler(obj);
        Nan::Set(handle(), Nan::New("amino").ToLocalChecked(), jsObj);
    }

    void setup() override {
        AminoJSObject::setup();

        //register native properties
        propX = createFloatProperty("x");
        propY = createFloatProperty("y");
        propZ = createFloatProperty("z");

        propScaleX = createFloatProperty("sx");
        propScaleY = createFloatProperty("sy");

        propRotateX = createFloatProperty("rx");
        propRotateY = createFloatProperty("ry");
        propRotateZ = createFloatProperty("rz");

        propOpacity = createFloatProperty("opacity");
        propVisible = createBooleanProperty("visible");
    }

    /**
     * Free all resources.
     */
    void destroy() override {
        AminoJSObject::destroy();

        //to be overwritten
    }

    /**
     * Get AminoGfx instance.
     */
    AminoGfx* getAminoGfx() {
        assert(eventHandler);

        return (AminoGfx *)eventHandler;
    }

    /**
     * Validate renderer instance. Must be called in JS method handler.
     */
    bool checkRenderer(AminoNode *node) {
        return checkRenderer(node->getAminoGfx());
    }

    /**
     * Validate renderer instance. Must be called in JS method handler.
     */
    bool checkRenderer(AminoGfx *amino) {
        assert(eventHandler);

        if (eventHandler != amino) {
            Nan::ThrowTypeError("invalid renderer");
            return false;
        }

        return true;
    }
};

/**
 * Text factory.
 */
class AminoTextFactory : public AminoJSObjectFactory {
public:
    AminoTextFactory(Nan::FunctionCallback callback);

    AminoJSObject* create() override;
};

/**
 * Text node class.
 */
class AminoText : public AminoNode {
public:
    //text
    Utf8Property *propText;

    //color
    FloatProperty *propR;
    FloatProperty *propG;
    FloatProperty *propB;

    //box
    FloatProperty *propW;
    FloatProperty *propH;

    Utf8Property *propWrap;
    int wrap = WRAP_NONE;

    //font
    ObjectProperty *propFont;
    AminoFontSize *fontSize = NULL;
    vertex_buffer_t *buffer = NULL;

    //alignment
    Utf8Property *propAlign;
    Utf8Property *propVAlign;
    int align = ALIGN_LEFT;
    int vAlign = VALIGN_BASELINE;

    //lines
    Int32Property *propMaxLines;
    int lineNr = 1;
    float lineW = 0;

    //mutex
    static uv_mutex_t freeTypeMutex;
    static bool freeTypeMutexInitialized;

    //constants
    static const int ALIGN_LEFT   = 0x0;
    static const int ALIGN_CENTER = 0x1;
    static const int ALIGN_RIGHT  = 0x2;

    static const int VALIGN_BASELINE = 0x0;
    static const int VALIGN_TOP      = 0x1;
    static const int VALIGN_MIDDLE   = 0x2;
    static const int VALIGN_BOTTOM   = 0x3;

    static const int WRAP_NONE = 0x0;
    static const int WRAP_END  = 0x1;
    static const int WRAP_WORD = 0x2;

    AminoText(): AminoNode(getFactory()->name, TEXT) {
        //mutex
        if (!freeTypeMutexInitialized) {
            freeTypeMutexInitialized = true;

            int res = uv_mutex_init(&freeTypeMutex);

            assert(res == 0);
        }
    }

    ~AminoText() {
        //empty
    }

    /**
     * Free buffers.
     */
    void destroy() override {
        AminoNode::destroy();

        if (buffer) {
            vertex_buffer_delete(buffer);
            buffer = NULL;
        }

        //release object values
        propFont->destroy();
    }

    /**
     * Setup instance.
     */
    void setup() override {
        AminoNode::setup();

        //register native properties
        propText = createUtf8Property("text");

        propR = createFloatProperty("r");
        propG = createFloatProperty("g");
        propB = createFloatProperty("b");

        propW = createFloatProperty("w");
        propH = createFloatProperty("h");

        propOriginX = createFloatProperty("originX");
        propOriginY = createFloatProperty("originY");

        propWrap = createUtf8Property("wrap");
        propAlign = createUtf8Property("align");
        propVAlign = createUtf8Property("vAlign");
        propFont = createObjectProperty("font");

        propMaxLines = createInt32Property("maxLines");
    }

    //creation

    /**
     * Create text factory.
     */
    static AminoTextFactory* getFactory() {
        static AminoTextFactory *textFactory = NULL;

        if (!textFactory) {
            textFactory = new AminoTextFactory(New);
        }

        return textFactory;
    }

    /**
     * Initialize Group template.
     */
    static v8::Local<v8::Function> GetInitFunction() {
        v8::Local<v8::FunctionTemplate> tpl = AminoJSObject::createTemplate(getFactory());

        //prototype methods
        // -> none

        //template function
        return Nan::GetFunction(tpl).ToLocalChecked();
    }

    /**
     * Handle async property updates.
     */
    void handleAsyncUpdate(AsyncPropertyUpdate *update) override {
        //default: set value
        AminoJSObject::handleAsyncUpdate(update);

        //check font updates
        AnyProperty *property = update->property;
        bool updated = false;

        if (property == propW || property == propH || property == propText || property == propMaxLines) {
            updated = true;
        } else if (property == propWrap) {
            //wrap
            std::string str = propWrap->value;
            int oldWrap = wrap;

            if (str == "none") {
                wrap = WRAP_NONE;
            } else if (str == "word") {
                wrap = WRAP_WORD;
            } else if (str == "end") {
                wrap = WRAP_END;
            } else {
                //error
                printf("unknown wrap mode: %s\n", str.c_str());
            }

            if (wrap != oldWrap) {
                updated = true;
            }
        } else if (property == propAlign) {
            //align
            std::string str = propAlign->value;
            int oldAlign = align;

            if (str == "left") {
                align = AminoText::ALIGN_LEFT;
            } else if (str == "center") {
                align = AminoText::ALIGN_CENTER;
            } else if (str == "right") {
                align = AminoText::ALIGN_RIGHT;
            } else {
                //error
                printf("unknown align mode: %s\n", str.c_str());
            }

            if (align != oldAlign) {
                updated = true;
            }
        } else if (property == propVAlign) {
            //vAlign
            std::string str = propVAlign->value;
            int oldVAlign = vAlign;

            if (str == "top") {
                vAlign = AminoText::VALIGN_TOP;
            } else if (str == "middle") {
                vAlign = AminoText::VALIGN_MIDDLE;
            } else if (str == "baseline") {
                vAlign = AminoText::VALIGN_BASELINE;
            } else if (str == "bottom") {
                vAlign = AminoText::VALIGN_BOTTOM;
            } else {
                //error
                printf("unknown vAlign mode: %s\n", str.c_str());
            }

            if (vAlign != oldVAlign) {
                updated = true;
            }
        } else if (property == propFont) {
            //font
            AminoFontSize *fs = (AminoFontSize *)propFont->value;

            if (fontSize == fs) {
                return;
            }

            //new font
            fontSize = fs;
            texture.textureId = INVALID_TEXTURE;

            //debug
            //printf("-> use font: %s\n", fs->font->fontName.c_str());

            updated = true;
        }

        if (updated) {
            getAminoGfx()->textUpdateNeeded(this);
        }
    }

    /**
     * Update the rendered text.
     */
    bool layoutText();

    /**
     * Create or update a font texture.
     */
    void updateTexture();
    static void updateTextureFromAtlas(GLuint textureId, texture_atlas_t *atlas);

    /**
     * Get font texture.
     */
    GLuint getTextureId();

private:
    amino_atlas_t texture = { INVALID_TEXTURE };

    /**
     * JS object construction.
     */
    static NAN_METHOD(New) {
        AminoJSObject::createInstance(info, getFactory());
    }

    static void addTextGlyphs(vertex_buffer_t *buffer, texture_font_t *font, const char *text, vec2 *pen, int wrap, int width, int *lineNr, int maxLines, float *lineW);
};

/**
 * Animation factory.
 */
class AminoAnimFactory : public AminoJSObjectFactory {
public:
    AminoAnimFactory(Nan::FunctionCallback callback);

    AminoJSObject* create() override;
};

/**
 * Animation class.
 */
class AminoAnim : public AminoJSObject {
private:
    AnyProperty *prop;

    bool started = false;
    bool ended = false;

    //properties
    float start;
    float end;
    int count;
    float duration;
    bool autoreverse;
    int direction = FORWARD;
    int timeFunc = TF_CUBIC_IN_OUT;
    Nan::Callback *then = NULL;

    //start pos
    float zeroPos;
    bool hasZeroPos = false;

    //sync time
    double refTime;
    bool hasRefTime = false;

    double startTime = 0;
    double lastTime  = 0;
    double pauseTime = 0;

    static const int FORWARD  = 1;
    static const int BACKWARD = 2;

    static const int FOREVER = -1;

public:
    static const int TF_LINEAR       = 0x0;
    static const int TF_CUBIC_IN     = 0x1;
    static const int TF_CUBIC_OUT    = 0x2;
    static const int TF_CUBIC_IN_OUT = 0x3;

    AminoAnim(): AminoJSObject(getFactory()->name) {
        //empty
    }

    ~AminoAnim() {
        //see destroy
    }

    void preInit(Nan::NAN_METHOD_ARGS_TYPE info) override {
        assert(info.Length() == 3);

        //params
        AminoGfx *obj = Nan::ObjectWrap::Unwrap<AminoGfx>(info[0]->ToObject());
        AminoNode *node = Nan::ObjectWrap::Unwrap<AminoNode>(info[1]->ToObject());
        unsigned int propId = info[2]->Uint32Value();

        assert(obj);
        assert(node);

        if (!node->checkRenderer(obj)) {
            return;
        }

        //get property
        AnyProperty *prop = node->getPropertyWithId(propId);

        if (!prop || prop->type != PROPERTY_FLOAT) {
            Nan::ThrowTypeError("property cannot be animated");
            return;
        }

        //bind to queue (retains AminoGfx reference)
        this->setEventHandler(obj);
        this->prop = prop;

        //retain property
        prop->retain();

        //enqueue
        obj->addAnimation(this);
    }

    void destroy() override {
        AminoJSObject::destroy();

        if (prop) {
            prop->release();
            prop = NULL;
        }

        if (then) {
            delete then;
            then = NULL;
        }
    }

    //creation

    /**
     * Create anim factory.
     */
    static AminoAnimFactory* getFactory() {
        static AminoAnimFactory *animFactory = NULL;

        if (!animFactory) {
            animFactory = new AminoAnimFactory(New);
        }

        return animFactory;
    }

    /**
     * Initialize Group template.
     */
    static v8::Local<v8::Function> GetInitFunction() {
        v8::Local<v8::FunctionTemplate> tpl = AminoJSObject::createTemplate(getFactory());

        //methods
        Nan::SetPrototypeMethod(tpl, "_start", Start);
        Nan::SetPrototypeMethod(tpl, "stop", Stop);

        //template function
        return Nan::GetFunction(tpl).ToLocalChecked();
    }

    /**
     * JS object construction.
     */
    static NAN_METHOD(New) {
        AminoJSObject::createInstance(info, getFactory());
    }

    static NAN_METHOD(Start) {
        assert(info.Length() == 1);

        AminoAnim *obj = Nan::ObjectWrap::Unwrap<AminoAnim>(info.This());
        v8::Local<v8::Object> data = info[0]->ToObject();

        assert(obj);

        obj->handleStart(data);
    }

    void handleStart(v8::Local<v8::Object> &data) {
        if (started) {
            Nan::ThrowTypeError("already started");
            return;
        }

        //parameters
        start       = Nan::Get(data, Nan::New<v8::String>("from").ToLocalChecked()).ToLocalChecked()->NumberValue();
        end         = Nan::Get(data, Nan::New<v8::String>("to").ToLocalChecked()).ToLocalChecked()->NumberValue();
        duration    = Nan::Get(data, Nan::New<v8::String>("duration").ToLocalChecked()).ToLocalChecked()->NumberValue();
        count       = Nan::Get(data, Nan::New<v8::String>("count").ToLocalChecked()).ToLocalChecked()->IntegerValue();
        autoreverse = Nan::Get(data, Nan::New<v8::String>("autoreverse").ToLocalChecked()).ToLocalChecked()->BooleanValue();

        //time func
        v8::String::Utf8Value str(Nan::Get(data, Nan::New<v8::String>("timeFunc").ToLocalChecked()).ToLocalChecked());
        std::string tf = std::string(*str);

        if (tf == "cubicIn") {
            timeFunc = TF_CUBIC_IN;
        } else if (tf == "cubicOut") {
            timeFunc = TF_CUBIC_OUT;
        } else if (tf == "cubicInOut") {
            timeFunc = TF_CUBIC_IN_OUT;
        } else {
            timeFunc = TF_LINEAR;
        }

        //then
        v8::MaybeLocal<v8::Value> maybeThen = Nan::Get(data, Nan::New<v8::String>("then").ToLocalChecked());

        if (!maybeThen.IsEmpty()) {
            v8::Local<v8::Value> thenLocal = maybeThen.ToLocalChecked();

            if (thenLocal->IsFunction()) {
                then = new Nan::Callback(thenLocal.As<v8::Function>());
            }
        }

        //optional values

        // 1) pos
        v8::MaybeLocal<v8::Value> maybePos = Nan::Get(data, Nan::New<v8::String>("pos").ToLocalChecked());

        if (!maybePos.IsEmpty()) {
            v8::Local<v8::Value> posLocal = maybePos.ToLocalChecked();

            if (posLocal->IsNumber()) {
                hasZeroPos = true;
                zeroPos = posLocal->NumberValue();
            }
        }

        // 2) refTime
        v8::MaybeLocal<v8::Value> maybeRefTime = Nan::Get(data, Nan::New<v8::String>("refTime").ToLocalChecked());

        if (!maybeRefTime.IsEmpty()) {
            v8::Local<v8::Value> refTimeLocal = maybeRefTime.ToLocalChecked();

            if (refTimeLocal->IsNumber()) {
                hasRefTime = true;
                refTime = refTimeLocal->NumberValue();
            }
        }

        //start
        started = true;
    }

    /**
     * Cubic-in time function.
     */
    static float cubicIn(float t) {
        return pow(t, 3);
    }

    /**
     * Cubic-out time function.
     */
    static float cubicOut(float t) {
        return 1 - cubicIn(1 - t);
    }

    /**
     * Cubic-in-out time function.
     */
    static float cubicInOut(float t) {
        if (t < 0.5) {
            return cubicIn(t * 2.0) / 2.0;
        }

        return 1 - cubicIn((1 - t) * 2) / 2;
    }

    /**
     * Call time function.
     */
    float timeToPosition(float t) {
        float t2 = 0;

        switch (timeFunc) {
            case TF_CUBIC_IN:
                t2 = cubicIn(t);
                break;

            case TF_CUBIC_OUT:
                t2 = cubicOut(t);
                break;

            case TF_CUBIC_IN_OUT:
                t2 = cubicInOut(t);
                break;

            case TF_LINEAR:
            default:
                t2 = t;
                break;
        }

        return start + (end - start) * t2;
    }

    /**
     * Toggle animation direction.
     *
     * Note: works only if autoreverse is enabled
     */
    void toggle() {
        if (autoreverse) {
            if (direction == FORWARD) {
                direction = BACKWARD;
            } else {
                direction = FORWARD;
            }
        }
    }

    /**
     * Apply animation value.
     *
     * @param value current property value.
     */
    void applyValue(float value) {
        if (!prop) {
            return;
        }

        FloatProperty *floatProp = (FloatProperty *)prop;

        floatProp->setValue(value);
    }

    //TODO pause
    //TODO resume
    //TODO reset (start from beginning)

    static NAN_METHOD(Stop) {
        AminoAnim *obj = Nan::ObjectWrap::Unwrap<AminoAnim>(info.This());

        assert(obj);

        obj->stop();
    }

    /**
     * Stop animation.
     *
     * Note: has to be called on main thread!
     */
    void stop() {
        if (!destroyed) {
            //remove animation
            if (eventHandler) {
                ((AminoGfx *)eventHandler)->removeAnimation(this);
            }

            //free resources
            destroy();
        }
    }

    /**
     * End the animation.
     */
    void endAnimation() {
        if (ended) {
            return;
        }

        if (DEBUG_BASE) {
            printf("Anim: endAnimation()\n");
        }

        ended = true;

        //apply end state
        applyValue(end);

        //callback function
        if (then) {
            if (DEBUG_BASE) {
                printf("-> callback used\n");
            }

            enqueueJSCallbackUpdate((jsUpdateCallback)&AminoAnim::callThen, NULL, NULL);
        }

        //stop
        enqueueJSCallbackUpdate((jsUpdateCallback)&AminoAnim::callStop, NULL, NULL);
    }

    /**
     * Perform then() call on main thread.
     */
    void callThen(JSCallbackUpdate *update) {
        //create scope
        Nan::HandleScope scope;

        //call
        then->Call(handle(), 0, NULL);
    }

    /**
     * Perform stop() call on main thread.
     */
    void callStop(JSCallbackUpdate *update) {
        stop();
    }

    /**
     * Next animation step.
     */
    void update(double currentTime) {
        //check active
    	if (!started || ended) {
            return;
        }

        //check remaining loops
        if (count == 0) {
            endAnimation();
            return;
        }

        //handle first start
        if (startTime == 0) {
            startTime = currentTime;
            lastTime = currentTime;
            pauseTime = 0;

            //sync with reference time
            if (hasRefTime) {
                double diff = currentTime - refTime;

                if (diff < 0) {
                    //in future: wait
                    startTime = 0;
                    lastTime = 0;
                    return;
                }

                //check passed iterations
                int cycles = diff / duration;

                if (cycles > 0) {
                    //check end of animation
                    if (count != FOREVER) {
                        if (cycles >= count) {
                            //end reached
                            endAnimation();
                            return;
                        }

                        //reduce
                        count -= cycles;
                    }

                    diff -= cycles * duration;

                    //check direction
                    if (cycles & 0x1) {
                        toggle();
                    }
                }

                //shift start time
                startTime -= diff;
            }

            //adjust animation position
            if (hasZeroPos && zeroPos > start && zeroPos <= end) {
                float pos = (zeroPos - start) / (end - start);

                //shift start time
                startTime -= pos * duration;
            }
        }

        //validate time (should never happen if time is monotonic)
        if (currentTime < startTime) {
            //smooth animation
            startTime = currentTime - (lastTime - startTime);
            lastTime = currentTime;
        }

        //process
        double timePassed = currentTime - startTime;
        float t = timePassed / duration;

        lastTime = currentTime;

        if (t > 1) {
            //end reached
            bool doToggle = false;

            if (count == FOREVER) {
                doToggle = true;
            }

            if (count > 0) {
                count--;

                if (count > 0) {
                    doToggle = true;
                } else {
                    endAnimation();
                    return;
                }
            }

            if (doToggle) {
                //next cycle

                //calc exact time offset
                double overTime = timePassed - duration;

                if (overTime > duration) {
                    int times = overTime / duration;

                    overTime -= times * duration;

                    if (times & 0x1) {
                        doToggle = false;
                    }
                }

                startTime = currentTime - overTime;
                t = overTime / duration;

                if (doToggle) {
                    toggle();
                }
            } else {
                //end position
                t = 1;
            }
        }

        if (direction == BACKWARD) {
            t = 1 - t;
        }

        //apply time function
        float value = timeToPosition(t);

        applyValue(value);
    }
};

/**
 * Rect factory.
 */
class AminoRectFactory : public AminoJSObjectFactory {
public:
    AminoRectFactory(Nan::FunctionCallback callback, bool hasImage);

    AminoJSObject* create() override;

private:
    bool hasImage;
};

/**
 * Rectangle node class.
 */
class AminoRect : public AminoNode {
public:
    bool hasImage;

    //color (no texture)
    FloatProperty *propR;
    FloatProperty *propG;
    FloatProperty *propB;

    //texture (image only)
    ObjectProperty *propTexture;

    //texture offset (image only)
    FloatProperty *propLeft;
    FloatProperty *propRight;
    FloatProperty *propTop;
    FloatProperty *propBottom;

    //repeat
    Utf8Property *propRepeat;
    bool repeatX = false;
    bool repeatY = false;

    AminoRect(bool hasImage): AminoNode(hasImage ? getImageViewFactory()->name:getRectFactory()->name, RECT) {
        this->hasImage = hasImage;
    }

    ~AminoRect() {
        //empty
    }

    /**
     * Free resources.
     */
    void destroy() override {
        AminoNode::destroy();

        //release object values
        propTexture->destroy();
    }

    /**
     * Setup rect properties.
     */
    void setup() override {
        AminoNode::setup();

        //register native properties
        propW = createFloatProperty("w");
        propH = createFloatProperty("h");

        propOriginX = createFloatProperty("originX");
        propOriginY = createFloatProperty("originY");

        if (hasImage) {
            propTexture = createObjectProperty("image");

            propLeft = createFloatProperty("left");
            propRight = createFloatProperty("right");
            propTop = createFloatProperty("top");
            propBottom = createFloatProperty("bottom");

            propRepeat = createUtf8Property("repeat");
        } else {
            propR = createFloatProperty("r");
            propG = createFloatProperty("g");
            propB = createFloatProperty("b");
        }
    }

    //creation

    /**
     * Get rect factory.
     */
    static AminoRectFactory* getRectFactory() {
        static AminoRectFactory *rectFactory = NULL;

        if (!rectFactory) {
            rectFactory = new AminoRectFactory(NewRect, false);
        }

        return rectFactory;
    }

    /**
     * Initialize Rect template.
     */
    static v8::Local<v8::Function> GetRectInitFunction() {
        v8::Local<v8::FunctionTemplate> tpl = AminoJSObject::createTemplate(getRectFactory());

        //no methods

        //template function
        return Nan::GetFunction(tpl).ToLocalChecked();
    }

    /**
     * JS object construction.
     */
    static NAN_METHOD(NewRect) {
        AminoJSObject::createInstance(info, getRectFactory());
    }

    //ImageView

    //creation

    /**
     * Get image view factory.
     */
    static AminoRectFactory* getImageViewFactory() {
        static AminoRectFactory *rectFactory = NULL;

        if (!rectFactory) {
            rectFactory = new AminoRectFactory(NewImageView, true);
        }

        return rectFactory;
    }

    /**
     * Initialize ImageView template.
     */
    static v8::Local<v8::Function> GetImageViewInitFunction() {
        v8::Local<v8::FunctionTemplate> tpl = AminoJSObject::createTemplate(getImageViewFactory());

        //no methods

        //template function
        return Nan::GetFunction(tpl).ToLocalChecked();
    }

    /**
     * JS object construction.
     */
    static NAN_METHOD(NewImageView) {
        AminoJSObject::createInstance(info, getImageViewFactory());
    }

    /**
     * Handle async property updates.
     */
    void handleAsyncUpdate(AsyncPropertyUpdate *update) override {
        //default: set value
        AminoJSObject::handleAsyncUpdate(update);

        //check property updates
        AnyProperty *property = update->property;

        if (property == propRepeat) {
            std::string str = propRepeat->value;

            if (str == "no-repeat") {
                repeatX = false;
                repeatY = false;
            } else if (str == "repeat") {
                repeatX = true;
                repeatY = true;
            } else if (str == "repeat-x") {
                repeatX = true;
                repeatY = false;
            } else if (str == "repeat-y") {
                repeatX = false;
                repeatY = true;
            } else {
                //error
                printf("unknown repeat mode: %s\n", str.c_str());
            }

            return;
        }
    }
};

/**
 * Polygon factory.
 */
class AminoPolygonFactory : public AminoJSObjectFactory {
public:
    AminoPolygonFactory(Nan::FunctionCallback callback);

    AminoJSObject* create() override;
};

/**
 * AminoPolygon node class.
 */
class AminoPolygon : public AminoNode {
public:
    //fill
    FloatProperty *propFillR;
    FloatProperty *propFillG;
    FloatProperty *propFillB;

    //dimension
    UInt32Property *propDimension;
    BooleanProperty *propFilled;

    //points
    FloatArrayProperty *propGeometry;

    AminoPolygon(): AminoNode(getFactory()->name, POLY) {
        //empty
    }

    ~AminoPolygon() {
    }

    void setup() override {
        AminoNode::setup();

        //register native properties
        propFillR = createFloatProperty("fillR");
        propFillG = createFloatProperty("fillG");
        propFillB = createFloatProperty("fillB");

        propDimension = createUInt32Property("dimension");
        propFilled = createBooleanProperty("filled");

        propGeometry = createFloatArrayProperty("geometry");
    }

    //creation

    /**
     * Get polygon factory.
     */
    static AminoPolygonFactory* getFactory() {
        static AminoPolygonFactory *polygonFactory = NULL;

        if (!polygonFactory) {
            polygonFactory = new AminoPolygonFactory(New);
        }

        return polygonFactory;
    }

    /**
     * Initialize Group template.
     */
    static v8::Local<v8::Function> GetInitFunction() {
        v8::Local<v8::FunctionTemplate> tpl = AminoJSObject::createTemplate(getFactory());

        //no methods

        //template function
        return Nan::GetFunction(tpl).ToLocalChecked();
    }

    /**
     * JS object construction.
     */
    static NAN_METHOD(New) {
        AminoJSObject::createInstance(info, getFactory());
    }
};

/**
 * Model factory.
 */
class AminoModelFactory : public AminoJSObjectFactory {
public:
    AminoModelFactory(Nan::FunctionCallback callback);

    AminoJSObject* create() override;
};

/**
 * AminoModel node class.
 */
class AminoModel : public AminoNode {
public:
    //fill
    FloatProperty *propFillR;
    FloatProperty *propFillG;
    FloatProperty *propFillB;

    //arrays
    FloatArrayProperty *propVertices;
    FloatArrayProperty *propNormals;
    FloatArrayProperty *propUVs;
    UShortArrayProperty *propIndices;

    //texture
    ObjectProperty *propTexture;

    //VBO
    GLuint vboVertex = INVALID_BUFFER;
    GLuint vboNormal = INVALID_BUFFER;
    GLuint vboUV = INVALID_BUFFER;
    GLuint vboIndex = INVALID_BUFFER;

    bool vboVertexModified = true;
    bool vboNormalModified = true;
    bool vboUVModified = true;
    bool vboIndexModified = true;

    AminoModel(): AminoNode(getFactory()->name, MODEL) {
        //empty
    }

    ~AminoModel() {
    }

    /**
     * Free all resources.
     */
    void destroy() override {
        AminoNode::destroy();

        //free buffers
        if (eventHandler) {
            if (vboVertex != INVALID_BUFFER) {
                ((AminoGfx *)eventHandler)->deleteBufferAsync(vboVertex);
                vboVertex = INVALID_BUFFER;
            }

            if (vboNormal != INVALID_BUFFER) {
                ((AminoGfx *)eventHandler)->deleteBufferAsync(vboNormal);
                vboNormal = INVALID_BUFFER;
            }

            if (vboIndex != INVALID_BUFFER) {
                ((AminoGfx *)eventHandler)->deleteBufferAsync(vboIndex);
                vboIndex = INVALID_BUFFER;
            }
        }
    }

    /**
     * Setup properties.
     */
    void setup() override {
        AminoNode::setup();

        //register native properties
        propW = createFloatProperty("w");
        propH = createFloatProperty("h");

        propOriginX = createFloatProperty("originX");
        propOriginY = createFloatProperty("originY");

        propFillR = createFloatProperty("fillR");
        propFillG = createFloatProperty("fillG");
        propFillB = createFloatProperty("fillB");

        propVertices = createFloatArrayProperty("vertices");
        propNormals = createFloatArrayProperty("normals");
        propUVs = createFloatArrayProperty("uvs");
        propIndices = createUShortArrayProperty("indices");

        propTexture = createObjectProperty("texture");
    }

    //creation

    /**
     * Get polygon factory.
     */
    static AminoModelFactory* getFactory() {
        static AminoModelFactory *modelFactory = NULL;

        if (!modelFactory) {
            modelFactory = new AminoModelFactory(New);
        }

        return modelFactory;
    }

    /**
     * Initialize Group template.
     */
    static v8::Local<v8::Function> GetInitFunction() {
        v8::Local<v8::FunctionTemplate> tpl = AminoJSObject::createTemplate(getFactory());

        //no methods

        //template function
        return Nan::GetFunction(tpl).ToLocalChecked();
    }

    /**
     * JS object construction.
     */
    static NAN_METHOD(New) {
        AminoJSObject::createInstance(info, getFactory());
    }

    /*
     * Handle async property updates.
     */
    void handleAsyncUpdate(AsyncPropertyUpdate *update) override {
        //default: set value
        AminoJSObject::handleAsyncUpdate(update);

        //check array updates
        AnyProperty *property = update->property;

        if (property == propVertices) {
            vboVertexModified = true;
        } else if (property == propNormals) {
            vboNormalModified = true;
        } else if (property == propUVs) {
            vboUVModified = true;
        } else if (property == propIndices) {
            vboIndexModified = true;
        }
    }
};

/**
 * Group factory.
 */
class AminoGroupFactory : public AminoJSObjectFactory {
public:
    AminoGroupFactory(Nan::FunctionCallback callback);

    AminoJSObject* create() override;
};

//group insert

typedef struct {
    AminoNode *child;
    size_t pos;
} group_insert_t;

/**
 * Group node.
 *
 * Special: supports clipping
 */
class AminoGroup : public AminoNode {
public:
    //internal
    std::vector<AminoNode *> children;

    //properties
    BooleanProperty *propClipRect;
    BooleanProperty *propDepth;

    AminoGroup(): AminoNode(getFactory()->name, GROUP) {
        //empty
    }

    ~AminoGroup() {
    }

    void setup() override {
        AminoNode::setup();

        //register native properties
        propW = createFloatProperty("w");
        propH = createFloatProperty("h");

        propOriginX = createFloatProperty("originX");
        propOriginY = createFloatProperty("originY");

        propClipRect = createBooleanProperty("clipRect");
        propDepth = createBooleanProperty("depth");
    }

    //creation

    /**
     * Get group factory.
     */
    static AminoGroupFactory* getFactory() {
        static AminoGroupFactory *groupFactory = NULL;

        if (!groupFactory) {
            groupFactory = new AminoGroupFactory(New);
        }

        return groupFactory;
    }

    /**
     * Initialize Group template.
     */
    static v8::Local<v8::Function> GetInitFunction() {
        v8::Local<v8::FunctionTemplate> tpl = AminoJSObject::createTemplate(getFactory());

        //prototype methods
        Nan::SetPrototypeMethod(tpl, "_add", Add);
        Nan::SetPrototypeMethod(tpl, "_insert", Insert);
        Nan::SetPrototypeMethod(tpl, "_remove", Remove);

        //template function
        return Nan::GetFunction(tpl).ToLocalChecked();
    }

private:
    /**
     * JS object construction.
     */
    static NAN_METHOD(New) {
        AminoJSObject::createInstance(info, getFactory());
    }

    /**
     * Add a child node.
     */
    static NAN_METHOD(Add) {
        assert(info.Length() == 1);

        AminoGroup *group = Nan::ObjectWrap::Unwrap<AminoGroup>(info.This());
        AminoNode *child = Nan::ObjectWrap::Unwrap<AminoNode>(info[0]->ToObject());

        assert(group);
        assert(child);

        if (!child->checkRenderer(group)) {
            return;
        }

        //handle async
        group->enqueueValueUpdate(child, (asyncValueCallback)&AminoGroup::addChild);
    }

    /**
     * Add a child node.
     */
    void addChild(AsyncValueUpdate *update, int state) {
        if (state != AsyncValueUpdate::STATE_APPLY) {
            return;
        }

        AminoNode *node = (AminoNode *)update->valueObj;

        //keep retained instance
        update->valueObj = NULL;

        if (DEBUG_BASE) {
            printf("-> addChild()\n");
        }

        children.push_back(node);

        //debug (provoke crash to get stack trace)
        if (DEBUG_CRASH) {
            int *foo = (int*)1;

            *foo = 78; // trigger a SIGSEGV
            assert(false);
        }
    }

    /**
     * Insert a child node.
     */
    static NAN_METHOD(Insert) {
        assert(info.Length() == 2);

        AminoGroup *group = Nan::ObjectWrap::Unwrap<AminoGroup>(info.This());
        v8::Local<v8::Value> childValue = info[0];
        AminoNode *child = Nan::ObjectWrap::Unwrap<AminoNode>(childValue->ToObject());
        int pos = info[1]->Int32Value();

        assert(group);
        assert(child);
        assert(pos >= 0);

        if (!child->checkRenderer(group)) {
            return;
        }

        //retain instance of child
        child->retain();

        //handle async
        group_insert_t *data = new group_insert_t();

        data->child = child;
        data->pos = pos;

        group->enqueueValueUpdate(childValue, data, (asyncValueCallback)&AminoGroup::insertChild);
    }

    /**
     * Add a child node.
     */
    void insertChild(AsyncValueUpdate *update, int state) {
        if (state == AsyncValueUpdate::STATE_APPLY) {
            group_insert_t *data = (group_insert_t *)update->data;

            assert(data);

            //keep retained instance

            if (DEBUG_BASE) {
                printf("-> insertChild()\n");
            }

            children.insert(children.begin() + data->pos, data->child);
        } else if (state == AsyncValueUpdate::STATE_DELETE) {
            //on main thread
            group_insert_t *data = (group_insert_t *)update->data;

            assert(data);

            //free
            delete data;
            update->data = NULL;
        }
    }

    /**
     * Remove a child node.
     */
    static NAN_METHOD(Remove) {
        assert(info.Length() == 1);

        AminoGroup *group = Nan::ObjectWrap::Unwrap<AminoGroup>(info.This());
        AminoNode *child = Nan::ObjectWrap::Unwrap<AminoNode>(info[0]->ToObject());

        assert(group);
        assert(child);

        //handle async
        group->enqueueValueUpdate(child, (asyncValueCallback)&AminoGroup::removeChild);
    }

    /**
     * Remove a child node.
     */
    void removeChild(AsyncValueUpdate *update, int state) {
        if (state != AsyncValueUpdate::STATE_APPLY) {
            return;
        }

        if (DEBUG_BASE) {
            printf("-> removeChild()\n");
        }

        AminoNode *node = (AminoNode *)update->valueObj;

        //remove pointer
        std::vector<AminoNode *>::iterator pos = std::find(children.begin(), children.end(), node);

        if (pos != children.end()) {
            children.erase(pos);

            //remove strong reference (on main thread)
            update->releaseLater = node;
        }
    }
};

//font shader

typedef struct {
    float x, y, z;    // position
    float s, t;       // texture pos
} vertex_t;

#endif
