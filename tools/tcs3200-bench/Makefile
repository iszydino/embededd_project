TARGET  = tcs_test
CC      = arm-none-eabi-gcc
CFLAGS  = -mcpu=cortex-m3 -mthumb -Os -g -Wall -Wextra -ffreestanding \
          -ffunction-sections -fdata-sections
LDFLAGS = -T linker.ld -nostdlib -nostartfiles -Wl,--gc-sections \
          -Wl,-Map=build/$(TARGET).map
SRCS    = main.c startup.c
ENV    ?= BENCH
CFLAGS += -DENV_$(ENV)

.RECIPEPREFIX = >

all: build/$(TARGET).elf

build/$(TARGET).elf: $(SRCS) regs.h color.h linker.ld
> mkdir -p build
> $(CC) $(CFLAGS) $(SRCS) $(LDFLAGS) -lgcc -o $@
> arm-none-eabi-size $@

flash: all
> openocd -f interface/stlink.cfg -f target/stm32f1x.cfg -c "program build/$(TARGET).elf verify reset exit"

clean:
> rm -rf build

.PHONY: all flash clean
