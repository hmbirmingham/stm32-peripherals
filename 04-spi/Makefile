# Build 04-spi without STM32CubeIDE.
#
# Override TOOLCHAIN if your arm-none-eabi-gcc lives somewhere else:
#   make TOOLCHAIN=/opt/homebrew/bin/

TARGET = 04-spi
BUILD  = build

CUBE_TOOLS := $(firstword $(wildcard /Applications/STM32CubeIDE.app/Contents/Eclipse/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.*/tools/bin))
TOOLCHAIN  ?= $(if $(shell command -v arm-none-eabi-gcc),,$(CUBE_TOOLS)/)

CC      = $(TOOLCHAIN)arm-none-eabi-gcc
OBJCOPY = $(TOOLCHAIN)arm-none-eabi-objcopy
SIZE    = $(TOOLCHAIN)arm-none-eabi-size

LDSCRIPT = STM32F411RETX_FLASH.ld

C_SRCS = Src/main.c Src/syscalls.c Src/sysmem.c
S_SRCS = Startup/startup_stm32f411retx.s
OBJS   = $(addprefix $(BUILD)/,$(notdir $(C_SRCS:.c=.o)) $(notdir $(S_SRCS:.s=.o)))

ARCH   = -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
CFLAGS = $(ARCH) -std=c11 -Og -g3 -Wall -Wextra -ffunction-sections -fdata-sections -IInc
LFLAGS = $(ARCH) -T $(LDSCRIPT) -Wl,--gc-sections \
         -Wl,-Map=$(BUILD)/$(TARGET).map \
         --specs=nano.specs --specs=nosys.specs

all: $(BUILD)/$(TARGET).bin

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: Src/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: Startup/%.s | $(BUILD)
	$(CC) $(ARCH) -c $< -o $@

$(BUILD)/$(TARGET).elf: $(OBJS)
	$(CC) $(OBJS) $(LFLAGS) -o $@
	$(SIZE) $@

$(BUILD)/$(TARGET).bin: $(BUILD)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@

flash: $(BUILD)/$(TARGET).bin
	st-flash --reset write $< 0x08000000

# BME280 compensation math is pure arithmetic — verify on the host.
test: | $(BUILD)
	cc -std=c11 -Wall -Wextra -IInc -o $(BUILD)/test_bme280 test/test_bme280.c
	$(BUILD)/test_bme280

clean:
	rm -rf $(BUILD)

.PHONY: all flash test clean
