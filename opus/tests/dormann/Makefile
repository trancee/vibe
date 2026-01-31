# Download the AS65 assembler from here:
# https://www.kingswood-consulting.co.uk/assemblers/

AS = as65
ASFLAGS = -v -l -m -n -w -h0

all: 6502_functional_test 6502_decimal_test 6502_interrupt_test 65C02_extended_opcodes_test
 
6502_functional_test:
	$(AS) $(ASFLAGS) $@.a65

6502_interrupt_test:
	$(AS) $(ASFLAGS) $@.a65

6502_decimal_test:
	$(AS) $(ASFLAGS) $@.a65

65C02_extended_opcodes_test:
	$(AS) $(ASFLAGS) -x $@.a65c

clean:
	rm -f *.bin *.lst
