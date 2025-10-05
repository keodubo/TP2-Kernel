
all:  bootloader kernel userland image

bootloader:
	cd Bootloader; make all

kernel:
	cd Kernel; make all

userland:
	cd Userland; make all

image: kernel bootloader userland
	cd Image; make all

clean:
	cd Bootloader; make clean
	cd Image; make clean
	cd Kernel; make clean
	cd Userland; make clean

docker:
	@echo "Starting Docker container for kernel development (x86_64 emulation)..."
	@echo "Working directory: $(shell pwd)"
	docker run --platform linux/amd64 --add-host=host.docker.internal:host-gateway -v $${PWD}:/root -w /root --security-opt seccomp:unconfined -ti agodio/itba-so-multi-platform:3.0

.PHONY: bootloader image collections kernel userland all clean docker
