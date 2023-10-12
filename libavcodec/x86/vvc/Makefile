clean::
	$(RM) $(CLEANSUFFIXES:%=libavcodec/x86/vvc/%)

OBJS-$(CONFIG_VVC_DECODER)             += x86/vvc/vvcdsp_init.o
X86ASM-OBJS-$(CONFIG_VVC_DECODER)      += x86/vvc/vvc_alf.o                 \
                                          x86/vvc/vvc_inter.o				\
