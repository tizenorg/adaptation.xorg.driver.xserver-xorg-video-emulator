TOP_DIR = ..
OBJ_PREFIX = /src
TARGET = $(LIB_DIR)/vigs_drv.so
TARGETS = $(TARGET)

.PHONY:: uxa

include $(TOP_DIR)/Make.rules

$(TARGET): $(OBJ_DIR) $(DEPS) $(OBJS) $(LIB_DIR) uxa
ifeq ($(V),0)
	@echo [LD] $@
	@$(CC) $(LDFLAGS) -pthread -shared -o $@ $(OBJS) -lpthread -ldrm_vigs -luxa
else
	$(CC) $(LDFLAGS) -pthread -shared -o $@ $(OBJS) -lpthread -ldrm_vigs -luxa
endif

uxa:
	@make -C $(TOP_DIR)/src/uxa

clean::
	@make -C $(TOP_DIR)/src/uxa clean

install:: $(INSTALL_DIR) $(TARGET)
ifeq ($(V),0)
	@echo [INSTALL] $(TARGET)
	@cp -d $(TARGET) $(INSTALL_DIR)
else
	cp -d $(TARGET) $(INSTALL_DIR)
endif

install-strip:: install
ifeq ($(V),0)
	@echo [STRIP] $(TARGET)
	@$(STRIP) $(INSTALL_DIR)/vigs_drv.so
else
	$(STRIP) $(INSTALL_DIR)/vigs_drv.so
endif
