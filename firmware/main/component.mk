#
# GNU Make SDK only globs .c/.cpp/.cc. Compile .cxx via COMPONENT_OBJS
# and extra pattern rules.
#

COMPONENT_ADD_INCLUDEDIRS := . ../include
COMPONENT_PRIV_INCLUDEDIRS :=
COMPONENT_EMBED_FILES := index.html calibrate.html
COMPONENT_EXTRA_INCLUDES := $(BUILD_DIR_BASE)/include $(BUILD_DIR_BASE) \
	$(COMPONENT_PATH)/../include

COMPONENT_OBJS := app_main.o RoboHero.o Mqtt.o Store.o Servo.o \
	Shell.o Web.o Pca9685.o Motions.o Discovery.o

%.o: $(COMPONENT_PATH)/%.cxx $(COMMON_MAKEFILES) $(COMPONENT_MAKEFILE)
	$(summary) CXX $(patsubst $(PWD)/%,%,$(CURDIR))/$@
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(addprefix -I ,$(COMPONENT_INCLUDES)) \
		$(addprefix -I ,$(COMPONENT_EXTRA_INCLUDES)) -I . \
		-c $(abspath $<) -o $@
