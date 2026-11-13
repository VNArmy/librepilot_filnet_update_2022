# osg and osgearth emit a lot of unused parameter warnings...
QMAKE_CXXFLAGS += -Wno-unused-parameter

# set debug suffix if needed
win32:CONFIG(debug, debug|release):DS = "d"

contains(QT_ARCH, x86_64)  {
    macx {
        LIB_DIR_NAME = lib
    } else {
        LIB_DIR_NAME = lib64
    }
} else {
    LIB_DIR_NAME = lib
}

gstreamer {
    include(../gstreamer/gstreamer.pri)
    include(../gstreamer/gstreamer_dependencies.pri)
}

osg {
    OSG_SDK_DIR = $$clean_path($$(OSG_SDK_DIR))
    message(Using osg from here: $$OSG_SDK_DIR)

    linux|macx {
        INCLUDEPATH += $$OSG_SDK_DIR/include
        LIBS += -L$$OSG_SDK_DIR/$$LIB_DIR_NAME
        LIBS += -lOpenThreads -losg -losgUtil -losgDB -losgGA -losgFX -losgViewer -losgText
        osgQt:LIBS += -losgQt
    }

    win32 {
        LIBS += -lOpenThreads$${DS} -losg$${DS} -losgUtil$${DS} -losgDB$${DS} -losgGA$${DS} -losgFX$${DS} -losgViewer$${DS} -losgText$${DS}
        osgQt:LIBS += -losgQt$${DS}
    }
}

osgearth {
    OSGEARTH_SDK_DIR = $$clean_path($$(OSGEARTH_SDK_DIR))
    message(Using osgearth from here: $$OSGEARTH_SDK_DIR)

    # osgEarth 3 merged osgEarthUtil (and its other helper libraries) into
    # libosgEarth, and its headers need C++17. MSYS2 only ships osgEarth 3.
    win32 {
        CONFIG += osgearth3
    } else {
        !exists($$OSGEARTH_SDK_DIR/include/osgEarthUtil):!exists(/usr/include/osgEarthUtil):CONFIG += osgearth3
    }
    osgearth3 {
        message(Using the osgEarth 3 API)
        CONFIG += c++17
    }

    linux|macx {
        INCLUDEPATH += $$OSGEARTH_SDK_DIR/include
        LIBS += -L$$OSGEARTH_SDK_DIR/$$LIB_DIR_NAME
        LIBS += -losgEarth
        !osgearth3:LIBS += -losgEarthUtil
        osgearthQt:LIBS +=  -losgEarthQt
    }

    win32 {
        LIBS += -losgEarth$${DS}
        !osgearth3:LIBS += -losgEarthUtil$${DS}
        osgearthQt:LIBS += -losgEarthQt$${DS}
    }
}
