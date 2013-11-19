all:
	@cd ./src; make;
clean:
	@rm proxy; cd ./src; make clean;