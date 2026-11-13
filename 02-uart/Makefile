# Build 02-uart without STM32CubeIDE.
#
# The ARM toolchain ships inside CubeIDE on macOS, so this looks for it there
# unless arm-none-eabi-gcc is already on PATH. Override with TOOLCHAIN if yours
# lives somewhere else, for example:
#
#   make TOOLCHAIN=/opt/homebrew/bin/

TARGET = 02-uart
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
CFLAGS = $(ARCH) -std=c11 -Og -g3 -Wall -Wextra -ffunction-sections -fdata-sections
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

# Flash over ST-LINK. The board exposes its virtual COM port at the same time,
# so a serial monitor can stay open on another terminal.
flash: $(BUILD)/$(TARGET).bin
	st-flash --reset write $< 0x08000000

# The BRR helper is pure arithmetic, so it runs on the host with no board.
test: | $(BUILD)
	cc -std=c11 -Wall -Wextra -o $(BUILD)/test_brr test/test_brr.c
	$(BUILD)/test_brr

clean:
	rm -rf $(BUILD)

.PHONY: all flash test clean
