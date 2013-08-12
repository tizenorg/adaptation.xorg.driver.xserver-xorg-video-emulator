TOP_DIR = ../..
OBJ_PREFIX = /src/uxa
TARGET = $(LIB_DIR)/libuxa.a
TARGETS = $(TARGET)

CFLAGS += -DHAVE_XEXTPROTO_71

include $(TOP_DIR)/Make.rules

$(TARGET): $(OBJ_DIR) $(DEPS) $(OBJS) $(LIB_DIR)
ifeq ($(V),0)
	@echo [AR] $@
	@$(AR) crs $@ $(OBJS)
else
	$(AR) crs $@ $(OBJS)
endif
