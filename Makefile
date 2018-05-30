main:
	gcc brcm-nand-bch.c bch.c bch.h -o main

clean:
	rm main b.bin
