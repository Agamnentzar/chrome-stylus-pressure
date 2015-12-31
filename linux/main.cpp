#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <X11/Xlib.h>
#include <X11/extensions/XInput.h>
#include <X11/Xutil.h>

#define VALUATOR_ABSX     0
#define VALUATOR_ABSY     1
#define VALUATOR_PRESSURE 2
#define VALUATOR_TILTX    3
#define VALUATOR_TILTY    4

#define STR(x) #x
#define XSTR(x) STR(x)

#ifdef DEBUG
#define debug printf
#else
#define debug(body, ...)
#endif

#ifndef VERSION
#define VERSION 0.0.0
#endif

const char *version = XSTR(VERSION);
char buffer[1024];

int screen_width, screen_height;
float absx_min, absx_max;
float absy_min, absy_max;
float pressure_min, pressure_max;
float tilt_min, tilt_max;

int motionType = -1;
int buttonPressType = -1;
int buttonReleaseType = -1;
int proximityInType = -1;
int proximityOutType = -1;

bool isWacom;
bool isEraser;
double pressure;
long posX;
long posY;
float sysX;
float sysY;
long tabX;
long tabY;
float rotationDeg;
float rotationRad;
float tiltX;
float tiltY;
float tangentialPressure;
long pointerType;
char tabletModel[256];
char tabletModelID[256];

void SendData(const char* data)
{
    int len = strlen(data);
    fwrite(&len, 4, 1, stdout);
    fwrite(data, 1, len, stdout);
    fflush(stdout);
}

bool FindDevices(Display *display, XDeviceInfo *stylus_info, XDeviceInfo *eraser_info)
{
    int found = 0;
    int num;
    XDeviceInfo *devices = XListInputDevices(display, &num);

    Atom stylus = XInternAtom(display, "STYLUS", false);
    Atom eraser = XInternAtom(display, "ERASER", false);

    for (int i = 0; i < num; i++) {
        debug("Got device \"%s\"\n", devices[i].name);

        if (devices[i].type == stylus) {
            found++;

            // Fill tablet model here.
            snprintf(tabletModel, sizeof(tabletModel), "%s", devices[i].name);
            snprintf(tabletModelID, sizeof(tabletModelID), "%s", devices[i].name);

            debug("Found stylus.\n");
            memcpy(stylus_info, &devices[i], sizeof(XDeviceInfo));
            XAnyClassPtr any = (XAnyClassPtr)devices[i].inputclassinfo;

            for (int j = 0; j < devices[i].num_classes; j++) {
                if (any->c_class == ValuatorClass) {
                    XValuatorInfoPtr v = (XValuatorInfoPtr)any;

                    absx_min = v->axes[VALUATOR_ABSX].min_value;
                    absx_max = v->axes[VALUATOR_ABSX].max_value;
                    debug("X range: min=%d, max=%d\n", v->axes[VALUATOR_ABSX].min_value, v->axes[VALUATOR_ABSX].max_value);

                    absy_min = v->axes[VALUATOR_ABSY].min_value;
                    absy_max = v->axes[VALUATOR_ABSY].max_value;
                    debug("Y range: min=%d, max=%d\n", v->axes[VALUATOR_ABSY].min_value, v->axes[VALUATOR_ABSY].max_value);

                    pressure_min = v->axes[VALUATOR_PRESSURE].min_value;
                    pressure_max = v->axes[VALUATOR_PRESSURE].max_value;
                    debug("Pressure range: min=%d, max=%d\n", v->axes[VALUATOR_PRESSURE].min_value, v->axes[VALUATOR_PRESSURE].max_value);

                    tilt_min = v->axes[VALUATOR_TILTX].min_value;
                    tilt_max = v->axes[VALUATOR_TILTX].max_value;
                    debug("Tilt range: min=%d, max=%d\n", v->axes[VALUATOR_TILTX].min_value, v->axes[VALUATOR_TILTX].max_value);
                    break;
                }

                any = (XAnyClassPtr)((char*)any + any->length);
            }
        } else if (devices[i].type == eraser) {
            found++;
            debug("Found eraser.\n");
            memcpy(eraser_info, &devices[i], sizeof(XDeviceInfo));
        }
    }

    XFreeDeviceList(devices);

    return found == 2;
}

XDevice *RegisterDeviceEvents(Display *display, XDeviceInfo *deviceInfo)
{
    int num = 0;
    XEventClass eventList[5];
    Window rootWin = RootWindow(display, DefaultScreen(display));
    XDevice *device = XOpenDevice(display, deviceInfo->id);

    assert(device != NULL);

    if (device->num_classes > 0) {
        XInputClassInfo *ip;
        int i = 0;

        for (ip = device->classes; i < deviceInfo->num_classes; ip++, i++) {
            switch (ip->input_class) {
                case KeyClass:
                    break;

                case ButtonClass:
                    DeviceButtonPress(device, buttonPressType, eventList[num]);
                    num++;
                    DeviceButtonRelease(device, buttonReleaseType, eventList[num]);
                    num++;
                    break;

                case ValuatorClass:
                    DeviceMotionNotify(device, motionType, eventList[num]);
                    num++;
                    ProximityIn(device, proximityInType, eventList[num]);
                    num++;
                    ProximityOut(device, proximityOutType, eventList[num]);
                    num++;
                    break;

                default:
                    fprintf(stderr, "Unknown class %d.\n", ip->input_class);
                    break;
            }
        }

        if (XSelectExtensionEvent(display, rootWin, eventList, num)) {
            fprintf(stderr, "Error selecting extended events.\n");
            return NULL;
        }
    }

    return device;
}

void PrintDeviceEvents(Display *display, XDevice *stylus, XDevice *eraser)
{
    XEvent ev;

    // This allows the thread to finish instead of blocking on XNextEvent.
    if (XPending(display) <= 0) {
        usleep(1000);
        return;
    }

    XNextEvent(display, &ev);

    if (ev.type == motionType) {
        XDeviceMotionEvent *motion = (XDeviceMotionEvent*)&ev;

        for (int i = 0; i < motion->axes_count; i++) {
            if (motion->first_axis + i == VALUATOR_ABSX) { // X
                // Tablet position.
                tabX = motion->axis_data[i];
                float absx_range = ((float)absx_min - (float)absx_max);

                // "Low-res" screen position.
                posX = (int)((motion->axis_data[i] / absx_range) * (float)screen_width);

                // "High-res" screen position.
                sysX = ((motion->axis_data[i] / absx_range) * (float)screen_width);
            }

            if (motion->first_axis + i == VALUATOR_ABSY) { // Y
                // Tablet position.
                tabY = sysY = posY = motion->axis_data[i];
                float absy_range = ((float)absy_min - (float)absy_max);

                // "Low-res" screen position.
                posY = (int)((motion->axis_data[i] / absy_range) * (float)screen_height);

                // "High-res" screen position.
                sysY = ((motion->axis_data[i] / absy_range) * (float)screen_height);
            }

            if (motion->first_axis + i == VALUATOR_PRESSURE) {
                pressure = motion->axis_data[i] / ((float)pressure_max - (float)pressure_min);
                snprintf(buffer, sizeof(buffer), "{\"p\":%f}", pressure);
                SendData(buffer);
            }

            if (motion->first_axis + i == VALUATOR_TILTX) // Tilt X
                tiltX = motion->axis_data[i] / ((float)tilt_max - (float)tilt_min);

            if (motion->first_axis + i == VALUATOR_TILTY) // Tilt Y
                tiltY = motion->axis_data[i] / ((float)tilt_max - (float)tilt_min);
        }
    } else if (ev.type == buttonPressType || ev.type == buttonReleaseType) {
        XDeviceButtonEvent *button = (XDeviceButtonEvent*)&ev;

        debug("Button %s event on button %d\n", (ev.type == buttonPressType) ? "press" : "release", button->button);

        for (int i = 0; i < button->axes_count; i++) {
            debug("%d axis = %d, ", button->first_axis + i, button->axis_data[i]);
        }

        debug("\n");
    } else if (ev.type == proximityInType || ev.type == proximityOutType) {
        XProximityNotifyEvent *prox = (XProximityNotifyEvent*)&ev;

        if (prox->type == proximityInType) {
            if (prox->deviceid == stylus->device_id) {
                isEraser = false;
                pointerType = 1; // Pen
            } else {
                isEraser = true;
                pointerType = 3; // Eraser
            } 
        } else if (prox->type == proximityOutType) {
            pointerType = 0; // Out of proximity
        }
    }
}

int main()
{
	snprintf(buffer, sizeof(buffer), "{\"version\":\"%s\"}", version);
	SendData(buffer);
    
    Display *display = XOpenDisplay(NULL);

    assert(display != NULL);

    // TODO: fix for multiple monitors ?
    screen_width = XWidthOfScreen(DefaultScreenOfDisplay(display));
    screen_height = XHeightOfScreen(DefaultScreenOfDisplay(display));

    XDeviceInfo stylus_info, eraser_info;

    while (true) {
        if (FindDevices(display, &stylus_info, &eraser_info)) {
            XDevice *stylus = RegisterDeviceEvents(display, &stylus_info);
            XDevice *eraser = RegisterDeviceEvents(display, &eraser_info);

            assert(stylus != NULL);
            assert(eraser != NULL);

            while (true) { // TODO: check if device is still active
                PrintDeviceEvents(display, stylus, eraser);
            }

            XCloseDevice(display, eraser);
            XCloseDevice(display, stylus);
        } else {
            usleep(1000);
        }
    }

    XCloseDisplay(display);
	return 0;
}
