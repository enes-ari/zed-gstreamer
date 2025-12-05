// /////////////////////////////////////////////////////////////////////////

//
// Copyright (c) 2024, STEREOLABS.
//
// All rights reserved.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// /////////////////////////////////////////////////////////////////////////

#include <gst/base/gstpushsrc.h>
#include <gst/gst.h>
#include <gst/video/video.h>

#include "gstzedsrc.h"

GST_DEBUG_CATEGORY_STATIC(gst_zedsrc_debug);
#define GST_CAT_DEFAULT gst_zedsrc_debug

/* prototypes */
static void gst_zedsrc_set_property(GObject *object, guint property_id, const GValue *value,
                                    GParamSpec *pspec);
static void gst_zedsrc_get_property(GObject *object, guint property_id, GValue *value,
                                    GParamSpec *pspec);
static void gst_zedsrc_dispose(GObject *object);
static void gst_zedsrc_finalize(GObject *object);

static gboolean gst_zedsrc_start(GstBaseSrc *src);
static gboolean gst_zedsrc_stop(GstBaseSrc *src);
static GstCaps *gst_zedsrc_get_caps(GstBaseSrc *src, GstCaps *filter);
static gboolean gst_zedsrc_set_caps(GstBaseSrc *src, GstCaps *caps);
static gboolean gst_zedsrc_unlock(GstBaseSrc *src);
static gboolean gst_zedsrc_unlock_stop(GstBaseSrc *src);

static GstFlowReturn gst_zedsrc_fill(GstPushSrc *src, GstBuffer *buf);

enum {
    PROP_0,
    PROP_CAM_RES,
    PROP_CAM_FPS,
    PROP_STREAM_TYPE,
    PROP_SDK_VERBOSE,
    PROP_CAM_FLIP,
    PROP_CAM_ID,
    PROP_CAM_SN,
    PROP_SVO_FILE,
    PROP_OPENCV_CALIB_FILE,
    PROP_STREAM_IP,
    PROP_STREAM_PORT,
    PROP_DEPTH_MIN,
    PROP_DEPTH_MAX,
    PROP_DEPTH_MODE,
    PROP_DIS_SELF_CALIB,
    PROP_ROI,
    PROP_ROI_X,
    PROP_ROI_Y,
    PROP_ROI_W,
    PROP_ROI_H,
    // PROP_RIGHT_DEPTH_ENABLE,
    PROP_DEPTH_STAB,
    PROP_CONFIDENCE_THRESH,
    PROP_TEXTURE_CONF_THRESH,
    PROP_3D_REF_FRAME,
    PROP_FILL_MODE,
    PROP_COORD_SYS,
    PROP_BRIGHTNESS,
    PROP_CONTRAST,
    PROP_HUE,
    PROP_SATURATION,
    PROP_SHARPNESS,
    PROP_GAMMA,
    PROP_GAIN,
    PROP_EXPOSURE,
    PROP_EXPOSURE_RANGE_MIN,
    PROP_EXPOSURE_RANGE_MAX,
    PROP_AEC_AGC,
    PROP_AEC_AGC_ROI_X,
    PROP_AEC_AGC_ROI_Y,
    PROP_AEC_AGC_ROI_W,
    PROP_AEC_AGC_ROI_H,
    PROP_AEC_AGC_ROI_SIDE,
    PROP_WHITEBALANCE,
    PROP_WHITEBALANCE_AUTO,
    PROP_LEDSTATUS,
    N_PROPERTIES
};

typedef enum {
    GST_ZEDSRC_HD2K = 0, // 2208x1242
    GST_ZEDSRC_HD1080 = 1, // 1920x1080
    GST_ZEDSRC_HD1200 = 2, // 1920x1200
    GST_ZEDSRC_HD720 = 3, // 1280x720
    GST_ZEDSRC_SVGA = 4, // 960x600
    GST_ZEDSRC_VGA = 5, // 672x376
    GST_ZEDSRC_AUTO_RES = 6, // Default value for the camera model
} GstZedSrcRes;

typedef enum {
    GST_ZEDSRC_120FPS = 120,
    GST_ZEDSRC_100FPS = 100,
    GST_ZEDSRC_60FPS = 60,
    GST_ZEDSRC_30FPS = 30,
    GST_ZEDSRC_15FPS = 15
} GstZedSrcFPS;

typedef enum {
    GST_ZEDSRC_NO_FLIP = 0,
    GST_ZEDSRC_FLIP = 1,
    GST_ZEDSRC_AUTO = 2,
} GstZedSrcFlip;

typedef enum {
    GST_ZEDSRC_ONLY_LEFT = 0,
    GST_ZEDSRC_ONLY_RIGHT = 1,
    GST_ZEDSRC_LEFT_RIGHT = 2,
    GST_ZEDSRC_DEPTH_16 = 3,
    GST_ZEDSRC_LEFT_DEPTH = 4
} GstZedSrcStreamType;

typedef enum {
    GST_ZEDSRC_COORD_IMAGE = 0,
    GST_ZEDSRC_COORD_LEFT_HANDED_Y_UP = 1,
    GST_ZEDSRC_COORD_RIGHT_HANDED_Y_UP = 2,
    GST_ZEDSRC_COORD_RIGHT_HANDED_Z_UP = 3,
    GST_ZEDSRC_COORD_LEFT_HANDED_Z_UP = 4,
    GST_ZEDSRC_COORD_RIGHT_HANDED_Z_UP_X_FWD = 5
} GstZedSrcCoordSys;

typedef enum {
    GST_ZEDSRC_SIDE_LEFT = 0,
    GST_ZEDSRC_SIDE_RIGHT = 1,
    GST_ZEDSRC_SIDE_BOTH = 2
} GstZedSrcSide;

//////////////// DEFAULT PARAMETERS
/////////////////////////////////////////////////////////////////////////////

// INITIALIZATION
#define DEFAULT_PROP_CAM_RES        GST_ZEDSRC_AUTO_RES
#define DEFAULT_PROP_CAM_FPS        GST_ZEDSRC_15FPS
#define DEFAULT_PROP_SDK_VERBOSE    0
#define DEFAULT_PROP_CAM_FLIP       2
#define DEFAULT_PROP_CAM_ID         0
#define DEFAULT_PROP_CAM_SN         0
#define DEFAULT_PROP_SVO_FILE       ""
#define DEFAULT_PROP_OPENCV_CALIB_FILE       ""
#define DEFAULT_PROP_STREAM_IP      ""
#define DEFAULT_PROP_STREAM_PORT    30000
#define DEFAULT_PROP_STREAM_TYPE    0
#define DEFAULT_PROP_DEPTH_MIN      300.f
#define DEFAULT_PROP_DEPTH_MAX      20000.f
#define DEFAULT_PROP_DEPTH_MODE     static_cast<gint>(sl::DEPTH_MODE::NONE)
#define DEFAULT_PROP_COORD_SYS      static_cast<gint>(sl::COORDINATE_SYSTEM::IMAGE)
#define DEFAULT_PROP_DIS_SELF_CALIB FALSE
#define DEFAULT_PROP_DEPTH_STAB     1
//#define DEFAULT_PROP_RIGHT_DEPTH              FALSE
#define DEFAULT_PROP_ROI   FALSE
#define DEFAULT_PROP_ROI_X -1
#define DEFAULT_PROP_ROI_Y -1
#define DEFAULT_PROP_ROI_W -1
#define DEFAULT_PROP_ROI_H -1

// RUNTIME
#define DEFAULT_PROP_CONFIDENCE_THRESH   50
#define DEFAULT_PROP_TEXTURE_CONF_THRESH 100
#define DEFAULT_PROP_3D_REF_FRAME        static_cast<gint>(sl::REFERENCE_FRAME::WORLD)
#define DEFAULT_PROP_FILL_MODE           FALSE

// CAMERA CONTROLS
#define DEFAULT_PROP_BRIGHTNESS        4
#define DEFAULT_PROP_CONTRAST          4
#define DEFAULT_PROP_HUE               0
#define DEFAULT_PROP_SATURATION        4
#define DEFAULT_PROP_SHARPNESS         4
#define DEFAULT_PROP_GAMMA             8
#define DEFAULT_PROP_GAIN              60
#define DEFAULT_PROP_EXPOSURE          80
#define DEFAULT_PROP_EXPOSURE_RANGE_MIN 28
#define DEFAULT_PROP_EXPOSURE_RANGE_MAX 66000
#define DEFAULT_PROP_AEG_AGC           1
#define DEFAULT_PROP_AEG_AGC_ROI_X     -1
#define DEFAULT_PROP_AEG_AGC_ROI_Y     -1
#define DEFAULT_PROP_AEG_AGC_ROI_W     -1
#define DEFAULT_PROP_AEG_AGC_ROI_H     -1
#define DEFAULT_PROP_AEG_AGC_ROI_SIDE  GST_ZEDSRC_SIDE_BOTH
#define DEFAULT_PROP_WHITEBALANCE      4600
#define DEFAULT_PROP_WHITEBALANCE_AUTO 1
#define DEFAULT_PROP_LEDSTATUS         1
//////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define GST_TYPE_ZED_SIDE (gst_zedsrc_side_get_type())
static GType gst_zedsrc_side_get_type(void) {
    static GType zedsrc_side_type = 0;

    if (!zedsrc_side_type) {
        static GEnumValue pattern_types[] = {
            {static_cast<gint>(sl::SIDE::LEFT), "Left side only", "LEFT"},
            {static_cast<gint>(sl::SIDE::RIGHT), "Right side only", "RIGHT"},
            {static_cast<gint>(sl::SIDE::BOTH), "Left and Right side", "BOTH"},
            {0, NULL, NULL},
        };

        zedsrc_side_type = g_enum_register_static("GstZedsrcSide", pattern_types);
    }

    return zedsrc_side_type;
}

#define GST_TYPE_ZED_RESOL (gst_zedsrc_resol_get_type())
static GType gst_zedsrc_resol_get_type(void) {
    static GType zedsrc_resol_type = 0;

    if (!zedsrc_resol_type) {
        static GEnumValue pattern_types[] = {
            {GST_ZEDSRC_HD2K, "2208x1242", "HD2K (USB3)"},
            {GST_ZEDSRC_HD1080, "1920x1080", "HD1080 (USB3/GMSL2)"},
            {GST_ZEDSRC_HD1200, "1920x1200", "HD1200 (GMSL2)"},
            {GST_ZEDSRC_HD720, "1280x720", "HD720 (USB3)"},
            {GST_ZEDSRC_SVGA, "960x600", "SVGA (GMSL2)"},
            {GST_ZEDSRC_VGA, "672x376", "VGA (USB3)"},
            {GST_ZEDSRC_AUTO_RES, "Automatic",
             "Default value for the camera model"},
            {0, NULL, NULL},
        };

        zedsrc_resol_type = g_enum_register_static("GstZedSrcRes", pattern_types);
    }

    return zedsrc_resol_type;
}

#define GST_TYPE_ZED_FPS (gst_zedsrc_fps_get_type())
static GType gst_zedsrc_fps_get_type(void) {
    static GType zedsrc_fps_type = 0;

    if (!zedsrc_fps_type) {
        static GEnumValue pattern_types[] = {
            {GST_ZEDSRC_120FPS, "only SVGA (GMSL2) resolution", "120 FPS"},
            {GST_ZEDSRC_100FPS, "only VGA (USB3) resolution", "100 FPS"},
            {GST_ZEDSRC_60FPS, "VGA (USB3), HD720, HD1080 (GMSL2), and HD1200 (GMSL2) resolutions",
             "60  FPS"},
            {GST_ZEDSRC_30FPS, "VGA (USB3), HD720 (USB3) and HD1080 (USB3/GMSL2) resolutions",
             "30  FPS"},
            {GST_ZEDSRC_15FPS, "all resolutions (NO GMSL2)", "15  FPS"},
            {0, NULL, NULL},
        };

        zedsrc_fps_type = g_enum_register_static("GstZedSrcFPS", pattern_types);
    }

    return zedsrc_fps_type;
}

#define GST_TYPE_ZED_FLIP (gst_zedsrc_flip_get_type())
static GType gst_zedsrc_flip_get_type(void) {
    static GType zedsrc_flip_type = 0;

    if (!zedsrc_flip_type) {
        static GEnumValue pattern_types[] = {
            {GST_ZEDSRC_NO_FLIP, "Force no flip", "No Flip"},
            {GST_ZEDSRC_FLIP, "Force flip", "Flip"},
            {GST_ZEDSRC_AUTO, "Auto mode (ZED2/ZED2i/ZED-M only)", "Auto"},
            {0, NULL, NULL},
        };

        zedsrc_flip_type = g_enum_register_static("GstZedSrcFlip", pattern_types);
    }

    return zedsrc_flip_type;
}

#define GST_TYPE_ZED_STREAM_TYPE (gst_zedsrc_stream_type_get_type())
static GType gst_zedsrc_stream_type_get_type(void) {
    static GType zedsrc_stream_type_type = 0;

    if (!zedsrc_stream_type_type) {
        static GEnumValue pattern_types[] = {
            {GST_ZEDSRC_ONLY_LEFT, "8 bits- 4 channels Left image", "Left image [BGRA]"},
            {GST_ZEDSRC_ONLY_RIGHT, "8 bits- 4 channels Right image", "Right image [BGRA]"},
            {GST_ZEDSRC_LEFT_RIGHT, "8 bits- 4 channels bit Left and Right",
             "Stereo couple left/right [BGRA]"},
            {GST_ZEDSRC_DEPTH_16, "16 bits depth", "Depth image [GRAY16_LE]"},
            {GST_ZEDSRC_LEFT_DEPTH, "8 bits- 4 channels Left and Depth(image)",
             "Left and Depth up/down [BGRA]"},
            {0, NULL, NULL},
        };

        zedsrc_stream_type_type = g_enum_register_static("GstZedSrcCoordSys", pattern_types);
    }

    return zedsrc_stream_type_type;
}

#define GST_TYPE_ZED_COORD_SYS (gst_zedsrc_coord_sys_get_type())
static GType gst_zedsrc_coord_sys_get_type(void) {
    static GType zedsrc_coord_sys_type = 0;

    if (!zedsrc_coord_sys_type) {
        static GEnumValue pattern_types[] = {
            {GST_ZEDSRC_COORD_IMAGE,
             "Standard coordinates system in computer vision. Used in OpenCV.", "Image"},
            {GST_ZEDSRC_COORD_LEFT_HANDED_Y_UP,
             "Left-Handed with Y up and Z forward. Used in Unity with DirectX.",
             "Left handed, Y up"},
            {GST_ZEDSRC_COORD_RIGHT_HANDED_Y_UP,
             "Right-Handed with Y pointing up and Z backward. Used in OpenGL.",
             "Right handed, Y up"},
            {GST_ZEDSRC_COORD_RIGHT_HANDED_Z_UP,
             "Right-Handed with Z pointing up and Y forward. Used in 3DSMax.",
             "Right handed, Z up"},
            {GST_ZEDSRC_COORD_LEFT_HANDED_Z_UP,
             "Left-Handed with Z axis pointing up and X forward. Used in Unreal Engine.",
             "Left handed, Z up"},
            {GST_ZEDSRC_COORD_RIGHT_HANDED_Z_UP_X_FWD,
             "Right-Handed with Z pointing up and X forward. Used in ROS (REP 103).",
             "Right handed, Z up, X fwd"},
            {0, NULL, NULL},
        };

        zedsrc_coord_sys_type = g_enum_register_static("GstZedsrcStreamType", pattern_types);
    }

    return zedsrc_coord_sys_type;
}

#define GST_TYPE_ZED_DEPTH_MODE (gst_zedsrc_depth_mode_get_type())
static GType gst_zedsrc_depth_mode_get_type(void) {
    static GType zedsrc_depth_mode_type = 0;

    if (!zedsrc_depth_mode_type) {
        static GEnumValue pattern_types[] = {
            {static_cast<gint>(sl::DEPTH_MODE::NEURAL_PLUS),
             "More accurate Neural disparity estimation, Requires AI module.", "NEURAL_PLUS"},
            {static_cast<gint>(sl::DEPTH_MODE::NEURAL),
             "End to End Neural disparity estimation, requires AI module", "NEURAL"},
            {static_cast<gint>(sl::DEPTH_MODE::ULTRA),
             "Computation mode favorising edges and sharpness. Requires more GPU memory and "
             "computation power.",
             "ULTRA"},
            {static_cast<gint>(sl::DEPTH_MODE::QUALITY),
             "Computation mode designed for challenging areas with untextured surfaces.",
             "QUALITY"},
            {static_cast<gint>(sl::DEPTH_MODE::PERFORMANCE),
             "Computation mode optimized for speed.", "PERFORMANCE"},
            {static_cast<gint>(sl::DEPTH_MODE::NONE),
             "This mode does not compute any depth map. Only rectified stereo images will be "
             "available.",
             "NONE"},
            {0, NULL, NULL},
        };

        zedsrc_depth_mode_type = g_enum_register_static("GstZedsrcDepthMode", pattern_types);
    }

    return zedsrc_depth_mode_type;
}

#define GST_TYPE_ZED_3D_REF_FRAME (gst_zedsrc_3d_meas_ref_frame_get_type())
static GType gst_zedsrc_3d_meas_ref_frame_get_type(void) {
    static GType zedsrc_3d_meas_ref_frame_type = 0;

    if (!zedsrc_3d_meas_ref_frame_type) {
        static GEnumValue pattern_types[] = {
            {static_cast<gint>(sl::REFERENCE_FRAME::WORLD),
             "The pose transform will contain the motion with reference to "
             "the world frame.",
             "WORLD"},
            {static_cast<gint>(sl::REFERENCE_FRAME::CAMERA),
             "The  pose transform will contain the motion with reference to the previous camera "
             "frame.",
             "CAMERA"},
            {0, NULL, NULL},
        };

        zedsrc_3d_meas_ref_frame_type =
            g_enum_register_static("GstZedsrc3dMeasRefFrame", pattern_types);
    }

    return zedsrc_3d_meas_ref_frame_type;
}

/* pad templates */
static GstStaticPadTemplate gst_zedsrc_src_template =
    GST_STATIC_PAD_TEMPLATE("src", GST_PAD_SRC, GST_PAD_ALWAYS,
                            GST_STATIC_CAPS(("video/x-raw, "   // Double stream VGA
                                             "format = (string)BGRA, "
                                             "width = (int)1344, "
                                             "height = (int)376 , "
                                             "framerate = (fraction) { 15, 30, 60, 100 }"
                                             ";"
                                             "video/x-raw, "   // Double stream HD720
                                             "format = (string)BGRA, "
                                             "width = (int)2560, "
                                             "height = (int)720, "
                                             "framerate = (fraction) { 15, 30, 60 }"
                                             ";"
                                             "video/x-raw, "   // Double stream HD1080
                                             "format = (string)BGRA, "
                                             "width = (int)3840, "
                                             "height = (int)1080, "
                                             "framerate = (fraction) { 15, 30, 60 }"
                                             ";"
                                             "video/x-raw, "   // Double stream HD2K
                                             "format = (string)BGRA, "
                                             "width = (int)4416, "
                                             "height = (int)1242, "
                                             "framerate = (fraction)15"
                                             ";"
                                             "video/x-raw, "   // Double stream HD1200 (GMSL2)
                                             "format = (string)BGRA, "
                                             "width = (int)3840, "
                                             "height = (int)1200, "
                                             "framerate = (fraction) { 15, 30, 60 }"
                                             ";"
                                             "video/x-raw, "   // Double stream SVGA (GMSL2)
                                             "format = (string)BGRA, "
                                             "width = (int)1920, "
                                             "height = (int)600, "
                                             "framerate = (fraction) { 15, 30, 60, 120 }"
                                             ";"
                                             "video/x-raw, "   // Color VGA
                                             "format = (string)BGRA, "
                                             "width = (int)672, "
                                             "height =  (int)376, "
                                             "framerate = (fraction) { 15, 30, 60, 100 }"
                                             ";"
                                             "video/x-raw, "   // Color HD720
                                             "format = (string)BGRA, "
                                             "width = (int)1280, "
                                             "height =  (int)720, "
                                             "framerate =  (fraction)  { 15, 30, 60}"
                                             ";"
                                             "video/x-raw, "   // Color HD1080
                                             "format = (string)BGRA, "
                                             "width = (int)1920, "
                                             "height = (int)1080, "
                                             "framerate = (fraction) { 15, 30, 60 }"
                                             ";"
                                             "video/x-raw, "   // Color HD2K
                                             "format = (string)BGRA, "
                                             "width = (int)2208, "
                                             "height = (int)1242, "
                                             "framerate = (fraction)15"
                                             ";"
                                             "video/x-raw, "   // Color HD1200 (GMSL2)
                                             "format = (string)BGRA, "
                                             "width = (int)1920, "
                                             "height = (int)1200, "
                                             "framerate = (fraction) { 15, 30, 60 }"
                                             ";"
                                             "video/x-raw, "   // Color SVGA (GMSL2)
                                             "format = (string)BGRA, "
                                             "width = (int)960, "
                                             "height = (int)600, "
                                             "framerate = (fraction) { 15, 30, 60, 120 }"
                                             ";"
                                             "video/x-raw, "   // Depth VGA
                                             "format = (string)GRAY16_LE, "
                                             "width = (int)672, "
                                             "height =  (int)376, "
                                             "framerate = (fraction) { 15, 30, 60, 100 }"
                                             ";"
                                             "video/x-raw, "   // Depth HD720
                                             "format = (string)GRAY16_LE, "
                                             "width = (int)1280, "
                                             "height =  (int)720, "
                                             "framerate =  (fraction)  { 15, 30, 60}"
                                             ";"
                                             "video/x-raw, "   // Depth HD1080
                                             "format = (string)GRAY16_LE, "
                                             "width = (int)1920, "
                                             "height = (int)1080, "
                                             "framerate = (fraction) { 15, 30, 60 }"
                                             ";"
                                             "video/x-raw, "   // Depth HD2K
                                             "format = (string)GRAY16_LE, "
                                             "width = (int)2208, "
                                             "height = (int)1242, "
                                             "framerate = (fraction)15"
                                             ";"
                                             "video/x-raw, "   // Depth HD1200 (GMSL2)
                                             "format = (string)GRAY16_LE, "
                                             "width = (int)1920, "
                                             "height = (int)1200, "
                                             "framerate = (fraction) { 15, 30, 60 }"
                                             ";"
                                             "video/x-raw, "   // Depth SVGA (GMSL2)
                                             "format = (string)GRAY16_LE, "
                                             "width = (int)960, "
                                             "height = (int)600, "
                                             "framerate = (fraction) { 15, 30, 60, 120 }")));

/* class initialization */
G_DEFINE_TYPE(GstZedSrc, gst_zedsrc, GST_TYPE_PUSH_SRC);

static void gst_zedsrc_class_init(GstZedSrcClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GstElementClass *gstelement_class = GST_ELEMENT_CLASS(klass);
    GstBaseSrcClass *gstbasesrc_class = GST_BASE_SRC_CLASS(klass);
    GstPushSrcClass *gstpushsrc_class = GST_PUSH_SRC_CLASS(klass);

    gobject_class->set_property = gst_zedsrc_set_property;
    gobject_class->get_property = gst_zedsrc_get_property;
    gobject_class->dispose = gst_zedsrc_dispose;
    gobject_class->finalize = gst_zedsrc_finalize;

    gst_element_class_add_pad_template(gstelement_class,
                                       gst_static_pad_template_get(&gst_zedsrc_src_template));

    gst_element_class_set_static_metadata(gstelement_class, "ZED Camera Source", "Source/Video",
                                          "Stereolabs ZED Camera source",
                                          "Stereolabs <support@stereolabs.com>");

    gstbasesrc_class->start = GST_DEBUG_FUNCPTR(gst_zedsrc_start);
    gstbasesrc_class->stop = GST_DEBUG_FUNCPTR(gst_zedsrc_stop);
    gstbasesrc_class->get_caps = GST_DEBUG_FUNCPTR(gst_zedsrc_get_caps);
    gstbasesrc_class->set_caps = GST_DEBUG_FUNCPTR(gst_zedsrc_set_caps);
    gstbasesrc_class->unlock = GST_DEBUG_FUNCPTR(gst_zedsrc_unlock);
    gstbasesrc_class->unlock_stop = GST_DEBUG_FUNCPTR(gst_zedsrc_unlock_stop);

    gstpushsrc_class->fill = GST_DEBUG_FUNCPTR(gst_zedsrc_fill);

    /* Install GObject properties */
    g_object_class_install_property(
        gobject_class, PROP_CAM_RES,
        g_param_spec_enum("camera-resolution", "Camera Resolution", "Camera Resolution",
                          GST_TYPE_ZED_RESOL, DEFAULT_PROP_CAM_RES,
                          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(
        gobject_class, PROP_CAM_FPS,
        g_param_spec_enum("camera-fps", "Camera frame rate", "Camera frame rate", GST_TYPE_ZED_FPS,
                          DEFAULT_PROP_CAM_FPS,
                          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(
        gobject_class, PROP_STREAM_TYPE,
        g_param_spec_enum("stream-type", "Image stream type", "Image stream type",
                          GST_TYPE_ZED_STREAM_TYPE, DEFAULT_PROP_STREAM_TYPE,
                          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(
        gobject_class, PROP_SDK_VERBOSE,
        g_param_spec_int("sdk-verbose", "ZED SDK Verbose", "ZED SDK Verbose level", 0, 1000,
                         DEFAULT_PROP_SDK_VERBOSE,
                         (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(
        gobject_class, PROP_CAM_FLIP,
        g_param_spec_enum("camera-image-flip", "Camera image flip",
                          "Use the camera in forced flip/no flip or automatic mode",
                          GST_TYPE_ZED_FLIP, DEFAULT_PROP_CAM_FLIP,
                          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(
        gobject_class, PROP_CAM_ID,
        g_param_spec_int("camera-id", "Camera ID", "Select camera from cameraID", 0, 255,
                         DEFAULT_PROP_CAM_ID,
                         (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(
        gobject_class, PROP_CAM_SN,
        g_param_spec_int64("camera-sn", "Camera Serial Number",
                           "Select camera from camera serial number", 0, G_MAXINT64,
                           DEFAULT_PROP_CAM_SN,
                           (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(
        gobject_class, PROP_SVO_FILE,
        g_param_spec_string("svo-file-path", "SVO file", "Input from SVO file",
                            DEFAULT_PROP_SVO_FILE,
                            (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(
        gobject_class, PROP_OPENCV_CALIB_FILE,
        g_param_spec_string("opencv-calibration-file", "Optional OpenCV Calibration File", "Optional OpenCV Calibration File",
                            DEFAULT_PROP_OPENCV_CALIB_FILE,
                            (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(
        gobject_class, PROP_STREAM_IP,
        g_param_spec_string("input-stream-ip", "Input Stream IP",
                            "Specify IP adress when using streaming input", DEFAULT_PROP_SVO_FILE,
                            (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(
        gobject_class, PROP_STREAM_PORT,
        g_param_spec_int("input-stream-port", "Input Stream Port",
                         "Specify port when using streaming input", 1, G_MAXUINT16,
                         DEFAULT_PROP_STREAM_PORT,
                         (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(
        gobject_class, PROP_DEPTH_MIN,
        g_param_spec_float("depth-minimum-distance", "Minimum depth value", "Minimum depth value",
                           100.f, 3000.f, DEFAULT_PROP_DEPTH_MIN,
                           (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(
        gobject_class, PROP_DEPTH_MAX,
        g_param_spec_float("depth-maximum-distance", "Maximum depth value", "Maximum depth value",
                           500.f, 40000.f, DEFAULT_PROP_DEPTH_MAX,
                           (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(
        gobject_class, PROP_DEPTH_MODE,
        g_param_spec_enum("depth-mode", "Depth Mode", "Depth Mode", GST_TYPE_ZED_DEPTH_MODE,
                          DEFAULT_PROP_DEPTH_MODE,
                          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(
        gobject_class, PROP_DIS_SELF_CALIB,
        g_param_spec_boolean("camera-disable-self-calib", "Disable self calibration",
                             "Disable the self calibration processing when the camera is opened",
                             DEFAULT_PROP_DIS_SELF_CALIB,
                             (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    /*g_object_class_install_property( gobject_class, PROP_RIGHT_DEPTH_ENABLE,
                                     g_param_spec_boolean("enable-right-side-measure", "Enable right
       side measure", "Enable the MEASURE::DEPTH_RIGHT and other MEASURE::<XXX>_RIGHT at the cost of
       additional computation time", DEFAULT_PROP_RIGHT_DEPTH, (GParamFlags) (G_PARAM_READWRITE |
       G_PARAM_STATIC_STRINGS)));*/

    g_object_class_install_property(
        gobject_class, PROP_DEPTH_STAB,
        g_param_spec_int("depth-stabilization", "Depth stabilization", "Enable depth stabilization",
                         0, 100, DEFAULT_PROP_DEPTH_STAB,
                         (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(
        gobject_class, PROP_COORD_SYS,
        g_param_spec_enum("coordinate-system", "SDK Coordinate System", "3D Coordinate System",
                          GST_TYPE_ZED_COORD_SYS, DEFAULT_PROP_COORD_SYS,
                          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(
        gobject_class, PROP_ROI,
        g_param_spec_boolean("roi", "Region of interest", "Enable region of interest filtering",
                             DEFAULT_PROP_ROI,
                             (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(
        gobject_class, PROP_ROI_X,
        g_param_spec_int("roi-x", "Region of interest top left 'X' coordinate",
                         "Region of interest top left 'X' coordinate (-1 to not set ROI)", -1, 2208,
                         DEFAULT_PROP_ROI_X,
                         (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(
        gobject_class, PROP_ROI_Y,
        g_param_spec_int("roi-y", "Region of interest top left 'Y' coordinate",
                         "Region of interest top left 'Y' coordinate (-1 to not set ROI)", -1, 1242,
                         DEFAULT_PROP_ROI_Y,
                         (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(
        gobject_class, PROP_ROI_W,
        g_param_spec_int("roi-w", "Region of interest width",
                         "Region of intererst width (-1 to not set ROI)", -1, 2208,
                         DEFAULT_PROP_ROI_W,
                         (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(
        gobject_class, PROP_ROI_H,
        g_param_spec_int("roi-h", "Region of interest height",
                         "Region of interest height (-1 to not set ROI)", -1, 1242,
                         DEFAULT_PROP_ROI_H,
                         (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(
        gobject_class, PROP_CONFIDENCE_THRESH,
        g_param_spec_int("confidence-threshold", "Depth Confidence Threshold",
                         "Specify the Depth Confidence Threshold", 0, 100,
                         DEFAULT_PROP_CONFIDENCE_THRESH,
                         (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(
        gobject_class, PROP_TEXTURE_CONF_THRESH,
        g_param_spec_int("texture-confidence-threshold", "Texture Confidence Threshold",
                         "Specify the Texture Confidence Threshold", 0, 100,
                         DEFAULT_PROP_TEXTURE_CONF_THRESH,
                         (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(
        gobject_class, PROP_3D_REF_FRAME,
        g_param_spec_enum("measure3D-reference-frame", "3D Measures Reference Frame",
                          "Specify the 3D Reference Frame", GST_TYPE_ZED_3D_REF_FRAME,
                          DEFAULT_PROP_3D_REF_FRAME,
                          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(
        gobject_class, PROP_FILL_MODE,
        g_param_spec_boolean("fill-mode", "Depth Fill Mode", "Specify the Depth Fill Mode",
                             DEFAULT_PROP_FILL_MODE,
                             (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(
        gobject_class, PROP_BRIGHTNESS,
        g_param_spec_int("ctrl-brightness", "Camera control: brightness", "Image brightness", 0, 8,
                         DEFAULT_PROP_BRIGHTNESS,
                         (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property(
        gobject_class, PROP_CONTRAST,
        g_param_spec_int("ctrl-contrast", "Camera control: contrast", "Image contrast", 0, 8,
                         DEFAULT_PROP_CONTRAST,
                         (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property(
        gobject_class, PROP_HUE,
        g_param_spec_int("ctrl-hue", "Camera control: hue", "Image hue", 0, 11, DEFAULT_PROP_HUE,
                         (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property(
        gobject_class, PROP_SATURATION,
        g_param_spec_int("ctrl-saturation", "Camera control: saturation", "Image saturation", 0, 8,
                         DEFAULT_PROP_SATURATION,
                         (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property(
        gobject_class, PROP_SHARPNESS,
        g_param_spec_int("ctrl-sharpness", "Camera control: sharpness", "Image sharpness", 0, 8,
                         DEFAULT_PROP_SHARPNESS,
                         (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property(
        gobject_class, PROP_GAMMA,
        g_param_spec_int("ctrl-gamma", "Camera control: gamma", "Image gamma", 1, 9, DEFAULT_PROP_GAMMA,
                         (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property(
        gobject_class, PROP_GAIN,
        g_param_spec_int("ctrl-gain", "Camera control: gain", "Camera gain", 0, 100, DEFAULT_PROP_GAIN,
                         (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property(
        gobject_class, PROP_EXPOSURE,
        g_param_spec_int("ctrl-exposure", "Camera control: exposure", "Camera exposure", 0, 100,
                         DEFAULT_PROP_EXPOSURE,
                         (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property(
        gobject_class, PROP_EXPOSURE_RANGE_MIN,
        g_param_spec_int("ctrl-exposure-range-min", "Minimum Exposure time [µsec]",
                         "Minimum exposure time in microseconds for the automatic exposure setting",
                         28, 66000, DEFAULT_PROP_EXPOSURE_RANGE_MIN,
                         (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property(
        gobject_class, PROP_EXPOSURE_RANGE_MAX,
        g_param_spec_int("ctrl-exposure-range-max", "Maximum Exposure time [µsec]",
                         "Maximum exposure time in microseconds for the automatic exposure setting",
                         28, 66000, DEFAULT_PROP_EXPOSURE_RANGE_MAX,
                         (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property(
        gobject_class, PROP_AEC_AGC,
        g_param_spec_boolean("ctrl-aec-agc", "Camera control: automatic gain and exposure",
                             "Camera automatic gain and exposure", DEFAULT_PROP_AEG_AGC,
                             (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property(
        gobject_class, PROP_AEC_AGC_ROI_X,
        g_param_spec_int("ctrl-aec-agc-roi-x",
                         "Camera control: auto gain/exposure ROI top left 'X' coordinate",
                         "Auto gain/exposure ROI top left 'X' coordinate (-1 to not set ROI)", -1,
                         2208, DEFAULT_PROP_AEG_AGC_ROI_X,
                         (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property(
        gobject_class, PROP_AEC_AGC_ROI_Y,
        g_param_spec_int("ctrl-aec-agc-roi-y",
                         "Camera control: auto gain/exposure ROI top left 'Y' coordinate",
                         "Auto gain/exposure ROI top left 'Y' coordinate (-1 to not set ROI)", -1,
                         1242, DEFAULT_PROP_AEG_AGC_ROI_Y,
                         (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property(
        gobject_class, PROP_AEC_AGC_ROI_W,
        g_param_spec_int("ctrl-aec-agc-roi-w", "Camera control: auto gain/exposure ROI width",
                         "Auto gain/exposure ROI width (-1 to not set ROI)", -1, 2208,
                         DEFAULT_PROP_AEG_AGC_ROI_W,
                         (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property(
        gobject_class, PROP_AEC_AGC_ROI_H,
        g_param_spec_int("ctrl-aec-agc-roi-h", "Camera control: auto gain/exposure ROI height",
                         "Auto gain/exposure ROI height (-1 to not set ROI)", -1, 1242,
                         DEFAULT_PROP_AEG_AGC_ROI_H,
                         (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property(
        gobject_class, PROP_AEC_AGC_ROI_SIDE,
        g_param_spec_enum("ctrl-aec-agc-roi-side", "Camera control: auto gain/exposure ROI side",
                          "Auto gain/exposure ROI side", GST_TYPE_ZED_SIDE,
                          DEFAULT_PROP_AEG_AGC_ROI_SIDE,
                          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property(
        gobject_class, PROP_WHITEBALANCE,
        g_param_spec_int("ctrl-whitebalance-temperature", "Camera control: white balance temperature",
                         "Image white balance temperature", 2800, 6500, DEFAULT_PROP_WHITEBALANCE,
                         (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property(
        gobject_class, PROP_WHITEBALANCE_AUTO,
        g_param_spec_boolean("ctrl-whitebalance-auto", "Camera control: automatic whitebalance",
                             "Image automatic white balance", DEFAULT_PROP_WHITEBALANCE_AUTO,
                             (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property(
        gobject_class, PROP_LEDSTATUS,
        g_param_spec_boolean("ctrl-led-status", "Camera control: led status", "Camera LED on/off",
                             DEFAULT_PROP_LEDSTATUS,
                             (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}

static void gst_zedsrc_reset(GstZedSrc *src) {
    if (src->zed.isOpened()) {
        src->zed.close();
    }

    src->out_framesize = 0;
    src->is_started = FALSE;

    src->last_frame_count = 0;
    src->total_dropped_frames = 0;

    if (src->caps) {
        gst_caps_unref(src->caps);
        src->caps = NULL;
    }
}

static void gst_zedsrc_init(GstZedSrc *src) {
    /* set source as live (no preroll) */
    gst_base_src_set_live(GST_BASE_SRC(src), TRUE);

    /* override default of BYTES to operate in time mode */
    gst_base_src_set_format(GST_BASE_SRC(src), GST_FORMAT_TIME);

    // ----> Parameters initialization
    src->camera_resolution = DEFAULT_PROP_CAM_RES;
    src->camera_fps = DEFAULT_PROP_CAM_FPS;
    src->sdk_verbose = DEFAULT_PROP_SDK_VERBOSE;
    src->camera_image_flip = DEFAULT_PROP_CAM_FLIP;
    src->camera_id = DEFAULT_PROP_CAM_ID;
    src->camera_sn = DEFAULT_PROP_CAM_SN;
    src->svo_file = *g_string_new(DEFAULT_PROP_SVO_FILE);
    src->opencv_calibration_file = *g_string_new(DEFAULT_PROP_OPENCV_CALIB_FILE);
    src->stream_ip = *g_string_new(DEFAULT_PROP_STREAM_IP);

    src->stream_port = DEFAULT_PROP_STREAM_PORT;
    src->stream_type = DEFAULT_PROP_STREAM_TYPE;

    src->depth_min_dist = DEFAULT_PROP_DEPTH_MIN;
    src->depth_max_dist = DEFAULT_PROP_DEPTH_MAX;
    src->depth_mode = DEFAULT_PROP_DEPTH_MODE;
    src->camera_disable_self_calib = DEFAULT_PROP_DIS_SELF_CALIB;
    src->depth_stabilization = DEFAULT_PROP_DEPTH_STAB;
    src->coord_sys = DEFAULT_PROP_COORD_SYS;
    src->confidence_threshold = DEFAULT_PROP_CONFIDENCE_THRESH;
    src->texture_confidence_threshold = DEFAULT_PROP_TEXTURE_CONF_THRESH;
    src->measure3D_reference_frame = DEFAULT_PROP_3D_REF_FRAME;
    src->fill_mode = DEFAULT_PROP_FILL_MODE;
    src->roi = DEFAULT_PROP_ROI;
    src->roi_x = DEFAULT_PROP_ROI_X;
    src->roi_y = DEFAULT_PROP_ROI_Y;
    src->roi_w = DEFAULT_PROP_ROI_W;
    src->roi_h = DEFAULT_PROP_ROI_H;

    src->brightness = DEFAULT_PROP_BRIGHTNESS;
    src->contrast = DEFAULT_PROP_CONTRAST;
    src->hue = DEFAULT_PROP_HUE;
    src->saturation = DEFAULT_PROP_SATURATION;
    src->sharpness = DEFAULT_PROP_SHARPNESS;
    src->gamma = DEFAULT_PROP_GAMMA;
    src->gain = DEFAULT_PROP_GAIN;
    src->exposure = DEFAULT_PROP_EXPOSURE;
    src->exposureRange_min = DEFAULT_PROP_EXPOSURE_RANGE_MIN;
    src->exposureRange_max = DEFAULT_PROP_EXPOSURE_RANGE_MAX;
    src->aec_agc = DEFAULT_PROP_AEG_AGC;
    src->aec_agc_roi_x = DEFAULT_PROP_AEG_AGC_ROI_X;
    src->aec_agc_roi_y = DEFAULT_PROP_AEG_AGC_ROI_Y;
    src->aec_agc_roi_w = DEFAULT_PROP_AEG_AGC_ROI_W;
    src->aec_agc_roi_h = DEFAULT_PROP_AEG_AGC_ROI_H;
    src->aec_agc_roi_side = DEFAULT_PROP_AEG_AGC_ROI_SIDE;
    src->whitebalance_temperature = DEFAULT_PROP_WHITEBALANCE;
    src->whitebalance_temperature_auto = DEFAULT_PROP_WHITEBALANCE_AUTO;
    src->led_status = DEFAULT_PROP_LEDSTATUS;
    // <---- Parameters initialization

    src->stop_requested = FALSE;
    src->caps = NULL;

    gst_zedsrc_reset(src);
}

void gst_zedsrc_set_property(GObject *object, guint property_id, const GValue *value,
                             GParamSpec *pspec) {
    GstZedSrc *src;
    const gchar *str;

    src = GST_ZED_SRC(object);

    GST_TRACE_OBJECT(src, "gst_zedsrc_set_property");

    switch (property_id) {
    case PROP_CAM_RES:
        src->camera_resolution = g_value_get_enum(value);
        break;
    case PROP_CAM_FPS:
        src->camera_fps = g_value_get_enum(value);
        break;
    case PROP_SDK_VERBOSE:
        src->sdk_verbose = g_value_get_int(value);
        break;
    case PROP_CAM_FLIP:
        src->camera_image_flip = g_value_get_enum(value);
        break;
    case PROP_CAM_ID:
        src->camera_id = g_value_get_int(value);
        break;
    case PROP_CAM_SN:
        src->camera_sn = g_value_get_int64(value);
        break;
    case PROP_SVO_FILE:
        str = g_value_get_string(value);
        src->svo_file = *g_string_new(str);
        break;
    case PROP_OPENCV_CALIB_FILE:
        str = g_value_get_string(value);
        src->opencv_calibration_file = *g_string_new(str);
        break;
    case PROP_STREAM_IP:
        str = g_value_get_string(value);
        src->stream_ip = *g_string_new(str);
        break;
    case PROP_STREAM_PORT:
        src->stream_port = g_value_get_int(value);
        break;
    case PROP_STREAM_TYPE:
        src->stream_type = g_value_get_enum(value);
        break;
    case PROP_DEPTH_MIN:
        src->depth_min_dist = g_value_get_float(value);
        break;
    case PROP_DEPTH_MAX:
        src->depth_max_dist = g_value_get_float(value);
        break;
    case PROP_DEPTH_MODE:
        src->depth_mode = g_value_get_enum(value);
        break;
    case PROP_DIS_SELF_CALIB:
        src->camera_disable_self_calib = g_value_get_boolean(value);
        break;
    case PROP_DEPTH_STAB:
        src->depth_stabilization = g_value_get_int(value);
        break;
    case PROP_COORD_SYS:
        src->coord_sys = g_value_get_enum(value);
        break;
        /*case PROP_RIGHT_DEPTH_ENABLE:
        src->enable_right_side_measure =  g_value_get_boolean(value);
        break;*/
    case PROP_CONFIDENCE_THRESH:
        src->confidence_threshold = g_value_get_int(value);
        break;
    case PROP_TEXTURE_CONF_THRESH:
        src->texture_confidence_threshold = g_value_get_int(value);
        break;
    case PROP_3D_REF_FRAME:
        src->measure3D_reference_frame = g_value_get_enum(value);
        break;
    case PROP_FILL_MODE:
        src->fill_mode = g_value_get_boolean(value);
        break;
    case PROP_ROI:
        src->roi = g_value_get_boolean(value);
        break;
    case PROP_ROI_X:
        src->roi_x = g_value_get_int(value);
        break;
    case PROP_ROI_Y:
        src->roi_y = g_value_get_int(value);
        break;
    case PROP_ROI_W:
        src->roi_w = g_value_get_int(value);
        break;
    case PROP_ROI_H:
        src->roi_h = g_value_get_int(value);
        break;
    case PROP_BRIGHTNESS:
        src->brightness = g_value_get_int(value);
        break;
    case PROP_CONTRAST:
        src->contrast = g_value_get_int(value);
        break;
    case PROP_HUE:
        src->hue = g_value_get_int(value);
        break;
    case PROP_SATURATION:
        src->saturation = g_value_get_int(value);
        break;
    case PROP_SHARPNESS:
        src->sharpness = g_value_get_int(value);
        break;
    case PROP_GAMMA:
        src->gamma = g_value_get_int(value);
        break;
    case PROP_GAIN:
        src->gain = g_value_get_int(value);
        src->exposure_gain_updated = TRUE;
        break;
    case PROP_EXPOSURE:
        src->exposure = g_value_get_int(value);
        src->exposure_gain_updated = TRUE;
        break;
    case PROP_EXPOSURE_RANGE_MIN:
        src->exposureRange_min = g_value_get_int(value);
        break;
    case PROP_EXPOSURE_RANGE_MAX:
        src->exposureRange_max = g_value_get_int(value);
        break;
    case PROP_AEC_AGC:
        src->aec_agc = g_value_get_boolean(value);
        src->exposure_gain_updated = TRUE;
        break;
    case PROP_AEC_AGC_ROI_X:
        src->aec_agc_roi_x = g_value_get_int(value);
        src->exposure_gain_updated = TRUE;
        break;
    case PROP_AEC_AGC_ROI_Y:
        src->aec_agc_roi_y = g_value_get_int(value);
        src->exposure_gain_updated = TRUE;
        break;
    case PROP_AEC_AGC_ROI_W:
        src->aec_agc_roi_w = g_value_get_int(value);
        src->exposure_gain_updated = TRUE;
        break;
    case PROP_AEC_AGC_ROI_H:
        src->aec_agc_roi_h = g_value_get_int(value);
        src->exposure_gain_updated = TRUE;
        break;
    case PROP_AEC_AGC_ROI_SIDE:
        src->aec_agc_roi_side = g_value_get_enum(value);
        src->exposure_gain_updated = TRUE;
        break;
    case PROP_WHITEBALANCE:
        src->whitebalance_temperature = g_value_get_int(value);
        break;
    case PROP_WHITEBALANCE_AUTO:
        src->whitebalance_temperature_auto = g_value_get_boolean(value);
        break;
    case PROP_LEDSTATUS:
        src->led_status = g_value_get_boolean(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

void gst_zedsrc_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec) {
    GstZedSrc *src;

    g_return_if_fail(GST_IS_ZED_SRC(object));
    src = GST_ZED_SRC(object);

    GST_TRACE_OBJECT(src, "gst_zedsrc_get_property");

    switch (property_id) {
    case PROP_CAM_RES:
        g_value_set_enum(value, src->camera_resolution);
        break;
    case PROP_CAM_FPS:
        g_value_set_enum(value, src->camera_fps);
        break;
    case PROP_SDK_VERBOSE:
        g_value_set_int(value, src->sdk_verbose);
        break;
    case PROP_CAM_FLIP:
        g_value_set_enum(value, src->camera_image_flip);
        break;
    case PROP_CAM_ID:
        g_value_set_int(value, src->camera_id);
        break;
    case PROP_CAM_SN:
        g_value_set_int64(value, src->camera_id);
        break;
    case PROP_SVO_FILE:
        g_value_set_string(value, src->svo_file.str);
        break;
    case PROP_OPENCV_CALIB_FILE:
        g_value_set_string(value, src->opencv_calibration_file.str);
        break;
    case PROP_STREAM_IP:
        g_value_set_string(value, src->stream_ip.str);
        break;
    case PROP_STREAM_PORT:
        g_value_set_int(value, src->stream_port);
        break;
    case PROP_STREAM_TYPE:
        g_value_set_enum(value, src->stream_type);
        break;
    case PROP_DEPTH_MIN:
        g_value_set_float(value, src->depth_min_dist);
        break;
    case PROP_DEPTH_MAX:
        g_value_set_float(value, src->depth_max_dist);
        break;
    case PROP_DEPTH_MODE:
        g_value_set_enum(value, src->depth_mode);
        break;
    case PROP_COORD_SYS:
        g_value_set_enum(value, src->coord_sys);
        break;
    case PROP_DIS_SELF_CALIB:
        g_value_set_boolean(value, src->camera_disable_self_calib);
        break;
        /*case PROP_RIGHT_DEPTH_ENABLE:
        g_value_set_boolean( value, src->enable_right_side_measure);
        break;*/
    case PROP_DEPTH_STAB:
        g_value_set_int(value, src->depth_stabilization);
        break;
    case PROP_CONFIDENCE_THRESH:
        g_value_set_int(value, src->confidence_threshold);
        break;
    case PROP_TEXTURE_CONF_THRESH:
        g_value_set_int(value, src->texture_confidence_threshold);
        break;
    case PROP_3D_REF_FRAME:
        g_value_set_enum(value, src->measure3D_reference_frame);
        break;
    case PROP_FILL_MODE:
        g_value_set_boolean(value, src->fill_mode);
        break;
    case PROP_ROI:
        g_value_set_boolean(value, src->roi);
        break;
    case PROP_ROI_X:
        g_value_set_int(value, src->roi_x);
        break;
    case PROP_ROI_Y:
        g_value_set_int(value, src->roi_y);
        break;
    case PROP_ROI_W:
        g_value_set_int(value, src->roi_w);
        break;
    case PROP_ROI_H:
        g_value_set_int(value, src->roi_h);
        break;
    case PROP_BRIGHTNESS:
        g_value_set_int(value, src->brightness);
        break;
    case PROP_CONTRAST:
        g_value_set_int(value, src->contrast);
        break;
    case PROP_HUE:
        g_value_set_int(value, src->hue);
        break;
    case PROP_SATURATION:
        g_value_set_int(value, src->saturation);
        break;
    case PROP_SHARPNESS:
        g_value_set_int(value, src->sharpness);
        break;
    case PROP_GAMMA:
        g_value_set_int(value, src->gamma);
        break;
    case PROP_GAIN:
        g_value_set_int(value, src->gain);
        break;
    case PROP_EXPOSURE:
        g_value_set_int(value, src->exposure);
        break;
    case PROP_EXPOSURE_RANGE_MIN:
        g_value_set_int(value, src->exposureRange_min);
        break;
    case PROP_EXPOSURE_RANGE_MAX:
        g_value_set_int(value, src->exposureRange_max);
        break;
    case PROP_AEC_AGC:
        g_value_set_boolean(value, src->aec_agc);
        break;
    case PROP_AEC_AGC_ROI_X:
        g_value_set_int(value, src->aec_agc_roi_x);
        break;
    case PROP_AEC_AGC_ROI_Y:
        g_value_set_int(value, src->aec_agc_roi_y);
        break;
    case PROP_AEC_AGC_ROI_W:
        g_value_set_int(value, src->aec_agc_roi_w);
        break;
    case PROP_AEC_AGC_ROI_H:
        g_value_set_int(value, src->aec_agc_roi_h);
        break;
    case PROP_AEC_AGC_ROI_SIDE:
        g_value_set_enum(value, src->aec_agc_roi_side);
        break;
    case PROP_WHITEBALANCE:
        g_value_set_int(value, src->whitebalance_temperature);
        break;
    case PROP_WHITEBALANCE_AUTO:
        g_value_set_boolean(value, src->whitebalance_temperature_auto);
        break;
    case PROP_LEDSTATUS:
        g_value_set_boolean(value, src->led_status);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

void gst_zedsrc_dispose(GObject *object) {
    GstZedSrc *src;

    g_return_if_fail(GST_IS_ZED_SRC(object));
    src = GST_ZED_SRC(object);

    GST_TRACE_OBJECT(src, "gst_zedsrc_dispose");

    /* clean up as possible.  may be called multiple times */

    G_OBJECT_CLASS(gst_zedsrc_parent_class)->dispose(object);
}

void gst_zedsrc_finalize(GObject *object) {
    GstZedSrc *src;

    g_return_if_fail(GST_IS_ZED_SRC(object));
    src = GST_ZED_SRC(object);

    GST_TRACE_OBJECT(src, "gst_zedsrc_finalize");

    /* clean up object here */
    if (src->caps) {
        gst_caps_unref(src->caps);
        src->caps = NULL;
    }

    G_OBJECT_CLASS(gst_zedsrc_parent_class)->finalize(object);
}

static gboolean gst_zedsrc_calculate_caps(GstZedSrc *src) {
    GST_TRACE_OBJECT(src, "gst_zedsrc_calculate_caps");

    guint32 width, height;
    gint fps;
    GstVideoInfo vinfo;
    GstVideoFormat format = GST_VIDEO_FORMAT_BGRA;

    if (src->stream_type == GST_ZEDSRC_DEPTH_16) {
        format = GST_VIDEO_FORMAT_GRAY16_LE;
    }

    sl::CameraInformation cam_info = src->zed.getCameraInformation();

    width = cam_info.camera_configuration.resolution.width;
    height = cam_info.camera_configuration.resolution.height;

     if (src->stream_type == GST_ZEDSRC_LEFT_RIGHT || src->stream_type == GST_ZEDSRC_LEFT_DEPTH) {
        width *= 2; // Double the width for Side-by-Side
    }

    fps = static_cast<gint>(cam_info.camera_configuration.fps);

    if (format != GST_VIDEO_FORMAT_UNKNOWN) {
        gst_video_info_init(&vinfo);
        gst_video_info_set_format(&vinfo, format, width, height);
        if (src->caps) {
            gst_caps_unref(src->caps);
        }
        src->out_framesize = (guint) GST_VIDEO_INFO_SIZE(&vinfo);
        vinfo.fps_n = fps;
        vinfo.fps_d = 1;
        src->caps = gst_video_info_to_caps(&vinfo);
    }

    gst_base_src_set_blocksize(GST_BASE_SRC(src), src->out_framesize);
    gst_base_src_set_caps(GST_BASE_SRC(src), src->caps);
    GST_DEBUG_OBJECT(src, "Created caps %" GST_PTR_FORMAT, src->caps);

    return TRUE;
}

static gboolean gst_zedsrc_start(GstBaseSrc *bsrc) {
#if (ZED_SDK_MAJOR_VERSION != 5)
    GST_ELEMENT_ERROR(src, LIBRARY, FAILED,
    ("Wrong ZED SDK version. SDK v5.0 EA or newer required "),
                      (NULL));
#endif

    GstZedSrc *src = GST_ZED_SRC(bsrc);
    sl::ERROR_CODE ret;

    GST_TRACE_OBJECT(src, "gst_zedsrc_calculate_caps");

    // ----> Set init parameters
    sl::InitParameters init_params;

    GST_INFO("CAMERA INITIALIZATION PARAMETERS");

    switch(src->camera_resolution) {
        case GST_ZEDSRC_HD2K:
            init_params.camera_resolution = sl::RESOLUTION::HD2K;
            break;
        case GST_ZEDSRC_HD1080:
            init_params.camera_resolution = sl::RESOLUTION::HD1080;
            break;
        case GST_ZEDSRC_HD1200:
            init_params.camera_resolution = sl::RESOLUTION::HD1200;
            break;
        case GST_ZEDSRC_HD720:
            init_params.camera_resolution = sl::RESOLUTION::HD720;
            break;
        case GST_ZEDSRC_SVGA:
            init_params.camera_resolution = sl::RESOLUTION::SVGA;
            break;
        case GST_ZEDSRC_VGA:
            init_params.camera_resolution = sl::RESOLUTION::SVGA;
            break;
        case GST_ZEDSRC_AUTO_RES:
            init_params.camera_resolution = sl::RESOLUTION::AUTO;
            break;
        default:
            GST_ELEMENT_ERROR(src, RESOURCE, NOT_FOUND,
                              ("Failed to set camera resolution"), (NULL));
            return FALSE;
    }
    GST_INFO(" * Camera resolution: %s", sl::toString(init_params.camera_resolution).c_str());
    init_params.camera_fps = src->camera_fps;
    GST_INFO(" * Camera FPS: %d", init_params.camera_fps);
    init_params.sdk_verbose = src->sdk_verbose == TRUE;
    GST_INFO(" * SDK verbose: %s", (init_params.sdk_verbose ? "TRUE" : "FALSE"));
    init_params.camera_image_flip = src->camera_image_flip;
    GST_INFO(" * Camera flipped: %s",
             sl::toString(static_cast<sl::FLIP_MODE>(init_params.camera_image_flip)).c_str());

    init_params.depth_mode = static_cast<sl::DEPTH_MODE>(src->depth_mode);
    if ((src->stream_type == GST_ZEDSRC_LEFT_DEPTH || src->stream_type == GST_ZEDSRC_DEPTH_16) &&
        init_params.depth_mode == sl::DEPTH_MODE::NONE) {
        init_params.depth_mode = sl::DEPTH_MODE::NEURAL;
        src->depth_mode = static_cast<gint>(init_params.depth_mode);
        GST_WARNING_OBJECT(
            src,
            "'stream-type' setting requires depth calculation. Depth mode value forced to NEURAL");
    }
    GST_INFO(" * Depth Mode: %s", sl::toString(init_params.depth_mode).c_str());
    init_params.coordinate_units = sl::UNIT::MILLIMETER;   // ready for 16bit depth image
    GST_INFO(" * Coordinate units: %s", sl::toString(init_params.coordinate_units).c_str());
    init_params.coordinate_system = static_cast<sl::COORDINATE_SYSTEM>(src->coord_sys);
    GST_INFO(" * Coordinate system: %s", sl::toString(init_params.coordinate_system).c_str());
    init_params.depth_minimum_distance = src->depth_min_dist;
    GST_INFO(" * MIN depth: %g", init_params.depth_minimum_distance);
    init_params.depth_maximum_distance = src->depth_max_dist;
    GST_INFO(" * MAX depth: %g", init_params.depth_maximum_distance);
    init_params.depth_stabilization = src->depth_stabilization;
    GST_INFO(" * Depth Stabilization: %d", init_params.depth_stabilization);
    init_params.enable_right_side_measure = false;   // src->enable_right_side_measure==TRUE;
    init_params.camera_disable_self_calib = src->camera_disable_self_calib == TRUE;
    GST_INFO(" * Disable self calibration: %s",
             (init_params.camera_disable_self_calib ? "TRUE" : "FALSE"));

    sl::String opencv_calibration_file(src->opencv_calibration_file.str);
    init_params.optional_opencv_calibration_file = opencv_calibration_file;
    GST_INFO(" * Calibration File: %s ", init_params.optional_opencv_calibration_file.c_str());

    std::cout << "Setting depth_mode to " << init_params.depth_mode << std::endl;

    if (src->svo_file.len != 0) {
        sl::String svo(static_cast<char *>(src->svo_file.str));
        init_params.input.setFromSVOFile(svo);
        init_params.svo_real_time_mode = true;

        GST_INFO(" * Input SVO filename: %s", src->svo_file.str);
    } else if (src->camera_id != DEFAULT_PROP_CAM_ID) {
        init_params.input.setFromCameraID(src->camera_id);

        GST_INFO(" * Input Camera ID: %d", src->camera_id);
    } else if (src->camera_sn != DEFAULT_PROP_CAM_SN) {
        init_params.input.setFromSerialNumber(src->camera_sn);

        GST_INFO(" * Input Camera SN: %ld", src->camera_sn);
    } else if (src->stream_ip.len != 0) {
        sl::String ip(static_cast<char *>(src->stream_ip.str));
        init_params.input.setFromStream(ip, src->stream_port);

        GST_INFO(" * Input Stream: %s:%d", src->stream_ip.str, src->stream_port);
    } else {
        GST_INFO(" * Input from default device");
    }
    // <---- Set init parameters

    // ----> Open camera
    ret = src->zed.open(init_params);

    if (ret > sl::ERROR_CODE::SUCCESS) {
        GST_ELEMENT_ERROR(src, RESOURCE, NOT_FOUND,
                          ("Failed to open camera, '%s'", sl::toString(ret).c_str()), (NULL));
        return FALSE;
    }
    // <---- Open camera

    // ----> Camera Controls
    GST_INFO("CAMERA CONTROLS");
    src->zed.setCameraSettings((sl::VIDEO_SETTINGS::BRIGHTNESS), (src->brightness));
    GST_INFO(" * BRIGHTNESS: %d", src->brightness);
    src->zed.setCameraSettings(sl::VIDEO_SETTINGS::CONTRAST, src->contrast);
    GST_INFO(" * CONTRAST: %d", src->contrast);
    src->zed.setCameraSettings(sl::VIDEO_SETTINGS::HUE, src->hue);
    GST_INFO(" * HUE: %d", src->hue);
    src->zed.setCameraSettings(sl::VIDEO_SETTINGS::SATURATION, src->saturation);
    GST_INFO(" * SATURATION: %d", src->saturation);
    src->zed.setCameraSettings(sl::VIDEO_SETTINGS::SHARPNESS, src->sharpness);
    GST_INFO(" * SHARPNESS: %d", src->sharpness);
    src->zed.setCameraSettings(sl::VIDEO_SETTINGS::GAMMA, src->gamma);
    GST_INFO(" * GAMMA: %d", src->gamma);
    if (src->aec_agc == FALSE) {
        src->zed.setCameraSettings(sl::VIDEO_SETTINGS::AEC_AGC, src->aec_agc);
        GST_INFO(" * AEC_AGC: %s", (src->aec_agc ? "TRUE" : "FALSE"));
        src->zed.setCameraSettings(sl::VIDEO_SETTINGS::EXPOSURE, src->exposure);
        GST_INFO(" * EXPOSURE: %d", src->exposure);
        src->zed.setCameraSettings(sl::VIDEO_SETTINGS::GAIN, src->gain);
        GST_INFO(" * GAIN: %d", src->gain);
    } else {
        src->zed.setCameraSettings(sl::VIDEO_SETTINGS::AEC_AGC, src->aec_agc);
        GST_INFO(" * AEC_AGC: %s", (src->aec_agc ? "TRUE" : "FALSE"));

        if (src->aec_agc_roi_x != -1 && src->aec_agc_roi_y != -1 && src->aec_agc_roi_w != -1 &&
            src->aec_agc_roi_h != -1) {
            sl::Rect roi;
            roi.x = src->aec_agc_roi_x;
            roi.y = src->aec_agc_roi_y;
            roi.width = src->aec_agc_roi_w;
            roi.height = src->aec_agc_roi_h;

            sl::SIDE side = static_cast<sl::SIDE>(src->aec_agc_roi_side);

            GST_INFO(" * AEC_AGC_ROI: (%d,%d)-%dx%d - Side: %d", src->aec_agc_roi_x,
                     src->aec_agc_roi_y, src->aec_agc_roi_w, src->aec_agc_roi_h,
                     src->aec_agc_roi_side);

            src->zed.setCameraSettings(sl::VIDEO_SETTINGS::AEC_AGC_ROI, roi, side);
        }

        src->zed.setCameraSettings(sl::VIDEO_SETTINGS::AUTO_EXPOSURE_TIME_RANGE, src->exposureRange_min, src->exposureRange_max);
        GST_INFO(" * AUTO EXPOSURE TIME RANGE: [%d,%d]", src->exposureRange_min, src->exposureRange_max);
    }
    if (src->whitebalance_temperature_auto == FALSE) {
        src->zed.setCameraSettings(sl::VIDEO_SETTINGS::WHITEBALANCE_AUTO,
                                   src->whitebalance_temperature_auto);
        GST_INFO(" * WHITEBALANCE_AUTO: %s",
                 (src->whitebalance_temperature_auto ? "TRUE" : "FALSE"));
        src->whitebalance_temperature /= 100;
        src->whitebalance_temperature *= 100;
        src->zed.setCameraSettings(sl::VIDEO_SETTINGS::WHITEBALANCE_TEMPERATURE,
                                   src->whitebalance_temperature);
        GST_INFO(" * WHITEBALANCE_TEMPERATURE: %d", src->whitebalance_temperature);

    } else {
        src->zed.setCameraSettings(sl::VIDEO_SETTINGS::WHITEBALANCE_AUTO,
                                   src->whitebalance_temperature_auto);
        GST_INFO(" * WHITEBALANCE_AUTO: %s",
                 (src->whitebalance_temperature_auto ? "TRUE" : "FALSE"));
    }
    src->zed.setCameraSettings(sl::VIDEO_SETTINGS::LED_STATUS, src->led_status);
    GST_INFO(" * LED_STATUS: %s", (src->led_status ? "ON" : "OFF"));
    // <---- Camera Controls

    // ----> Runtime parameters
    GST_TRACE_OBJECT(src, "CAMERA RUNTIME PARAMETERS");

    GST_INFO(" * Depth Confidence threshold: %d", src->confidence_threshold);
    GST_INFO(" * Depth Texture Confidence threshold: %d", src->texture_confidence_threshold);
    GST_INFO(" * 3D Reference Frame: %s",
             sl::toString((sl::COORDINATE_SYSTEM) src->measure3D_reference_frame).c_str());
    GST_INFO(" * Fill Mode: %s", (src->fill_mode ? "TRUE" : "FALSE"));

    if (src->roi) {
        if (src->roi_x != -1 &&
                src->roi_y != -1 &&
                src->roi_w != -1 &&
                src->roi_h != -1) {
            int roi_x_end = src->roi_x + src->roi_w;
            int roi_y_end = src->roi_y + src->roi_h;
            sl::Resolution resolution = sl::getResolution(init_params.camera_resolution);
            if (src->roi_x >= 0 && src->roi_x < resolution.width &&
                    src->roi_y >= 0 && src->roi_y < resolution.height &&
                    roi_x_end <= resolution.width && roi_y_end <= resolution.height) {

                sl::Mat roi_mask(resolution, sl::MAT_TYPE::U8_C1, sl::MEM::CPU);
                roi_mask.setTo<sl::uchar1>(0, sl::MEM::CPU);
                for (unsigned int v = src->roi_y; v < roi_y_end; v++)
                  for (unsigned int u = src->roi_x; u < roi_x_end; u++)
                        roi_mask.setValue<sl::uchar1>(u, v, 255, sl::MEM::CPU);

                GST_INFO(" * ROI mask: (%d,%d)-%dx%d",
                        src->roi_x, src->roi_y, src->roi_w, src->roi_h);

                ret = src->zed.setRegionOfInterest(roi_mask);
                if (ret!=sl::ERROR_CODE::SUCCESS) {
                    GST_ELEMENT_ERROR (src, RESOURCE, NOT_FOUND,
                                    ("Failed to set region of interest, '%s'", sl::toString(ret).c_str() ), (NULL));
                    return FALSE;
                }
            }
        }
    }
    // <---- Runtime parameters

    if (!gst_zedsrc_calculate_caps(src)) {
        return FALSE;
    }

    return TRUE;
}

static gboolean gst_zedsrc_stop(GstBaseSrc *bsrc) {
    GstZedSrc *src = GST_ZED_SRC(bsrc);

    GST_TRACE_OBJECT(src, "gst_zedsrc_stop");

    gst_zedsrc_reset(src);

    return TRUE;
}

static GstCaps *gst_zedsrc_get_caps(GstBaseSrc *bsrc, GstCaps *filter) {
    GstZedSrc *src = GST_ZED_SRC(bsrc);
    GstCaps *caps;

    if (src->caps) {
        caps = gst_caps_copy(src->caps);
    } else {
        caps = gst_pad_get_pad_template_caps(GST_BASE_SRC_PAD(src));
    }

    GST_DEBUG_OBJECT(src, "The caps before filtering are %" GST_PTR_FORMAT, caps);

    if (filter && caps) {
        GstCaps *tmp = gst_caps_intersect(caps, filter);
        gst_caps_unref(caps);
        caps = tmp;
    }

    GST_DEBUG_OBJECT(src, "The caps after filtering are %" GST_PTR_FORMAT, caps);

    return caps;
}

static gboolean gst_zedsrc_set_caps(GstBaseSrc *bsrc, GstCaps *caps) {
    GstZedSrc *src = GST_ZED_SRC(bsrc);
    GST_TRACE_OBJECT(src, "gst_zedsrc_set_caps");

    GstVideoInfo vinfo;

    gst_caps_get_structure(caps, 0);

    GST_DEBUG_OBJECT(src, "The caps being set are %" GST_PTR_FORMAT, caps);

    gst_video_info_from_caps(&vinfo, caps);

    if (GST_VIDEO_INFO_FORMAT(&vinfo) == GST_VIDEO_FORMAT_UNKNOWN) {
        goto unsupported_caps;
    }

    return TRUE;

unsupported_caps:
    GST_ERROR_OBJECT(src, "Unsupported caps: %" GST_PTR_FORMAT, caps);
    return FALSE;
}

static gboolean gst_zedsrc_unlock(GstBaseSrc *bsrc) {
    GstZedSrc *src = GST_ZED_SRC(bsrc);

    GST_TRACE_OBJECT(src, "gst_zedsrc_unlock");

    src->stop_requested = TRUE;

    return TRUE;
}

static gboolean gst_zedsrc_unlock_stop(GstBaseSrc *bsrc) {
    GstZedSrc *src = GST_ZED_SRC(bsrc);

    GST_TRACE_OBJECT(src, "gst_zedsrc_unlock_stop");

    src->stop_requested = FALSE;

    return TRUE;
}

static GstFlowReturn gst_zedsrc_fill(GstPushSrc *psrc, GstBuffer *buf) {
    GstZedSrc *src = GST_ZED_SRC(psrc);

    GST_TRACE_OBJECT(src, "gst_zedsrc_fill");

    sl::ERROR_CODE ret;
    GstMapInfo minfo;
    GstClock *clock;
    GstClockTime clock_time;

    static int temp_ugly_buf_index = 0;

    if (!src->is_started) {
        src->acq_start_time = gst_clock_get_time(gst_element_get_clock(GST_ELEMENT(src)));

        src->is_started = TRUE;
    }


    CUcontext zctx = src->zed.getCUDAContext();

    /// Push zed cuda context as current
    int cu_err = (int)cudaGetLastError();
    if (cu_err>0)
    GST_ELEMENT_ERROR(src, RESOURCE, FAILED,
                      ("Cuda ERROR trigger before ZED SDK : %d",cu_err),
                      (NULL));

    cuCtxPushCurrent_v2(zctx);

    // ----> ZED grab
    ret = src->zed.grab();

    if (ret > sl::ERROR_CODE::SUCCESS) {
        GST_ELEMENT_ERROR(src, RESOURCE, FAILED,
                          ("Grabbing failed with error: '%s' - %s", sl::toString(ret).c_str(),
                           sl::toVerbose(ret).c_str()),
                          (NULL));
        cuCtxPopCurrent_v2(NULL);
        return GST_FLOW_ERROR;
    }
    // <---- ZED grab

    // ----> Clock update
    clock = gst_element_get_clock(GST_ELEMENT(src));
    clock_time = gst_clock_get_time(clock);
    gst_object_unref(clock);
    // <---- Clock update

    // Memory mapping
    if (FALSE == gst_buffer_map(buf, &minfo, GST_MAP_WRITE)) {
        GST_ELEMENT_ERROR(src, RESOURCE, FAILED, ("Failed to map buffer for writing"), (NULL));
        cuCtxPopCurrent_v2(NULL);
        return GST_FLOW_ERROR;
    }

    // ZED Mats
    sl::Mat left_img;
    sl::Mat right_img;
    sl::Mat depth_data;

    // ----> Mats retrieving
    auto check_ret = [src](sl::ERROR_CODE ret) {
        if (ret != sl::ERROR_CODE::SUCCESS) {
            GST_ELEMENT_ERROR(src, RESOURCE, FAILED,
                            ("Grabbing failed with error: '%s' - %s", sl::toString(ret).c_str(),
                            sl::toVerbose(ret).c_str()),
                            (NULL));
            return false;
        }
        return true;
    };

    if (src->stream_type == GST_ZEDSRC_ONLY_LEFT) {
        ret = src->zed.retrieveImage(left_img, sl::VIEW::LEFT, sl::MEM::CPU);
        if(!check_ret(ret)) return GST_FLOW_ERROR;
    } else if (src->stream_type == GST_ZEDSRC_ONLY_RIGHT) {
        ret = src->zed.retrieveImage(left_img, sl::VIEW::RIGHT, sl::MEM::CPU);
        if(!check_ret(ret)) return GST_FLOW_ERROR;
    } else if (src->stream_type == GST_ZEDSRC_LEFT_RIGHT) {
        ret = src->zed.retrieveImage(left_img, sl::VIEW::SIDE_BY_SIDE, sl::MEM::CPU);
        if(!check_ret(ret)) return GST_FLOW_ERROR;
    } else if (src->stream_type == GST_ZEDSRC_DEPTH_16) {
        ret = src->zed.retrieveMeasure(depth_data, sl::MEASURE::DEPTH_U16_MM, sl::MEM::CPU);
        if(!check_ret(ret)) return GST_FLOW_ERROR;
    } else if (src->stream_type == GST_ZEDSRC_LEFT_DEPTH) {
        ret = src->zed.retrieveImage(left_img, sl::VIEW::LEFT, sl::MEM::CPU);
        if(!check_ret(ret)) return GST_FLOW_ERROR;
        ret = src->zed.retrieveMeasure(depth_data, sl::MEASURE::DEPTH, sl::MEM::CPU);
        if(!check_ret(ret)) return GST_FLOW_ERROR;
    }
    // <---- Mats retrieving

    // ----> Memory copy
    if (src->stream_type == GST_ZEDSRC_DEPTH_16) {
        memcpy(minfo.data, depth_data.getPtr<sl::ushort1>(), minfo.size);
    } else if (src->stream_type == GST_ZEDSRC_LEFT_DEPTH) {
        // TODO: Implement left depth copy
        return GST_FLOW_ERROR;
    } else {
        memcpy(minfo.data, left_img.getPtr<sl::uchar4>(), minfo.size);
    }
    cuCtxPopCurrent_v2(NULL);
    // <---- Memory copy


    // ----> Timestamp meta-data
    GST_BUFFER_TIMESTAMP(buf) =
        GST_CLOCK_DIFF(gst_element_get_base_time(GST_ELEMENT(src)), clock_time);
    GST_BUFFER_DTS(buf) = GST_BUFFER_TIMESTAMP(buf);
    GST_BUFFER_OFFSET(buf) = temp_ugly_buf_index++;
    // <---- Timestamp meta-data

    // Buffer release
    gst_buffer_unmap(buf, &minfo);

    if (src->stop_requested) {
        return GST_FLOW_FLUSHING;
    }

    return GST_FLOW_OK;
}

static gboolean plugin_init(GstPlugin *plugin) {
    GST_DEBUG_CATEGORY_INIT(gst_zedsrc_debug, "zedsrc", 0, "debug category for zedsrc element");
    gst_element_register(plugin, "zedsrc", GST_RANK_NONE, gst_zedsrc_get_type());

    return TRUE;
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR, GST_VERSION_MINOR, zedsrc, "Zed camera source", plugin_init,
                  GST_PACKAGE_VERSION, GST_PACKAGE_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
