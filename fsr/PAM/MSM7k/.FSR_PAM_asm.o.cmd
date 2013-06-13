cmd_drivers/fsr/PAM/MSM7k/FSR_PAM_asm.o := ../toolchain/arm-eabi-4.4.0/bin/arm-eabi-gcc -Wp,-MD,drivers/fsr/PAM/MSM7k/.FSR_PAM_asm.o.d  -nostdinc -isystem /home/se-part/Opensource/Europa_TIM/eclair/GT-I5500_OpenSource/GT-I5500_OpenSource_Kernel/toolchain/arm-eabi-4.4.0/bin/../lib/gcc/arm-eabi/4.4.0/include -Iinclude  -I/home/se-part/Opensource/Europa_TIM/eclair/GT-I5500_OpenSource/GT-I5500_OpenSource_Kernel/kernel/arch/arm/include -include include/linux/autoconf.h -D__KERNEL__ -mlittle-endian -Iarch/arm/mach-msm/include -D__ASSEMBLY__ -mabi=aapcs-linux -mno-thumb-interwork -D__LINUX_ARM_ARCH__=6 -march=armv6k -mtune=arm1136j-s -msoft-float    -DMODULE -c -o drivers/fsr/PAM/MSM7k/FSR_PAM_asm.o drivers/fsr/PAM/MSM7k/FSR_PAM_asm.S

deps_drivers/fsr/PAM/MSM7k/FSR_PAM_asm.o := \
  drivers/fsr/PAM/MSM7k/FSR_PAM_asm.S \

drivers/fsr/PAM/MSM7k/FSR_PAM_asm.o: $(deps_drivers/fsr/PAM/MSM7k/FSR_PAM_asm.o)

$(deps_drivers/fsr/PAM/MSM7k/FSR_PAM_asm.o):
