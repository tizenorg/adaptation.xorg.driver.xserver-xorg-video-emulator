TOP_DIR = .

include $(TOP_DIR)/Make.rules

SUBDIRS = src

$(EVERYTHING)::
	@for n in $(SUBDIRS); do $(MAKE) -C $$n $@ || exit 1; done
