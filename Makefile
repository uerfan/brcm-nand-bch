main:
	gcc brcm-nand-bch.c bch.c bch.h -o main
	gcc ecc-bypass.c bch.c bch.h -o bypass

clean:
	rm main bypass
 