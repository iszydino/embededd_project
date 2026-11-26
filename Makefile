TARGET = fw
CC     = arm-none-eabi-gcc
FR     = middlewares/FreeRTOS
MAIN  ?= main.c

INC = -Ibsp -Ihal/inc -Iapp/inc \
      -I$(FR)/include -I$(FR)/portable/GCC/ARM_CM3 \
      -Imiddlewares/CMSIS/Include -Imiddlewares/CMSIS/Device

CFLAGS = -mcpu=cortex-m3 -mthumb -Os -g -Wall -Wextra -DSTM32F103xB \
	 -ffunction-sections -fdata-sections $(INC)
LDFLAGS = -mcpu=cortex-m3 -mthumb -T bsp/stm32f103c8.ld \
	  -Wl,--gc-sections -specs=nano.specs -specs=nosys.specs \
	  -Wl,-Map=build/$(TARGET).map

SRCS = $(wildcard hal/src/*.c) $(wildcard app/src/*.c) \
       $(wildcard $(FR)/*.c) $(FR)/portable/GCC/ARM_CM3/port.c \
       $(FR)/portable/MemMang/heap_4.c bsp/system_stm32f1xx.c
SRCS += $(MAIN)
ASRC = bsp/startup_stm32f103xb.s


# --- ST HAL + SSD1306 + bsp ---
HALD = middlewares/STM32F1xx_HAL
SSD  = middlewares/SSD1306
INC    += -I$(HALD)/Inc -I$(SSD)
CFLAGS += -DUSE_HAL_DRIVER -MMD -MP
SRCS   += $(wildcard $(HALD)/Src/*.c) $(wildcard $(SSD)/*.c) bsp/bsp_hal.c bsp/syscalls.c
build/middlewares/%.o: CFLAGS += -w

OBJS = $(patsubst %.c,build/%.o,$(SRCS)) $(patsubst %.s,build/%.o,$(ASRC))

all: build/$(TARGET).elf

build/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@
build/%.o: %.s
	@mkdir -p $(dir $@)
	$(CC) -mcpu=cortex-m3 -mthumb -c $< -o $@
build/$(TARGET).elf: $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@
	arm-none-eabi-size $@

flash: all
	openocd -f interface/stlink.cfg -f target/stm32f1x.cfg -c "program build/$(TARGET).elf verify reset exit"
clean:
	rm -rf build
.PHONY: all flash clean

ENV ?= BENCH
CFLAGS += -DENV_$(ENV)

-include $(OBJS:.o=.d)

# WEIGHT=1 khi da co task_weight.c. Doi gia tri phai make clean
WEIGHT ?= 0
CFLAGS += -DAPP_USE_WEIGHT=$(WEIGHT)

# 1 = kenh A gain 128 (loadcell o A+/A-), 2 = kenh B gain 32
HX711_GAIN ?= 1
CFLAGS += -DHX711_GAIN_PULSES=$(HX711_GAIN)U
