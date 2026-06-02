DRIVER_NAME=kernel_ipc_driver
USERSPACE_BIN_NAME=userspace
KERNEL_VERSION=$(shell uname -r)

.PHONY: all install clean 

obj-m += src/$(DRIVER_NAME).o
all: $(DRIVER_NAME).ko $(USERSPACE_BIN_NAME)

# we shouldn't really have `sudo` in the makefile but `sudo make` changes cwd so leave it
install: $(DRIVER_NAME).ko
	sudo insmod src/$(DRIVER_NAME).ko

# clean uninstalls the module with rmmod
clean:
	rm -rf $(USERSPACE_BIN_NAME)
	make -C /lib/modules/$(KERNEL_VERSION)/build M=$(PWD) clean
	sudo rmmod -f $(DRIVER_NAME)

# note: the standard way that this builds is just to dump all the build artifacts into src/ 
# instead of having a build folder. this is terrible but solving it is more complicated than expected
$(DRIVER_NAME).ko:
	@echo "Creating $(DRIVER_NAME)"
	make -C /lib/modules/$(KERNEL_VERSION)/build M=$(PWD) modules

# note: @ prevents `make` from logging the command
$(USERSPACE_BIN_NAME): 
	@echo "Creating $(USERSPACE_BIN_NAME)"
	gcc -pthread src/$(USERSPACE_BIN_NAME).c -o $(USERSPACE_BIN_NAME)