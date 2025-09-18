# Build device tree overlay for W5500 Ethernet

DTC := dtc
SRC := w5500-ethernet-overlay.dts
DTBO := w5500-ethernet.dtbo

all: $(DTBO)

$(DTBO): $(SRC)
	$(DTC) -@ -I dts -O dtb -o $@ $<

clean:
	rm -f $(DTBO)
